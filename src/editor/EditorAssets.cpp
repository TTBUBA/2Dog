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
#include <utility>
namespace twodog {
void EditorAssets::reloadProject([[maybe_unused]] EditorContext &ctx) {
    auto p = std::make_unique<Project>(ctx.root_);
    auto a = std::make_unique<VisualAssets>(*p, ctx.renderer_);
    auto w = std::make_unique<World2D>(*p, ctx.scene_.document());
    auto v = std::make_unique<SceneView>(*p, *a, ctx.scene_.document());
    ctx.project_.swap(p);
    ctx.assets_.swap(a);
    ctx.preview_.swap(w);
    ctx.view_.swap(v);
    // ImGui draw commands may still reference old texture IDs until this frame is rendered.
    ctx.retiredAssets_.push_back({std::move(p), std::move(a)});
}

void EditorAssets::registerAsset([[maybe_unused]] EditorContext &ctx, const std::string &id, const std::string &type,
                                 const std::filesystem::path &path) {
    if (id.empty() || ctx.project_->assets().contains(id))
        throw std::runtime_error("Use a new, nonempty asset ID");
    auto settings = ctx.project_->settings();
    settings["assets"].array().push_back(
        Json::Object{{"id", id}, {"type", type}, {"path", pathToUtf8(path.lexically_relative(ctx.root_))}});
    const auto previous = ctx.project_->settings();
    settings.save(ctx.root_ / "project.json");
    try {
        EditorAssets::reloadProject(ctx);
    } catch (...) {
        previous.save(ctx.root_ / "project.json");
        throw;
    }
}

std::string EditorAssets::assetKind([[maybe_unused]] EditorContext &ctx, const std::filesystem::path &path) {
    if (std::filesystem::is_directory(path))
        return "folder";
    if (path.extension() == ".cs")
        return "script";
    for (const auto &[id, a] : ctx.project_->assets()) {
        static_cast<void>(id);
        if (a.path == path)
            return a.type;
    }
    return "file";
}

std::string EditorAssets::assetID([[maybe_unused]] EditorContext &ctx, const std::filesystem::path &path) {
    for (const auto &[id, a] : ctx.project_->assets())
        if (a.path == path)
            return id;
    return {};
}

std::filesystem::path EditorAssets::writableAssetFolder([[maybe_unused]] EditorContext &ctx) {
    const auto relative = ctx.assetFolder_.lexically_relative(ctx.root_ / "assets");
    try {
        return containedPath(ctx.root_ / "assets", relative);
    } catch (const std::exception &) {
        return ctx.root_ / "assets";
    }
}

void EditorAssets::refreshBrowser([[maybe_unused]] EditorContext &ctx) {
    ctx.assetFiles_.clear();
    auto add = [&](const std::filesystem::directory_entry &entry) {
        const auto name = pathToUtf8(entry.path().filename());
        if (name.empty() || name.front() == '.' || entry.is_symlink() || name == "bin" || name == "obj")
            return;
        if (entry.is_directory() || entry.path().extension() == ".cs" ||
            !EditorAssets::assetID(ctx, entry.path()).empty())
            ctx.assetFiles_.push_back(entry.path());
    };
    if (ctx.allAssets_) {
        for (auto i = std::filesystem::recursive_directory_iterator(ctx.root_);
             i != std::filesystem::recursive_directory_iterator(); ++i) {
            const auto name = pathToUtf8(i->path().filename());
            if (i->is_directory() && (i->is_symlink() || name.starts_with(".") || name == "bin" || name == "obj")) {
                i.disable_recursion_pending();
                continue;
            }
            if (!i->is_directory())
                add(*i);
        }
    } else {
        if (!std::filesystem::is_directory(ctx.assetFolder_))
            ctx.assetFolder_ = ctx.root_ / "assets";
        for (const auto &entry : std::filesystem::directory_iterator(ctx.assetFolder_))
            add(entry);
    }
    std::sort(ctx.assetFiles_.begin(), ctx.assetFiles_.end(), [](const auto &a, const auto &b) {
        const bool ad = std::filesystem::is_directory(a), bd = std::filesystem::is_directory(b);
        return ad == bd ? a.filename() < b.filename() : ad;
    });
    ctx.browserScan_ = ctx.window_.nowSeconds() + .75;
}

void EditorAssets::openAsset([[maybe_unused]] EditorContext &ctx, const std::filesystem::path &path) {
    const auto kind = EditorAssets::assetKind(ctx, path), id = EditorAssets::assetID(ctx, path);
    if (kind == "folder") {
        ctx.assetFolder_ = path;
        ctx.allAssets_ = false;
        ctx.browserScan_ = 0;
    } else if (kind == "script")
        ctx.scripts_->open(path);
    else if (kind == "scene" && !ctx.player_)
        EditorActions::protect(ctx, [&ctx, path] { EditorActions::loadScene(ctx, path); });
    else if (kind == "prefab" && !ctx.player_) {
        auto next = ctx.scene_.document();
        next["entities"].array().push_back(ctx.project_->prefab(id, ctx.camera_.center));
        EditorActions::commit(ctx, std::move(next));
    } else if (kind == "tilemap" && !ctx.player_) {
        SceneView validation(
            *ctx.project_, *ctx.assets_,
            Json::Object{{"entities", Json::Array{}}, {"tilemaps", Json::Array{Json::Object{{"asset", id}}}}});
        ctx.tileDocument_ = Json::read(path);
        ctx.tileAsset_ = id;
        ctx.tileLayer_ = 0;
        ImGui::SetWindowFocus("Tilemap");
    }
}

} // namespace twodog
