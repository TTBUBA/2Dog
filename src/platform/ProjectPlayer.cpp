#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#include <twodog/core/Utf8Path.hpp>
#include <twodog/gameplay/Input.hpp>
#include <twodog/platform/Audio.hpp>
#include <twodog/platform/GameUI.hpp>
#include <twodog/platform/ProjectPlayer.hpp>
#include <twodog/platform/SceneView.hpp>
#include <twodog/scripting/ManagedHost.hpp>
#include <utility>

namespace twodog {
struct ProjectPlayer::Impl {
    Window &window;
    Project project;
    SpriteRenderer renderer;
    VisualAssets assets;
    Audio audio;
    GameUI ui;
    int viewportWidth = 0, viewportHeight = 0;
    bool embedded = false;
    FramebufferSize viewport() const {
        return viewportWidth > 0 ? FramebufferSize{viewportWidth, viewportHeight} : window.framebufferSize();
    }
    std::unique_ptr<World2D> world;
    std::unique_ptr<SceneView> view;
    Json baseline;
    std::string sceneAsset;
    std::filesystem::path assembly;
    bool playing = true, paused = false, singleStep = false, inFixed = false, dirty = false, shuttingDown = false;
    std::set<std::string, std::less<>> destroyed;
    Json::Array messages;
    std::string requestedScene;
    InputSnapshot fixedInput;
    double pollAt = 0;
    std::filesystem::file_time_type assemblyStamp{}, pendingStamp{};
    bool pendingReload = false;
    std::uint64_t fixedCount = 0;
    std::string status = "READY";
    std::unique_ptr<ManagedHost> scripts;
    // Destroy before world, assets and native callback context.
    Impl(Window &w, const std::filesystem::path &root, const std::filesystem::path &managed, bool enabled,
         const Json *initialScene, bool inEditor, const std::filesystem::path &gameAssembly)
        : window(w), project(root), renderer(w), assets(project, renderer),
          sceneAsset(project.settings().at("startScene").string()),
          assembly(gameAssembly.empty() ? std::filesystem::absolute(managed) / "game" / "GameScripts.dll"
                                        : gameAssembly) {
        if (gameAssembly.empty() && std::filesystem::exists(project.root() / ".twodog" / "runtime" / "GameScripts.dll"))
            assembly = project.root() / ".twodog" / "runtime" / "GameScripts.dll";
        embedded = inEditor;
        baseline = initialScene ? *initialScene : project.scene(sceneAsset);
        static_cast<void>(GameUI::layout(baseline, 1280, 720));
        world = std::make_unique<World2D>(project, baseline);
        view = std::make_unique<SceneView>(project, assets, baseline);
        bindInputs();
        if (enabled) {
            scripts = std::make_unique<ManagedHost>(
                managed, [this](ScriptApi op, const Json &request) { return dispatch(op, request); });
            scripts->prepare(assembly, world->bindings());
            scripts->commit();
            assemblyStamp = std::filesystem::last_write_time(assembly);
        }
        if (!embedded)
            window.setTitle("2Dog 0.6 | C# project | F5 Play/Stop, P Pause, N Step, F9 Reload");
        std::cout << "[2Dog] Project loaded: " << project.root() << " | C#=" << (enabled ? "on" : "off") << '\n';
    }
    ~Impl() {
        shuttingDown = true;
        if (scripts) {
            try {
                scripts->stop();
            } catch (const std::exception &e) {
                std::cerr << e.what() << '\n';
            }
        }
    }
    void bindOne(const Json &data) {
        std::vector<InputBinding> bindings;
        for (const auto &b : data.at("bindings").array()) {
            const auto device = b.at("device").string();
            InputDevice d;
            if (device == "keyboard")
                d = InputDevice::Keyboard;
            else if (device == "mouse")
                d = InputDevice::Mouse;
            else if (device == "gamepad")
                d = InputDevice::Gamepad;
            else
                throw std::invalid_argument("Unknown input device");
            bindings.push_back({d, b.at("code").integer()});
        }
        const auto action = data.at("action").string();
        if ((action.starts_with("engine.") || action.starts_with("ui.")))
            throw std::invalid_argument("engine.* and ui.* actions are reserved");
        window.input().rebind(action, std::move(bindings));
    }
    void bindInputs() {
        auto &input = window.input();
        input = Input{};
        input.bind("ui.pointer", {{InputDevice::Mouse, 0}});
        input.bind("ui.tab", {{InputDevice::Keyboard, 258}});
        input.bind("ui.submit", {{InputDevice::Keyboard, 257}, {InputDevice::Keyboard, 32}});
        input.bind("ui.left", {{InputDevice::Keyboard, 263}});
        input.bind("ui.right", {{InputDevice::Keyboard, 262}});
        input.bind("engine.play", {{InputDevice::Keyboard, 294}});
        input.bind("engine.scene", {{InputDevice::Keyboard, 295}});
        input.bind("engine.save", {{InputDevice::Keyboard, 296}});
        input.bind("engine.saved", {{InputDevice::Keyboard, 297}});
        input.bind("engine.reload", {{InputDevice::Keyboard, 298}});
        input.bind("engine.pause", {{InputDevice::Keyboard, 80}});
        input.bind("engine.step", {{InputDevice::Keyboard, 78}});
        for (const auto &action : project.settings().at("input").array())
            bindOne(action);
    }
    Json bindingSnapshot() const {
        auto result = world->bindings();
        std::erase_if(result.array(), [this](const Json &b) { return destroyed.contains(b.at("entity").string()); });
        return result;
    }
    void queuesClear() {
        destroyed.clear();
        messages.clear();
        requestedScene.clear();
        dirty = false;
        fixedInput.clear();
    }
    void replace(Json document, bool run) {
        static_cast<void>(GameUI::layout(document, 1280, 720));
        // Prepare everything that can fail without running user lifecycle code.
        auto candidate = std::make_unique<World2D>(project, document);
        auto visual = std::make_unique<SceneView>(project, assets, document);
        if (scripts && run)
            scripts->prepare(assembly, candidate->bindings());
        if (scripts) {
            if (run)
                scripts->detach();
            else
                scripts->stop();
        }
        audio.reset();
        ui.reset();
        queuesClear();
        world = std::move(candidate);
        view = std::move(visual);
        baseline = std::move(document);
        bindInputs();
        playing = run;
        paused = false;
        singleStep = false;
        fixedCount = 0;
        const auto size = viewport();
        view->camera.width = std::max(1, size.width);
        view->camera.height = std::max(1, size.height);
        if (scripts && run)
            scripts->commit();
        status = run ? "PLAY" : "STOP - SCENE RESTORED";
    }
    void reloadScripts() {
        if (!scripts || !playing)
            return;
        scripts->prepare(assembly, bindingSnapshot());
        scripts->commit();
        assemblyStamp = std::filesystem::last_write_time(assembly);
        pendingReload = false;
        status = "C# RELOADED";
    }
    void save(const std::string &name) {
        if (!destroyed.empty())
            throw std::runtime_error("Save after the current destruction batch has completed");
        const auto path = containedPath(project.root(), pathFromUtf8(name));
        if (path.extension() != ".json")
            throw std::invalid_argument("Scene output must have .json extension");
        // A script cannot overwrite the catalog, prefab or source scene by mistake.
        if (path == project.root() / "project.json")
            throw std::invalid_argument("Cannot overwrite project.json");
        for (const auto &[id, asset] : project.assets()) {
            static_cast<void>(id);
            if (asset.path == path)
                throw std::invalid_argument("Save to a new path; registered assets are protected");
        }
        auto snapshot = world->snapshot();
        snapshot["camera"] = view->cameraDocument();
        snapshot.save(path);
        status = "SCENE SAVED";
        std::cout << "[2Dog][scene] Saved " << path << '\n';
    }
    void flush() {
        const auto remove = destroyed;
        if (dirty && scripts && playing) {
            dirty = false;
            scripts->sync(bindingSnapshot());
        }
        for (const auto &id : remove) {
            destroyed.erase(id);
            if (world->alive(id)) {
                // UI children survive destruction of a container as root elements.
                for (const auto &[child, e] : world->entities())
                    if (e.document.contains("ui") && e.document.at("ui").text("parent") == id)
                        world->get(child).document["ui"]["parent"] = "";
                world->destroy(id);
            }
        }
        if (!requestedScene.empty()) {
            const auto id = std::exchange(requestedScene, {});
            try {
                replace(project.scene(id), playing);
                sceneAsset = id;
            } catch (const std::exception &e) {
                error(e);
            }
        }
        // Messages emitted from callbacks are delivered at the next boundary, never recursively.
        auto deliver = std::exchange(messages, {});
        if (scripts && playing)
            for (const auto &message : deliver)
                if (message.text("entity").empty() || world->alive(message.at("entity").string()))
                    scripts->message(message);
    }
    void error(const std::exception &e) {
        status = "ERROR - SEE CONSOLE";
        std::cerr << "[2Dog][project] " << e.what() << '\n';
    }
    void poll(bool shortcuts, bool acceptInput) {
        auto &input = window.input();
        input.setBlocked(false, false, false);
        if (!acceptInput) {
            input.releaseAll();
            static_cast<void>(input.consumeFixed());
        }
        const auto size = viewport();
        view->camera.width = std::max(1, size.width);
        view->camera.height = std::max(1, size.height);
        if (acceptInput)
            view->camera.zoom =
                std::clamp(view->camera.zoom * std::pow(1.1F, static_cast<float>(input.scrollY)), .25F, 4.0F);
        if (shortcuts)
            try {
                if (input.action("engine.play").pressed)
                    replace(baseline, !playing);
                if (input.action("engine.scene").pressed)
                    replace(project.scene(sceneAsset), playing);
                if (input.action("engine.save").pressed)
                    save("saved.scene.json");
                if (input.action("engine.saved").pressed)
                    replace(Json::read(project.root() / "saved.scene.json"), playing);
                if (input.action("engine.reload").pressed)
                    reloadScripts();
                if (input.action("engine.pause").pressed && playing) {
                    paused = !paused;
                    audio.pause(paused);
                }
                if (input.action("engine.step").pressed && playing && paused)
                    singleStep = true;
            } catch (const std::exception &e) {
                error(e);
            }
        if (singleStep) {
            singleStep = false;
            fixed(1.0 / 60.0, true);
        }
        ui.poll(
            *world, window, size.width, size.height,
            [this](Json message) {
                if (messages.size() < 1024)
                    messages.push_back(std::move(message));
            },
            acceptInput && playing && !paused);
        input.setBlocked(!acceptInput || ui.capturesKeyboard(), !acceptInput || ui.capturesMouse(), !acceptInput);
        const auto now = window.nowSeconds();
        if (now >= pollAt) {
            pollAt = now + .5;
            assets.poll();
            audio.collect();
            if (scripts && playing)
                try {
                    const auto stamp = std::filesystem::last_write_time(assembly);
                    if (stamp != assemblyStamp) {
                        if (pendingReload && stamp == pendingStamp) {
                            assemblyStamp = stamp;
                            reloadScripts();
                        } else {
                            pendingStamp = stamp;
                            pendingReload = true;
                        }
                    }
                } catch (const std::exception &e) {
                    pendingReload = false;
                    error(e);
                }
        }
    }
    void fixed(double dt, bool forced = false) {
        fixedInput = window.input().consumeFixed();
        if (!playing || (paused && !forced))
            return;
        flush();
        inFixed = true;
        try {
            if (scripts)
                scripts->fixedUpdate(dt);
        } catch (...) {
            inFixed = false;
            throw;
        }
        inFixed = false;
        flush();
        world->physics.step(static_cast<float>(dt));
        ++fixedCount;
        for (const auto &event : world->physics.events()) {
            std::string topic;
            switch (event.kind) {
            case PhysicsEvent::Kind::ContactBegin:
                topic = "collision.begin";
                break;
            case PhysicsEvent::Kind::ContactEnd:
                topic = "collision.end";
                break;
            case PhysicsEvent::Kind::TriggerBegin:
                topic = "trigger.begin";
                break;
            case PhysicsEvent::Kind::TriggerEnd:
                topic = "trigger.end";
                break;
            }
            const auto a = world->entityFor(event.a), b = world->entityFor(event.b);
            if (messages.size() < 1022 && !a.empty() && !b.empty()) {
                messages.push_back(
                    Json::Object{{"entity", a}, {"topic", topic}, {"payload", Json::Object{{"other", b}}}});
                messages.push_back(
                    Json::Object{{"entity", b}, {"topic", topic}, {"payload", Json::Object{{"other", a}}}});
            }
        }
        view->tick(*world, dt);
        flush();
        if (forced)
            std::cout << "[2Dog][step] fixed=" << fixedCount << '\n';
    }
    void update(double dt) {
        if (!playing || paused)
            return;
        flush();
        if (scripts)
            scripts->update(dt);
        flush();
    }
    void render(double alpha) {
        const auto size = viewport();
        view->camera.width = std::max(1, size.width);
        view->camera.height = std::max(1, size.height);
        view->draw(*world, renderer, (!playing || paused) ? 1.0F : static_cast<float>(alpha));
        Camera2D screen;
        screen.width = view->camera.width;
        screen.height = view->camera.height;
        screen.pixelsPerUnit = 1;
        screen.center = {static_cast<float>(screen.width) * .5F, static_cast<float>(screen.height) * .5F};
        std::vector<SpriteDraw> hud;
        const auto y = static_cast<float>(screen.height) - 16;
        assets.font("bitmap").append(hud,
                                     "2DOG 0.3 / C# / " +
                                         std::string(!playing ? "STOP"
                                                     : paused ? "PAUSE"
                                                              : "PLAY") +
                                         " / ENTITIES " + std::to_string(world->entities().size()) + " / STEP " +
                                         std::to_string(fixedCount),
                                     {16, y}, 2, {.5F, 1, .8F, 1});
        assets.font("bitmap").append(hud,
                                     "A-D MOVE / SPACE JUMP / CLICK CRATE / M MUSIC / Q MESSAGE\nF5 PLAY-STOP / P "
                                     "PAUSE / N STEP / F9 RELOAD C#\nF6 SOURCE SCENE / F7 SAVE / F8 LOAD SAVE / " +
                                         status,
                                     {16, y - 26}, 1.5F, {.9F, .93F, 1, 1});
        if (!embedded)
            renderer.draw(screen, hud);
        ui.draw(*world, assets, renderer, size.width, size.height);
    }
    void validateVisual(const Json &entity) {
        SceneView validation(project, assets, Json::Object{{"entities", Json::Array{entity}}});
        if (entity.contains("ui")) {
            auto candidate = world->snapshot();
            bool replaced = false;
            for (auto &e : candidate["entities"].array())
                if (e.at("id").string() == entity.at("id").string()) {
                    e = entity;
                    replaced = true;
                    break;
                }
            if (!replaced)
                candidate["entities"].array().push_back(entity);
            static_cast<void>(GameUI::layout(candidate, 1280, 720));
        }
    }
    Json dispatch(ScriptApi op, const Json &r) {
        const auto entity = op == ScriptApi::Create ? std::string{} : r.text("entity");
        switch (op) {
        case ScriptApi::Log:
            std::cout << "[2Dog][C#][" << r.text("level", "info") << "] " << r.at("message").string() << '\n';
            return {};
        case ScriptApi::Find:
            return world->find(r.at("name").string());
        case ScriptApi::Alive:
            return world->alive(entity) && !destroyed.contains(entity);
        case ScriptApi::GetTransform:
            return writeTransform(world->transform(entity));
        case ScriptApi::SetTransform:
            world->transform(entity, readTransform(r.at("transform")));
            return {};
        case ScriptApi::Create: {
            auto e = r.at("entity");
            if (!e.contains("id"))
                e["id"] = newPersistentId();
            validateVisual(e);
            const auto id = world->create(std::move(e));
            dirty = true;
            return id;
        }
        case ScriptApi::Destroy:
            static_cast<void>(world->get(entity));
            destroyed.insert(entity);
            dirty = true;
            return {};
        case ScriptApi::Instantiate: {
            auto e = project.prefab(r.at("prefab").string(), jsonVec2(r.at("position")));
            validateVisual(e);
            const auto id = world->create(std::move(e));
            dirty = true;
            return id;
        }
        case ScriptApi::Input: {
            const auto action = r.at("action").string();
            const auto state = inFixed ? fixedInput.at(action) : window.input().action(action);
            return Json::Object{{"down", state.down}, {"pressed", state.pressed}, {"released", state.released}};
        }
        case ScriptApi::Axis:
            return window.input().axis(r.at("axis").integer());
        case ScriptApi::Pointer:
            return toJson(view->pointer(
                {static_cast<float>(window.input().cursorX), static_cast<float>(window.input().cursorY)}));
        case ScriptApi::GetVelocity:
            return toJson(world->physics.velocity(world->get(entity).body));
        case ScriptApi::SetVelocity:
            world->physics.setVelocity(world->get(entity).body, jsonVec2(r.at("value")));
            return {};
        case ScriptApi::Impulse:
            world->physics.impulse(world->get(entity).body, jsonVec2(r.at("value")));
            return {};
        case ScriptApi::Force:
            world->physics.force(world->get(entity).body, jsonVec2(r.at("value")));
            return {};
        case ScriptApi::Raycast: {
            const auto ignore = r.text("ignore");
            const auto hit =
                world->physics.raycast(jsonVec2(r.at("origin")), jsonVec2(r.at("translation")), ~std::uint64_t{0},
                                       ignore.empty() ? BodyHandle{} : world->get(ignore).body);
            if (!hit)
                return {};
            return Json::Object{{"entity", world->entityFor(hit->body)},
                                {"point", toJson(hit->point)},
                                {"normal", toJson(hit->normal)},
                                {"fraction", hit->fraction}};
        }
        case ScriptApi::PlayAudio:
            return audio.play(project.asset(r.at("asset").string(), "audio").path, r.text("bus", "sfx"),
                              r.flag("loop"));
        case ScriptApi::StopAudio:
            audio.stop(r.at("voice").string());
            return {};
        case ScriptApi::Volume:
            audio.volume(r.at("bus").string(), static_cast<float>(r.at("value").number()));
            return {};
        case ScriptApi::LoadScene:
            if (!shuttingDown)
                requestedScene = r.at("asset").string();
            return {};
        case ScriptApi::SaveScene:
            save(r.at("path").string());
            return {};
        case ScriptApi::GetComponent: {
            const auto &e = world->get(entity).document;
            const auto name = r.at("component").string();
            if (name == "transform")
                return writeTransform(world->transform(entity));
            return e.contains(name) ? e.at(name) : Json{};
        }
        case ScriptApi::SetComponent: {
            static_cast<void>(r.at("value").object());
            const auto name = r.at("component").string();
            if (name != "sprite" && name != "animation" && name != "text" && name != "properties" && name != "ui")
                throw std::invalid_argument("Writable components: sprite, animation, text, properties, ui; use "
                                            "physics/transform APIs for bodies");
            auto e = world->get(entity).document;
            e[name] = r.at("value");
            Project::validateEntity(e);
            validateVisual(e);
            world->get(entity).document = std::move(e);
            return {};
        }
        case ScriptApi::Send: {
            if (messages.size() >= 1024)
                throw std::runtime_error("Message queue full");
            if (!entity.empty())
                static_cast<void>(world->get(entity));
            messages.push_back(Json::Object{{"entity", entity},
                                            {"topic", r.at("topic")},
                                            {"payload", r.contains("payload") ? r.at("payload") : Json{}}});
            return {};
        }
        case ScriptApi::Camera:
            view->configure(r);
            return {};
        case ScriptApi::Emit:
            view->emit(jsonVec2(r.at("position")), r.at("count").integer(), jsonVec4(r.at("color")));
            return {};
        case ScriptApi::SetState:
            view->setState(*world, entity, r.at("state").string());
            return {};
        case ScriptApi::ListEntities: {
            Json::Array result;
            for (const auto &[id, e] : world->entities()) {
                static_cast<void>(e);
                if (!destroyed.contains(id))
                    result.emplace_back(id);
            }
            return result;
        }
        case ScriptApi::AddCollider:
            world->addCollider(entity, r.at("collider"));
            return {};
        case ScriptApi::CreateJoint:
            return world->createJoint(r.at("joint"));
        case ScriptApi::DestroyJoint:
            world->destroyJoint(r.at("joint").string());
            return {};
        case ScriptApi::BindAction:
            bindOne(r);
            return {};
        case ScriptApi::BatchRead: {
            const auto &queries = r.at("queries").array();
            if (queries.size() > 128)
                throw std::invalid_argument("BatchRead limit: 128");
            Json::Array result;
            for (const auto &q : queries) {
                const auto code = static_cast<ScriptApi>(q.at("op").integer());
                switch (code) {
                case ScriptApi::Find:
                case ScriptApi::Alive:
                case ScriptApi::GetTransform:
                case ScriptApi::GetVelocity:
                case ScriptApi::GetComponent:
                case ScriptApi::Input:
                case ScriptApi::Axis:
                case ScriptApi::Pointer:
                case ScriptApi::Raycast:
                case ScriptApi::ListEntities:
                    break;
                default:
                    throw std::invalid_argument("BatchRead only accepts read operations");
                }
                result.push_back(dispatch(code, q.at("request")));
            }
            return result;
        }
        }
        throw std::invalid_argument("Unknown native API operation");
    }
};

ProjectPlayer::ProjectPlayer(Window &w, const std::filesystem::path &p, const std::filesystem::path &m, bool s,
                             const Json *scene, bool embedded, const std::filesystem::path &gameAssembly)
    : impl_(std::make_unique<Impl>(w, p, m, s, scene, embedded, gameAssembly)) {}
ProjectPlayer::~ProjectPlayer() = default;
void ProjectPlayer::poll(bool shortcuts, bool acceptInput) { impl_->poll(shortcuts, acceptInput); }
void ProjectPlayer::setViewport(int width, int height) {
    impl_->viewportWidth = std::max(1, width);
    impl_->viewportHeight = std::max(1, height);
}
void ProjectPlayer::setPaused(bool value) {
    impl_->paused = value;
    impl_->audio.pause(value);
}
void ProjectPlayer::step() {
    if (impl_->paused)
        impl_->fixed(1.0 / 60.0, true);
}
void ProjectPlayer::fixedUpdate(double dt) { impl_->fixed(dt); }
void ProjectPlayer::update(double dt) { impl_->update(dt); }
void ProjectPlayer::render(double alpha) { impl_->render(alpha); }
void ProjectPlayer::capture(const std::filesystem::path &p) { impl_->renderer.savePPM(p); }
} // namespace twodog
