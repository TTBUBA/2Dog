#include <twodog/editor/EditorActions.hpp>
#include <twodog/editor/EditorContext.hpp>
namespace twodog {
EditorContext::EditorContext(const WindowConfig &config, const LaunchOptions &options,
                             const std::filesystem::path &root, const std::filesystem::path &managed,
                             const std::filesystem::path &support)
    : window_(config), gl_(window_), gui_(window_), renderer_(window_), sceneTarget_(window_), gameTarget_(window_),
      root_(std::filesystem::canonical(root)), managed_(managed), nativeOnly_(options.noScripts), options_(options) {
    scene_.configure([this](const Json &next) { EditorActions::preview(*this, next); },
                     [this] { return player_ == nullptr; });
    window_.setEscapeCloses(false);
    assetFolder_ = root_ / "assets";
    scripts_ = std::make_unique<ScriptWorkspace>(root_, managed_, support);
    windowsExport_ = std::make_unique<WindowsExport>(root_, support);
    project_ = std::make_unique<Project>(root_);
    assets_ = std::make_unique<VisualAssets>(*project_, renderer_);
    EditorActions::loadScene(*this, project_->asset(project_->settings().text("startScene"), "scene").path);
    std::cout << "[Editor] Project: " << root_ << ". Scene and Game panels use independent render targets.\n";
}
EditorContext::~EditorContext() { player_.reset(); }
} // namespace twodog
