#include <algorithm>
#include <cmath>
#include <fstream>
#include <imgui_internal.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <sstream>
#include <twodog/core/Utf8Path.hpp>
#include <twodog/editor/EditorActions.hpp>
#include <twodog/editor/EditorAssets.hpp>
#include <twodog/editor/EditorChrome.hpp>
#include <twodog/editor/EditorContext.hpp>
#include <twodog/editor/EditorDefaults.hpp>
#include <twodog/editor/EditorWidgets.hpp>
#include <utility>
namespace twodog {
void EditorChrome::tileEditor([[maybe_unused]] EditorContext &ctx) {
    ImGui::Begin("Tilemap");
    ImGui::BeginDisabled(ctx.player_ != nullptr);
    if (ctx.tileDocument_.isObject()) {
        ImGui::TextUnformatted(ctx.tileAsset_.c_str());
        auto &layers = ctx.tileDocument_["layers"].array();
        ImGui::SliderInt("Layer", &ctx.tileLayer_, 0, std::max(0, static_cast<int>(layers.size()) - 1));
        ImGui::InputInt("Tile GID (0 erases)", &ctx.tileGid_);
        ctx.tileGid_ = std::max(0, ctx.tileGid_);
        if (!layers.empty()) {
            auto &layer = layers[static_cast<std::size_t>(ctx.tileLayer_)];
            if (layer.text("type") == "tilelayer" && layer.at("data").isArray()) {
                const int w = layer.at("width").integer(), h = layer.at("height").integer();
                if (w > 0 && h > 0 && w <= 128 && h <= 128 &&
                    layer.at("data").array().size() == static_cast<std::size_t>(w * h)) {
                    ImGui::TextUnformatted(
                        "Click/drag cells to paint. Save validates the scene before keeping the file.");
                    ImGui::BeginChild("Tile grid", ImVec2(0, 220), ImGuiChildFlags_Borders,
                                      ImGuiWindowFlags_HorizontalScrollbar);
                    for (int y = 0; y < h; ++y)
                        for (int x = 0; x < w; ++x) {
                            const auto index = static_cast<std::size_t>(y * w + x);
                            ImGui::PushID(static_cast<int>(index));
                            const auto gid = layer["data"].array()[index].integer();
                            const auto label = std::to_string(gid);
                            ImGui::Button(label.c_str(), ImVec2(27, 27));
                            if (ImGui::IsItemHovered() && ImGui::IsMouseDown(0))
                                layer["data"].array()[index] = ctx.tileGid_;
                            ImGui::PopID();
                            if (x + 1 < w)
                                ImGui::SameLine(0, 2);
                        }
                    ImGui::EndChild();
                } else
                    ImGui::TextUnformatted("Interactive paint supports maps up to 128 x 128.");
            }
        }
        if (ImGui::Button("Save tilemap"))
            ctx.attempt([&] {
                const auto path = ctx.project_->asset(ctx.tileAsset_, "tilemap").path;
                const auto old = Json::read(path);
                ctx.tileDocument_.save(path);
                try {
                    EditorActions::preview(ctx, ctx.scene_.document());
                } catch (...) {
                    old.save(path);
                    throw;
                }
                ctx.message_ = "TILEMAP SAVED";
            });
        ImGui::SameLine();
        if (ImGui::Button("Revert tilemap"))
            ctx.attempt([&] { ctx.tileDocument_ = Json::read(ctx.project_->asset(ctx.tileAsset_).path); });
    } else
        ImGui::TextWrapped(
            "Choose Paint tiles on a Tiled asset in Assets. Finite orthogonal JSON tile layers are supported.");
    ImGui::EndDisabled();
    ImGui::End();
}

void EditorChrome::settingsPanel([[maybe_unused]] EditorContext &ctx) {
    ImGui::Begin("Scene Settings");
    ImGui::BeginDisabled(ctx.player_ != nullptr);
    if (ctx.sceneSettingsText_.empty())
        ctx.sceneSettingsText_ = ctx.scene_.document().dump();
    ImGui::TextWrapped("Camera and tilemap entries are scene-wide. Edit the fields below then Apply. For object "
                       "animation, add Animator in Inspector; edit state, fps and frame UVs there.");
    // Keep a separate draft so incomplete edits do not replace the live scene.
    if (ctx.assetEditId_ != "settings") {
        ctx.assetEditId_ = "settings";
        ctx.assetText_ = ctx.scene_.document().dump();
    }
    Json settings = Json::parse(ctx.assetText_);
    for (const char *key : {"gravity", "camera", "tilemaps", "joints"})
        if (settings.contains(key))
            EditorWidgets::fields(ctx, settings[key], key);
    if (!settings.contains("camera") && ImGui::Button("Add Camera"))
        settings["camera"] = Json::parse(
            R"({"center":[0,0],"zoom":1,"pixelsPerUnit":64,"follow":"","bounds":[-1000,-1000,1000,1000],"smoothing":8,"pixelPerfect":false})");
    if (ImGui::Button("Add Tiled layer")) {
        if (!settings.contains("tilemaps"))
            settings["tilemaps"] = Json::Array{};
        settings["tilemaps"].array().push_back(
            Json::Object{{"asset", ""}, {"origin", Json::Array{0, 0}}, {"pixelsPerUnit", 64}, {"layer", -10}});
    }
    ctx.assetText_ = settings.dump();
    if (ImGui::Button("Apply scene settings"))
        ctx.attempt([&] {
            auto next = ctx.scene_.document();
            for (const char *key : {"gravity", "camera", "tilemaps", "joints"})
                if (settings.contains(key))
                    next[key] = settings.at(key);
            EditorActions::commit(ctx, std::move(next));
        });
    ImGui::SameLine();
    if (ImGui::Button("Reload fields"))
        ctx.assetText_ = ctx.scene_.document().dump();
    ImGui::EndDisabled();
    ImGui::End();
}

void EditorChrome::toolbar([[maybe_unused]] EditorContext &ctx) {
    if (!ImGui::BeginMainMenuBar())
        return;
    ImGui::BeginDisabled(ctx.player_ != nullptr);
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Build for Windows..."))
            ctx.windowsExport_->open();
        if (ImGui::MenuItem("Save scene", "Ctrl+S"))
            ctx.attempt([&] { EditorActions::save(ctx); });
        if (ImGui::MenuItem("Save scene as new asset..."))
            ImGui::OpenPopup("SaveAsMenu");
        if (ImGui::BeginPopup("SaveAsMenu")) {
            ImGui::InputText("Scene path", &ctx.saveName_);
            ImGui::InputText("New asset ID", &ctx.importId_);
            if (ImGui::Button("Save new scene"))
                ctx.attempt([&] {
                    const auto path = containedPath(ctx.root_, pathFromUtf8(ctx.saveName_));
                    if (path.extension() != ".json" || std::filesystem::exists(path))
                        throw std::runtime_error("Choose a new .json filename");
                    if (ctx.importId_.empty() || ctx.project_->assets().contains(ctx.importId_))
                        throw std::runtime_error("Choose a new asset ID");
                    std::filesystem::create_directories(path.parent_path());
                    ctx.scene_.document().save(path);
                    try {
                        EditorAssets::registerAsset(ctx, ctx.importId_, "scene", path);
                    } catch (...) {
                        std::filesystem::remove(path);
                        throw;
                    }
                    ctx.scenePath_ = path;
                    ctx.scene_.markSaved();
                    ctx.message_ = "NEW SCENE SAVED";
                    ImGui::CloseCurrentPopup();
                });
            ImGui::EndPopup();
        }
        if (ImGui::MenuItem("New empty scene"))
            ctx.attempt([&] {
                EditorActions::protect(ctx, [&ctx] {
                    auto next = Json::parse(
                        R"({"version":1,"id":"new-scene","gravity":[0,-9.81],"entities":[],"joints":[],"tilemaps":[]})");
                    next["id"] = newPersistentId();
                    EditorActions::commit(ctx, std::move(next));
                    ctx.scenePath_ = ctx.root_ / "scenes" / (newPersistentId() + ".scene.json");
                    std::filesystem::create_directories(ctx.scenePath_.parent_path());
                });
            });
        if (ImGui::MenuItem("Reload current scene"))
            ctx.attempt(
                [&] { EditorActions::protect(ctx, [&ctx] { EditorActions::loadScene(ctx, ctx.scenePath_); }); });
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Create")) {
        EditorWidgets::createObjectMenu(ctx);
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Undo", "Ctrl+Z", false, ctx.scene_.canUndo()))
        ctx.attempt([&] { EditorActions::history(ctx, true); });
    if (ImGui::MenuItem("Redo", "Ctrl+Y", false, ctx.scene_.canRedo()))
        ctx.attempt([&] { EditorActions::history(ctx, false); });
    ImGui::EndDisabled();
    ImGui::Dummy(ImVec2(12, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ctx.player_ ? ImVec4(.39F, .18F, .21F, 1) : ImVec4(.16F, .34F, .52F, 1));
    const bool togglePlay = ImGui::Button(ctx.player_ ? "Stop  [F5]" : "Play  [F5]", ImVec2(108, 0));
    ImGui::PopStyleColor();
    if (togglePlay)
        ctx.attempt([&] {
            if (ctx.player_)
                EditorActions::stop(ctx);
            else
                EditorActions::play(ctx);
        });
    ImGui::BeginDisabled(!ctx.player_);
    if (ImGui::MenuItem(ctx.paused_ ? "Resume" : "Pause")) {
        ctx.paused_ = !ctx.paused_;
        ctx.player_->setPaused(ctx.paused_);
    }
    if (ImGui::MenuItem("Step", nullptr, false, ctx.paused_))
        ctx.attempt([&] { ctx.player_->step(); });
    ImGui::EndDisabled();
    if (ImGui::MenuItem("Reset layout"))
        ctx.layout_ = false;
    ImGui::TextDisabled("  2Dog 0.6  |  %s%s", ctx.player_ ? (ctx.paused_ ? "PAUSED" : "PLAYING") : "EDITING",
                        ctx.scene_.dirty() ? "  /  Unsaved changes" : "");
    ImGui::EndMainMenuBar();
}

void EditorChrome::docking([[maybe_unused]] EditorContext &ctx) {
    const auto dock = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    if (ctx.layout_)
        return;
    ctx.layout_ = true;
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->WorkSize);
    ImGuiID center = dock;
    const auto left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, .18F, nullptr, &center);
    const auto right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, .29F, nullptr, &center);
    const auto bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, .28F, nullptr, &center);
    ImGuiID scene = center;
    const auto game = ImGui::DockBuilderSplitNode(scene, ImGuiDir_Right, .48F, nullptr, &scene);
    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Scene Settings", right);
    ImGui::DockBuilderDockWindow("Scene", scene);
    ImGui::DockBuilderDockWindow("Game", game);
    ImGui::DockBuilderDockWindow("Assets", bottom);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderDockWindow("Tilemap", bottom);
    ImGui::DockBuilderFinish(dock);
}

void EditorChrome::discardDialog([[maybe_unused]] EditorContext &ctx) {
    if (ctx.askDiscard_) {
        ImGui::OpenPopup("Unsaved scene");
        ctx.askDiscard_ = false;
    }
    if (ImGui::BeginPopupModal("Unsaved scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Save the current scene before continuing?");
        if (ImGui::Button("Save and continue"))
            ctx.attempt([&] {
                EditorActions::save(ctx);
                auto action = std::move(ctx.pending_);
                ImGui::CloseCurrentPopup();
                if (action)
                    action();
            });
        ImGui::SameLine();
        if (ImGui::Button("Discard")) {
            auto action = std::move(ctx.pending_);
            ImGui::CloseCurrentPopup();
            ctx.attempt([&] {
                if (action)
                    action();
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ctx.pending_ = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace twodog
