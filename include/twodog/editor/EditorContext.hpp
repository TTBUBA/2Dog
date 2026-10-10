#pragma once
#include <functional>
#include <imgui.h>
#include <iostream>
#include <memory>
#include <twodog/core/LaunchOptions.hpp>
#include <twodog/editor/EditorSession.hpp>
#include <twodog/editor/RenderTarget.hpp>
#include <twodog/editor/SceneDocument.hpp>
#include <twodog/editor/ScriptWorkspace.hpp>
#include <twodog/editor/WindowsExport.hpp>
#include <twodog/platform/GameUI.hpp>
#include <twodog/platform/OpenGL.hpp>
#include <twodog/platform/ProjectPlayer.hpp>
#include <vector>
namespace twodog {
// Session-owned shared state. Authoring document writes go through SceneDocument.
// Resource declaration order preserves GLFW/GL/ImGui and texture lifetimes.
struct EditorContext final {

    Window window_;
    OpenGL gl_;
    ImGuiSession gui_;
    ConsoleCapture console_;
    SpriteRenderer renderer_;
    RenderTarget sceneTarget_, gameTarget_;
    std::filesystem::path root_, managed_, scenePath_;
    std::unique_ptr<Project> project_;
    std::unique_ptr<VisualAssets> assets_;
    struct RetiredAssets {
        std::unique_ptr<Project> project;
        std::unique_ptr<VisualAssets> assets;
    };
    std::vector<RetiredAssets> retiredAssets_;
    std::unique_ptr<World2D> preview_;
    std::unique_ptr<SceneView> view_;
    GameUI gameUI_;
    std::unique_ptr<ProjectPlayer> player_;
    SceneDocument scene_;
    std::string message_ = "READY", filter_;
    Camera2D camera_;
    bool paused_ = false, nativeOnly_ = false, done_ = false;
    bool gameActive_ = false, sceneVisible_ = false, gameVisible_ = false, layout_ = false, snap_ = true,
         showUI_ = true;
    int tool_ = 0;
    float snapSize_ = .25F;
    double assetPoll_ = 0;
    ImVec2 sceneOrigin_{}, sceneSize_{}, gameOrigin_{}, gameSize_{};
    bool dragging_ = false;
    glm::vec2 dragMouse_{0};
    Transform2D dragStart_;
    std::string saveName_ = "scenes/new.scene.json", importSource_, importId_, importType_ = "texture",
                importDest_ = "assets/new.png", propertyName_, scriptType_ = "GameScripts.UiDemo";
    std::string assetEditId_, assetText_, sceneSettingsText_;
    Json tileDocument_;
    std::string tileAsset_;
    int tileLayer_ = 0, tileGid_ = 1;
    std::function<void()> pending_;
    bool askDiscard_ = false;
    LaunchOptions options_;
    std::uint64_t renderedFrames_ = 0;
    std::unique_ptr<ScriptWorkspace> scripts_;
    std::unique_ptr<WindowsExport> windowsExport_;
    std::filesystem::path assetFolder_, assetSelected_;
    std::vector<std::filesystem::path> assetFiles_;
    std::string assetSearch_, newFolder_ = "New Folder", newScript_ = "NewBehaviour", assetTypeFilter_ = "All";
    std::string attachFile_, attachEntity_;
    float thumbnailSize_ = 92;
    double browserScan_ = 0;
    bool allAssets_ = false;
    int createAssetKind_ = 0;
    bool openAssetCreate_ = false;
    int gameResolution_ = 0;
    std::function<void()> browserAction_;

    EditorContext(const WindowConfig &, const LaunchOptions &, const std::filesystem::path &root,
                  const std::filesystem::path &managed, const std::filesystem::path &support);
    ~EditorContext();
    EditorContext(const EditorContext &) = delete;
    EditorContext &operator=(const EditorContext &) = delete;
    template <class F> void attempt(F &&fn) {
        try {
            fn();
        } catch (const std::exception &e) {
            message_ = e.what();
            std::cerr << "[Editor] " << e.what() << '\n';
        }
    }
};
} // namespace twodog
