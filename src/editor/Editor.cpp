#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <sstream>
#include <twodog/core/Application.hpp>
#include <twodog/core/Utf8Path.hpp>
#include <twodog/editor/Editor.hpp>
#include <twodog/editor/EditorActions.hpp>
#include <twodog/editor/EditorAssets.hpp>
#include <twodog/editor/EditorChrome.hpp>
#include <twodog/editor/EditorContext.hpp>
#include <twodog/editor/EditorDefaults.hpp>
#include <twodog/editor/EditorWidgets.hpp>
#include <twodog/gameplay/Input.hpp>
#include <utility>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <twodog/editor/panels/AssetsPanel.hpp>
#include <twodog/editor/panels/ConsolePanel.hpp>
#include <twodog/editor/panels/GamePanel.hpp>
#include <twodog/editor/panels/HierarchyPanel.hpp>
#include <twodog/editor/panels/InspectorPanel.hpp>
#include <twodog/editor/panels/ScenePanel.hpp>
namespace twodog {
namespace {
std::filesystem::path executable(const char *arg) {
#ifdef _WIN32
    static_cast<void>(arg);
    std::vector<wchar_t> buffer(32768);
    const auto n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (n == 0 || n >= buffer.size())
        throw std::runtime_error("Cannot locate editor executable");
    return std::filesystem::path(std::wstring(buffer.data(), n));
#else
    return std::filesystem::absolute(arg);
#endif
}
class Editor final : public FrameHost {
    EditorContext context_;

  public:
    Editor(const WindowConfig &config, const LaunchOptions &options, const std::filesystem::path &root,
           const std::filesystem::path &managed, const std::filesystem::path &support)
        : context_(config, options, root, managed, support) {}
    void pollEvents() override {
        auto &ctx = context_;
        ctx.retiredAssets_.clear();
        ctx.window_.pollEvents();
        if (ctx.scripts_ && !ctx.nativeOnly_ && !ctx.options_.smokeTest)
            ctx.attempt([&] { ctx.scripts_->tick(ctx.window_.nowSeconds()); });
        if (ctx.window_.shouldClose()) {
            ctx.window_.cancelClose();
            if (ctx.player_)
                EditorActions::stop(ctx);
            if (ctx.window_.suspended())
                glfwRestoreWindow(static_cast<GLFWwindow *>(ctx.window_.nativeHandle()));
            ctx.attempt([&] { EditorActions::protect(ctx, [&ctx] { ctx.done_ = true; }); });
        }
        if (ctx.window_.suspended()) {
            ctx.window_.input().releaseAll();
            static_cast<void>(ctx.window_.input().consumeFixed());
            return;
        }
        if (ctx.window_.nowSeconds() >= ctx.assetPoll_) {
            ctx.assetPoll_ = ctx.window_.nowSeconds() + .5;
            ctx.assets_->poll();
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        auto &io = ImGui::GetIO();
        if (!io.WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
            if (ImGui::IsKeyPressed(ImGuiKey_F5))
                ctx.attempt([&] {
                    if (ctx.player_)
                        EditorActions::stop(ctx);
                    else
                        EditorActions::play(ctx);
                });
            if (!ctx.player_ && io.KeyCtrl) {
                if (ImGui::IsKeyPressed(ImGuiKey_S))
                    ctx.attempt([&] { EditorActions::save(ctx); });
                if (ImGui::IsKeyPressed(ImGuiKey_Z))
                    ctx.attempt([&] { EditorActions::history(ctx, true); });
                if (ImGui::IsKeyPressed(ImGuiKey_Y))
                    ctx.attempt([&] { EditorActions::history(ctx, false); });
            }
        }
        EditorChrome::toolbar(ctx);
        EditorChrome::docking(ctx);
        HierarchyPanel::draw(ctx);
        InspectorPanel::draw(ctx);
        AssetsPanel::draw(ctx);
        EditorChrome::settingsPanel(ctx);
        EditorChrome::tileEditor(ctx);
        ScenePanel::draw(ctx);
        GamePanel::draw(ctx);
        ConsolePanel::draw(ctx);
        EditorChrome::discardDialog(ctx);
        ctx.windowsExport_->draw(!ctx.player_ && !ctx.scripts_->busy(), [&] { EditorActions::save(ctx); });
        if (ctx.player_)
            ctx.attempt([&] {
                ctx.player_->setViewport(ctx.gameTarget_.width(), ctx.gameTarget_.height());
                if (ctx.gameSize_.x <= 0 || ctx.gameSize_.y <= 0)
                    ctx.gameSize_ = ImVec2(1, 1);
                ctx.window_.input().cursor((io.MousePos.x - ctx.gameOrigin_.x) *
                                               static_cast<float>(ctx.gameTarget_.width()) / ctx.gameSize_.x,
                                           (io.MousePos.y - ctx.gameOrigin_.y) *
                                               static_cast<float>(ctx.gameTarget_.height()) / ctx.gameSize_.y);
                ctx.player_->poll(false, ctx.gameActive_ && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId));
            });
    }
    bool shouldClose() const override {
        auto &ctx = context_;
        return ctx.done_;
    }
    bool suspended() const override {
        auto &ctx = context_;
        return ctx.window_.suspended();
    }
    void waitEvents(double timeout) override {
        auto &ctx = context_;
        ctx.window_.waitEvents(timeout);
    }
    double nowSeconds() const override {
        auto &ctx = context_;
        return ctx.window_.nowSeconds();
    }
    void fixedUpdate(double dt) override {
        auto &ctx = context_;
        if (ctx.player_)
            ctx.attempt([&] { ctx.player_->fixedUpdate(dt); });
    }
    void update(double dt) override {
        auto &ctx = context_;
        if (ctx.player_)
            ctx.attempt([&] { ctx.player_->update(dt); });
    }
    void render(double alpha) override {
        auto &ctx = context_;
        ScenePanel::render(ctx);
        GamePanel::render(ctx, alpha);
        ctx.gl_.clear(.04F, .05F, .07F);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ++ctx.renderedFrames_;
        if (!ctx.options_.capturePath.empty() && ctx.renderedFrames_ == ctx.options_.maxFrames)
            ctx.renderer_.savePPM(pathFromUtf8(ctx.options_.capturePath));
        if (ctx.options_.smokeTest)
            ctx.gl_.checkError("Editor frame");
    }
    void present() override {
        auto &ctx = context_;
        ctx.window_.present();
    }
};
} // namespace
int editorMain(int argc, char **argv) {
    try {
        std::vector<std::string_view> arguments;
        for (int i = 1; i < argc; ++i)
            arguments.emplace_back(argv[i]);
        const auto options = parseOptions(arguments);
        if (options.help) {
            std::cout << "2Dog Editor - Hierarchy, Inspector, Assets, Scene/Game, UI authoring\n" << usage;
            return 0;
        }
        const auto exe = executable(argv[0]).parent_path();
        const auto root = options.projectPath.empty() || options.projectPath == "bundled"
                              ? exe / "projects" / "EditorDemo"
                              : pathFromUtf8(options.projectPath);
        const auto managed = options.managedPath.empty() ? exe / "managed" : pathFromUtf8(options.managedPath);
        WindowConfig config;
        config.title = "2Dog Editor 0.7.3";
        config.width = 1600;
        config.height = 900;
        config.visible = !options.hidden;
        config.vsync = options.vsync;
        Editor editor(config, options, root, managed, exe / "EditorSupport");
        const auto stats = runApplication(editor, options.maxFrames);
        std::cout << "[Editor] frames=" << stats.frames << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "[Editor] " << e.what() << '\n';
        return 1;
    }
}
} // namespace twodog
