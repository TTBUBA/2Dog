#include <cwctype>
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
#include <twodog/editor/panels/AssetsPanel.hpp>
#include <utility>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Windows SDK declarations must precede common dialog declarations.
#include <windows.h>

#include <commdlg.h>
#endif
namespace twodog {
void AssetsPanel::folderTree([[maybe_unused]] EditorContext &ctx, const std::filesystem::path &directory, int depth) {
    if (depth > 12)
        return;
    const auto key = pathToUtf8(directory.lexically_relative(ctx.root_));
    ImGui::PushID(key.c_str());
    const auto name = directory == ctx.root_ ? "Project" : pathToUtf8(directory.filename());
    const auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                       (directory == ctx.root_ ? ImGuiTreeNodeFlags_DefaultOpen : 0) |
                       (ctx.assetFolder_ == directory && !ctx.allAssets_ ? ImGuiTreeNodeFlags_Selected : 0);
    const bool open = ImGui::TreeNodeEx(name.c_str(), flags);
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        ctx.assetFolder_ = directory;
        ctx.allAssets_ = false;
        ctx.browserScan_ = 0;
    }
    if (ImGui::BeginPopupContextItem("Folder actions")) {
        AssetsPanel::assetCreationMenu(ctx, directory);
        ImGui::EndPopup();
    }
    if (open) {
        std::vector<std::filesystem::path> dirs;
        for (const auto &entry : std::filesystem::directory_iterator(directory)) {
            const auto n = pathToUtf8(entry.path().filename());
            if (entry.is_directory() && !entry.is_symlink() && !n.starts_with(".") && n != "obj" && n != "bin")
                dirs.push_back(entry.path());
        }
        std::sort(dirs.begin(), dirs.end());
        for (const auto &dir : dirs)
            AssetsPanel::folderTree(ctx, dir, depth + 1);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void AssetsPanel::importDialog([[maybe_unused]] EditorContext &ctx) {
#ifdef _WIN32
    wchar_t filename[32768]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = filename;
    dialog.nMaxFile = 32768;
    dialog.lpstrFilter =
        L"Game assets\0*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.wav;*.mp3;*.ogg;*.flac;*.ttf;*.cs;*.json;*.tmj\0All "
        L"files\0*.*\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog))
        return;
    const std::filesystem::path source(filename);
    const auto target = EditorAssets::writableAssetFolder(ctx) / source.filename();
    if (std::filesystem::exists(target))
        throw std::runtime_error("A file with that name already exists in this folder");
    auto ext = source.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    std::string type;
    if (ext == L".cs")
        type = "script";
    else if (ext == L".ttf")
        type = "font";
    else if (ext == L".wav" || ext == L".mp3" || ext == L".ogg" || ext == L".flac")
        type = "audio";
    else if (ext == L".tmj")
        type = "tilemap";
    else if (ext == L".json") {
        auto j = Json::read(source);
        type = j.contains("entities")   ? "scene"
               : j.contains("entity")   ? "prefab"
               : j.contains("tilesets") ? "tilemap"
                                        : "";
    } else if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bmp" || ext == L".tga")
        type = "texture";
    if (type.empty())
        throw std::runtime_error("Unsupported asset format; use Advanced Import for explicit types");
    const auto id = type + "." + pathToUtf8(source.stem()) + "." + newPersistentId();
    std::filesystem::copy_file(source, target);
    try {
        if (type != "script")
            EditorAssets::registerAsset(ctx, id, type, target);
    } catch (...) {
        std::filesystem::remove(target);
        throw;
    }
    ctx.assetSelected_ = target;
    ctx.assetFolder_ = target.parent_path();
    ctx.allAssets_ = false;
    ctx.browserScan_ = 0;
    ctx.message_ = "ASSET IMPORTED";
#else
    throw std::runtime_error("Native import dialog requires Windows; use Advanced Import");
#endif
}

void AssetsPanel::requestAssetCreation([[maybe_unused]] EditorContext &ctx, int kind,
                                       const std::filesystem::path &folder) {
    ctx.assetFolder_ = folder;
    ctx.allAssets_ = false;
    ctx.browserScan_ = 0;
    ctx.createAssetKind_ = kind;
    ctx.openAssetCreate_ = true;
    auto &name = kind == 1 ? ctx.newFolder_ : ctx.newScript_;
    const std::string base = kind == 1 ? "New Folder" : "NewBehaviour";
    name = base;
    for (int i = 1; std::filesystem::exists(EditorAssets::writableAssetFolder(ctx) / (name + (kind == 1 ? "" : ".cs")));
         ++i)
        name = base + std::to_string(i);
}

void AssetsPanel::assetCreationMenu([[maybe_unused]] EditorContext &ctx, const std::filesystem::path &folder) {
    if (ImGui::MenuItem("Create Folder"))
        ctx.attempt([&] { AssetsPanel::requestAssetCreation(ctx, 1, folder); });
    if (ImGui::MenuItem("Create C# Script"))
        ctx.attempt([&] { AssetsPanel::requestAssetCreation(ctx, 2, folder); });
}

void AssetsPanel::assetBrowser([[maybe_unused]] EditorContext &ctx) {
    if (ImGui::Button("+ Create"))
        ImGui::OpenPopup("Asset create menu");
    if (ImGui::BeginPopup("Asset create menu")) {
        AssetsPanel::assetCreationMenu(ctx, ctx.assetFolder_);
        ImGui::EndPopup();
    }
    if (ctx.openAssetCreate_) {
        ImGui::OpenPopup("Create asset");
        ctx.openAssetCreate_ = false;
    }
    if (ImGui::BeginPopupModal("Create asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(ctx.createAssetKind_ == 1 ? "New Folder" : "New C# Behaviour");
        ImGui::TextWrapped("Inside: %s",
                           pathToUtf8(EditorAssets::writableAssetFolder(ctx).lexically_relative(ctx.root_)).c_str());
        auto &name = ctx.createAssetKind_ == 1 ? ctx.newFolder_ : ctx.newScript_;
        ImGui::InputText("Name", &name);
        if (ImGui::Button("Create", ImVec2(120, 32)))
            ctx.attempt([&] {
                if (ctx.createAssetKind_ == 1) {
                    if (ctx.newFolder_.empty() || ctx.newFolder_ == "." || ctx.newFolder_ == ".." ||
                        ctx.newFolder_.find_first_of("/\\:") != std::string::npos)
                        throw std::runtime_error("Enter a single folder name");
                    const auto target = containedPath(
                        ctx.root_ / "assets", (EditorAssets::writableAssetFolder(ctx) / pathFromUtf8(ctx.newFolder_))
                                                  .lexically_relative(ctx.root_ / "assets"));
                    if (!std::filesystem::create_directory(target))
                        throw std::runtime_error("Folder already exists");
                    ctx.assetSelected_ = target;
                    ctx.message_ = "Folder created. Double-click to open.";
                } else {
                    ctx.assetSelected_ = ctx.scripts_->create(EditorAssets::writableAssetFolder(ctx), ctx.newScript_);
                    ctx.message_ = "Script created. Drag it to an object; double-click to edit.";
                }
                ctx.assetFolder_ = ctx.assetSelected_.parent_path();
                ctx.allAssets_ = false;
                ctx.browserScan_ = 0;
                ImGui::CloseCurrentPopup();
            });
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 32)))
            ImGui::CloseCurrentPopup();
        ImGui::TextWrapped("%s", ctx.message_.c_str());
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(ctx.player_ != nullptr);
    if (ImGui::Button("Import..."))
        ctx.attempt([&] { AssetsPanel::importDialog(ctx); });
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("All assets")) {
        ctx.allAssets_ = true;
        ctx.browserScan_ = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        ctx.browserScan_ = 0;
        if (!ctx.player_)
            ctx.attempt([&] { EditorAssets::reloadProject(ctx); });
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(ctx.nativeOnly_);
    if (ImGui::Button("Compile C#"))
        ctx.scripts_->rebuild();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if(ImGui::Button("Refresh IDE project"))
        ctx.attempt([&] { ctx.scripts_->refreshIDE(); ctx.message_="IDE project refreshed. Reopen the solution in Visual Studio."; });
    if (ctx.nativeOnly_)
        ImGui::TextDisabled("Native-only mode: C# compilation and execution disabled");
    ImGui::TextColored(ctx.scripts_->ready() ? ImVec4(.3F, .9F, .65F, 1) : ImVec4(1, .75F, .3F, 1), "%s",
                       ctx.scripts_->status().c_str());
    ImGui::SetNextItemWidth(180);
    ImGui::InputTextWithHint("##AssetSearch", "Search assets...", &ctx.assetSearch_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(105);
    if (ImGui::BeginCombo("##AssetType", ctx.assetTypeFilter_.c_str())) {
        for (const char *k : {"All", "script", "texture", "scene", "prefab", "audio", "font", "tilemap"})
            if (ImGui::Selectable(k, ctx.assetTypeFilter_ == k))
                ctx.assetTypeFilter_ = k;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(115);
    ImGui::SliderFloat("Size", &ctx.thumbnailSize_, 64, 144, "%.0f");
    if (ImGui::SmallButton("Project")) {
        ctx.assetFolder_ = ctx.root_;
        ctx.allAssets_ = false;
        ctx.browserScan_ = 0;
    }
    std::filesystem::path crumb = ctx.root_;
    if (!ctx.allAssets_)
        for (const auto &part : ctx.assetFolder_.lexically_relative(ctx.root_)) {
            if (part == ".")
                continue;
            crumb /= part;
            ImGui::SameLine();
            ImGui::TextUnformatted("/");
            ImGui::SameLine();
            if (ImGui::SmallButton(pathToUtf8(part).c_str())) {
                ctx.assetFolder_ = crumb;
                ctx.browserScan_ = 0;
            }
        }
    else {
        ImGui::SameLine();
        ImGui::TextUnformatted("/ All assets");
    }
    if (ctx.window_.nowSeconds() >= ctx.browserScan_)
        ctx.attempt([&] { EditorAssets::refreshBrowser(ctx); });
    const auto available = ImGui::GetContentRegionAvail();
    const float height = std::max(160.0F, available.y - 75);
    ImGui::BeginChild("Folders", ImVec2(155, height), ImGuiChildFlags_Borders);
    ctx.attempt([&] { AssetsPanel::folderTree(ctx, ctx.root_); });
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("Asset grid", ImVec2(0, height), ImGuiChildFlags_Borders);
    if (ImGui::BeginPopupContextWindow("Assets context",
                                       ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        AssetsPanel::assetCreationMenu(ctx, ctx.assetFolder_);
        ImGui::EndPopup();
    }
    const float cell = ctx.thumbnailSize_ + 14;
    const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cell));
    int col = 0;
    for (const auto &path : ctx.assetFiles_) {
        const auto name = pathToUtf8(path.filename()), kind = EditorAssets::assetKind(ctx, path),
                   id = EditorAssets::assetID(ctx, path);
        if (!ctx.assetSearch_.empty() && name.find(ctx.assetSearch_) == std::string::npos)
            continue;
        if (kind != "folder" && ctx.assetTypeFilter_ != "All" && kind != ctx.assetTypeFilter_)
            continue;
        ImGui::PushID(pathToUtf8(path).c_str());
        const auto pos = ImGui::GetCursorScreenPos();
        ImGui::Selectable("##Tile", ctx.assetSelected_ == path, ImGuiSelectableFlags_AllowDoubleClick,
                          ImVec2(ctx.thumbnailSize_, ctx.thumbnailSize_ + 32));
        if (ImGui::IsItemClicked())
            ctx.assetSelected_ = path;
        if (ImGui::BeginPopupContextItem("Asset actions")) {
            ctx.assetSelected_ = path;
            AssetsPanel::assetCreationMenu(ctx, kind == "folder" ? path : path.parent_path());
            ImGui::EndPopup();
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            ctx.browserAction_ = [&ctx, path] { EditorAssets::openAsset(ctx, path); };
        if (kind == "script" && ImGui::BeginDragDropSource()) {
            const auto payload = pathToUtf8(path.lexically_relative(ctx.root_));
            ImGui::SetDragDropPayload("TWODOG_SCRIPT", payload.c_str(), payload.size() + 1);
            ImGui::Text("C# %s", name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(name.c_str());
            ImGui::TextDisabled("%s", kind.c_str());
            ImGui::TextUnformatted(pathToUtf8(path.lexically_relative(ctx.root_)).c_str());
            ImGui::EndTooltip();
        }
        auto *draw = ImGui::GetWindowDrawList();
        const ImVec2 a(pos.x + 8, pos.y + 8), b(pos.x + ctx.thumbnailSize_ - 8, pos.y + ctx.thumbnailSize_ - 8);
        if (kind == "texture") {
            const auto size = ctx.assets_->textureSize(id);
            const float scale =
                std::min((b.x - a.x) / static_cast<float>(size.x), (b.y - a.y) / static_cast<float>(size.y));
            const ImVec2 fit(static_cast<float>(size.x) * scale, static_cast<float>(size.y) * scale);
            const ImVec2 origin((a.x + b.x - fit.x) * .5F, (a.y + b.y - fit.y) * .5F);
            draw->AddRectFilled(a, b, IM_COL32(35, 40, 48, 255), 4);
            draw->AddImage(static_cast<ImTextureID>(ctx.renderer_.nativeTexture(ctx.assets_->texture(id))), origin,
                           ImVec2(origin.x + fit.x, origin.y + fit.y), ImVec2(0, 1), ImVec2(1, 0));
        } else {
            const ImU32 color = kind == "folder"   ? IM_COL32(205, 160, 60, 255)
                                : kind == "script" ? IM_COL32(80, 175, 115, 255)
                                : kind == "prefab" ? IM_COL32(75, 150, 225, 255)
                                                   : IM_COL32(100, 120, 155, 255);
            draw->AddRectFilled(ImVec2(a.x, a.y + 8), b, color, 6);
            if (kind == "folder")
                draw->AddRectFilled(a, ImVec2(a.x + 32, a.y + 14), color, 4);
            const char *icon = kind == "folder"    ? "DIR"
                               : kind == "script"  ? "C#"
                               : kind == "scene"   ? "SCN"
                               : kind == "prefab"  ? "PFB"
                               : kind == "audio"   ? "SND"
                               : kind == "font"    ? "TTF"
                               : kind == "tilemap" ? "MAP"
                                                   : "FILE";
            const auto sz = ImGui::CalcTextSize(icon);
            draw->AddText(ImVec2((a.x + b.x - sz.x) * .5F, (a.y + b.y - sz.y) * .5F), IM_COL32_WHITE, icon);
        }
        draw->PushClipRect(ImVec2(pos.x, pos.y + ctx.thumbnailSize_),
                           ImVec2(pos.x + ctx.thumbnailSize_, pos.y + ctx.thumbnailSize_ + 32), true);
        draw->AddText(ImVec2(pos.x + 3, pos.y + ctx.thumbnailSize_), IM_COL32_WHITE, name.c_str());
        draw->PopClipRect();
        ImGui::PopID();
        if (++col < columns)
            ImGui::SameLine(0, 14);
        else
            col = 0;
    }
    ImGui::EndChild();
    if (ctx.browserAction_) {
        auto action = std::move(ctx.browserAction_);
        ctx.browserAction_ = {};
        ctx.attempt([&] { action(); });
    }
    if (!ctx.assetSelected_.empty() && std::filesystem::exists(ctx.assetSelected_)) {
        const auto kind = EditorAssets::assetKind(ctx, ctx.assetSelected_),
                   id = EditorAssets::assetID(ctx, ctx.assetSelected_);
        ImGui::TextWrapped("%s  [%s]", pathToUtf8(ctx.assetSelected_.lexically_relative(ctx.root_)).c_str(),
                           kind.c_str());
        if (kind == "script") {
            if (ImGui::Button("Open in Visual Studio"))
                ctx.attempt([&] { ctx.scripts_->open(ctx.assetSelected_); });
            ImGui::SameLine();
            ImGui::BeginDisabled(ctx.player_ != nullptr || ctx.scene_.selected().empty());
            if (ImGui::Button("Attach to selected object"))
                ctx.attempt([&] { EditorActions::attachScript(ctx, ctx.assetSelected_, ctx.scene_.selected()); });
            ImGui::EndDisabled();
        }
        ImGui::BeginDisabled(ctx.player_ != nullptr);
        if (kind == "texture" && !ctx.scene_.draft().null() && ctx.scene_.draft().contains("sprite") &&
            ImGui::Button("Assign texture to selected sprite"))
            ctx.scene_.draft()["sprite"]["texture"] = id;
        if (kind == "scene" && ImGui::Button("Use as startup scene"))
            ctx.attempt([&] {
                auto settings = ctx.project_->settings();
                settings["startScene"] = id;
                settings.save(ctx.root_ / "project.json");
                EditorAssets::reloadProject(ctx);
            });
        ImGui::EndDisabled();
    }
}

void AssetsPanel::draw([[maybe_unused]] EditorContext &ctx) {
    ImGui::Begin("Assets");
    AssetsPanel::assetBrowser(ctx);
    ImGui::BeginDisabled(ctx.player_ != nullptr);
    if (ImGui::CollapsingHeader("Advanced Import")) {
        ImGui::InputText("Source absolute path", &ctx.importSource_);
        ImGui::InputText("Destination relative path", &ctx.importDest_);
        ImGui::InputText("Asset ID", &ctx.importId_);
        if (ImGui::BeginCombo("Asset type", ctx.importType_.c_str())) {
            for (const char *t : {"texture", "font", "audio", "tilemap", "prefab", "scene", "script"})
                if (ImGui::Selectable(t, ctx.importType_ == t))
                    ctx.importType_ = t;
            ImGui::EndCombo();
        }
        if (ImGui::Button("Copy and register"))
            ctx.attempt([&] {
                const auto target = containedPath(ctx.root_, pathFromUtf8(ctx.importDest_));
                if (ctx.importType_ != "script" &&
                    (ctx.project_->assets().contains(ctx.importId_) || ctx.importId_.empty()))
                    throw std::runtime_error("Asset ID must be new and nonempty");
                if (ctx.importType_ == "script") {
                    static_cast<void>(
                        containedPath(ctx.root_ / "assets", target.lexically_relative(ctx.root_ / "assets")));
                    if (target.extension() != ".cs")
                        throw std::runtime_error("Script destination must be assets/.../Name.cs");
                }
                if (std::filesystem::exists(target))
                    throw std::runtime_error("Destination exists; choose a new filename");
                std::filesystem::create_directories(target.parent_path());
                std::filesystem::copy_file(pathFromUtf8(ctx.importSource_), target);
                try {
                    if (ctx.importType_ != "script")
                        EditorAssets::registerAsset(ctx, ctx.importId_, ctx.importType_, target);
                    ctx.browserScan_ = 0;
                } catch (...) {
                    std::filesystem::remove(target);
                    throw;
                }
                ctx.message_ = "ASSET IMPORTED";
            });
    }
    if (ImGui::CollapsingHeader("Export selected object as prefab")) {
        ImGui::InputText("Prefab ID", &ctx.importId_);
        ImGui::InputText("Prefab path", &ctx.importDest_);
        if (ImGui::Button("Save new prefab"))
            ctx.attempt([&] {
                const auto *e = SceneDocument::find(ctx.scene_.document(), ctx.scene_.selected());
                if (!e)
                    throw std::runtime_error("Select an object first");
                const auto path = containedPath(ctx.root_, pathFromUtf8(ctx.importDest_));
                if (path.extension() != ".json" || std::filesystem::exists(path))
                    throw std::runtime_error("Choose a new .json path");
                if (ctx.importId_.empty() || ctx.project_->assets().contains(ctx.importId_))
                    throw std::runtime_error("Choose a new asset ID");
                auto prefab = *e;
                if (prefab.contains("ui"))
                    prefab["ui"]["parent"] = "";
                std::filesystem::create_directories(path.parent_path());
                Json(Json::Object{{"version", 1}, {"entity", prefab}}).save(path);
                try {
                    EditorAssets::registerAsset(ctx, ctx.importId_, "prefab", path);
                } catch (...) {
                    std::filesystem::remove(path);
                    throw;
                }
                ctx.message_ = "PREFAB SAVED";
            });
    }
    ImGui::EndDisabled();
    ImGui::End();
}

} // namespace twodog
