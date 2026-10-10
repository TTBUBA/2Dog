#include <twodog/data/ComponentSchema.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <sstream>
#include <twodog/core/Utf8Path.hpp>
#include <twodog/editor/EditorActions.hpp>
#include <twodog/editor/EditorAssets.hpp>
#include <twodog/editor/EditorContext.hpp>
#include <twodog/editor/EditorDefaults.hpp>
#include <twodog/editor/EditorWidgets.hpp>
#include <twodog/gameplay/Input.hpp>
#include <utility>
namespace twodog {
void EditorActions::refreshSelection([[maybe_unused]] EditorContext &ctx) { ctx.scene_.refreshSelection(); }

void EditorActions::preview([[maybe_unused]] EditorContext &ctx, const Json &doc) {
    auto world = std::make_unique<World2D>(*ctx.project_, doc);
    auto visual = std::make_unique<SceneView>(*ctx.project_, *ctx.assets_, doc);
    static_cast<void>(GameUI::layout(doc, 1280, 720));
    ctx.preview_ = std::move(world);
    ctx.view_ = std::move(visual);
}

void EditorActions::commit([[maybe_unused]] EditorContext &ctx, Json next, bool record) {
    ctx.scene_.commit(std::move(next), record);
}

void EditorActions::loadScene([[maybe_unused]] EditorContext &ctx, const std::filesystem::path &path) {
    ctx.scene_.load(Json::read(path));
    ctx.scenePath_ = path;
    ctx.assetEditId_.clear();
    if (ctx.scene_.document().contains("camera"))
        ctx.camera_ = ctx.view_->camera;
    ctx.message_ = "SCENE LOADED";
}

void EditorActions::save([[maybe_unused]] EditorContext &ctx) {
    if (ctx.player_)
        throw std::runtime_error("Stop Play before saving");
    EditorActions::preview(ctx, ctx.scene_.document());
    ctx.scene_.document().save(ctx.scenePath_);
    bool registered = false;
    for (const auto &[id, a] : ctx.project_->assets()) {
        static_cast<void>(id);
        if (a.type == "scene" && a.path == ctx.scenePath_)
            registered = true;
    }
    if (!registered)
        EditorAssets::registerAsset(ctx, ctx.scene_.document().at("id").string(), "scene", ctx.scenePath_);
    ctx.scene_.markSaved();
    ctx.message_ = "SCENE SAVED";
}

void EditorActions::protect([[maybe_unused]] EditorContext &ctx, std::function<void()> action) {
    if (ctx.scene_.dirty()) {
        ctx.pending_ = std::move(action);
        ctx.askDiscard_ = true;
    } else
        action();
}

void EditorActions::play([[maybe_unused]] EditorContext &ctx) {
    if (ctx.player_)
        return;
    if (!ctx.nativeOnly_ && ctx.scripts_)
        ctx.scripts_->tick(ctx.window_.nowSeconds(), true);
    if (!ctx.nativeOnly_ && (!ctx.scripts_ || !ctx.scripts_->ready()))
        throw std::runtime_error("Wait for a successful C# build before Play; see the Assets status and Console");
    ctx.player_ =
        std::make_unique<ProjectPlayer>(ctx.window_, ctx.root_, ctx.managed_, !ctx.nativeOnly_, &ctx.scene_.document(),
                                        true, ctx.nativeOnly_ ? std::filesystem::path{} : ctx.scripts_->assembly());
    ctx.paused_ = false;
    ctx.message_ = "PLAY - scene edits locked; Stop restores the authored scene";
}

void EditorActions::stop([[maybe_unused]] EditorContext &ctx) {
    ctx.player_.reset();
    ctx.window_.input() = Input{};
    ctx.paused_ = false;
    ctx.gameActive_ = false;
    ctx.message_ = "STOP - authored scene restored";
}

void EditorActions::history([[maybe_unused]] EditorContext &ctx, bool back) { ctx.scene_.history(back); }

void EditorActions::create([[maybe_unused]] EditorContext &ctx, const std::string &kind) {
    auto next = ctx.scene_.document();
    Json e = Json::Object{{"id", newPersistentId()}, {"name", kind}, {"transform", writeTransform(Transform2D{})}};
    if (kind == "Sprite") {
        e["sprite"] = ComponentSchema::defaults("sprite");
        e["sprite"]["color"] = Json::Array{.3, .8, .6, 1};
    } else if (kind != "Empty") {
        e["ui"] = defaultUI(kind.c_str());
        if (kind != "Canvas") {
            std::string parent;
            if (const auto *current = SceneDocument::find(next, ctx.scene_.selected());
                current && current->contains("ui"))
                parent = ctx.scene_.selected();
            if (parent.empty())
                for (const auto &candidate : next.at("entities").array())
                    if (candidate.contains("ui") && candidate.at("ui").text("kind") == "Canvas") {
                        parent = candidate.at("id").string();
                        break;
                    }
            if (parent.empty()) {
                parent = newPersistentId();
                next["entities"].array().push_back(Json::Object{{"id", parent},
                                                                {"name", "Canvas"},
                                                                {"transform", writeTransform(Transform2D{})},
                                                                {"ui", defaultUI("Canvas")}});
            }
            e["ui"]["parent"] = parent;
        }
    }
    const auto id = e.at("id").string();
    next["entities"].array().push_back(e);
    ctx.scene_.commit(std::move(next), true, id);
}

void EditorActions::duplicate([[maybe_unused]] EditorContext &ctx) { ctx.scene_.duplicateSelected(); }

void EditorActions::remove([[maybe_unused]] EditorContext &ctx) { ctx.scene_.deleteSelected(); }

void EditorActions::attachScript([[maybe_unused]] EditorContext &ctx, const std::filesystem::path &path,
                                 const std::string &target) {
    if (ctx.player_)
        throw std::runtime_error("Stop Play before assigning a script");
    const auto types = ctx.scripts_->types(path);
    if (types.size() != 1)
        throw std::runtime_error("Drag a file with exactly one class directly inheriting TwoDog.Behaviour; for other "
                                 "files use the type field");
    ctx.scene_.attachScript(target, types.front());
    ctx.message_ = "C# SCRIPT ATTACHED - save the scene to keep the assignment";
}

} // namespace twodog
