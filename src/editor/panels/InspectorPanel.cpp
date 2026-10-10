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
#include <twodog/editor/panels/InspectorPanel.hpp>
#include <utility>
namespace twodog {
void InspectorPanel::uiInspector([[maybe_unused]] EditorContext &ctx, Json &u) {
    const auto kind = u.text("kind");
    ImGui::TextDisabled("%s / Screen Space Overlay", kind.c_str());
    EditorWidgets::fields(ctx, u["visible"], "visible");
    if (kind == "Canvas") {
        ImGui::SeparatorText("Canvas Scaler");
        ImGui::TextUnformatted("Reference resolution (X / Y)");
        auto reference = jsonVec2(u.at("referenceResolution"));
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat2("##ReferenceResolution", &reference.x, 1, 1, 16384, "%.0f"))
            u["referenceResolution"] = toJson(reference);
        float match = static_cast<float>(u.num("match", .5));
        if (ImGui::SliderFloat("Width / Height", &match, 0, 1, "%.2f"))
            u["match"] = match;
        ImGui::TextWrapped(
            "0 = match width, 1 = match height. The Canvas fills the game viewport; children inherit its scale.");
        return;
    }
    ImGui::SeparatorText("Rect Transform");
    EditorWidgets::fields(ctx, u["parent"], "parent");
    if (ImGui::Button("Anchor presets", ImVec2(-1, 30)))
        ImGui::OpenPopup("Anchors");
    if (ImGui::BeginPopup("Anchors")) {
        const char *rows[] = {"Top", "Middle", "Bottom"};
        const char *cols[] = {"Left", "Center", "Right"};
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                if (x)
                    ImGui::SameLine();
                if (ImGui::Button((std::string(rows[y]) + " " + cols[x]).c_str(), ImVec2(110, 30))) {
                    u["anchorMin"] = u["anchorMax"] = u["pivot"] = Json::Array{x * .5, y * .5};
                    u["offset"] = Json::Array{0, 0};
                    auto size = jsonVec2(u.at("size"));
                    u["size"] = toJson(glm::max(size, glm::vec2(48)));
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        for (const char *mode : {"Stretch X", "Stretch Y", "Stretch XY"}) {
            if (ImGui::MenuItem(mode)) {
                const bool x = std::string(mode) != "Stretch Y", y = std::string(mode) != "Stretch X";
                u["anchorMin"] = Json::Array{x ? 0.0 : .5, y ? 0.0 : .5};
                u["anchorMax"] = Json::Array{x ? 1.0 : .5, y ? 1.0 : .5};
                u["pivot"] = Json::Array{.5, .5};
                u["offset"] = Json::Array{0, 0};
                u["size"] = Json::Array{x ? 0 : 240, y ? 0 : 48};
            }
        }
        ImGui::TextWrapped(
            "Presets reset offset/pivot. Stretched size is a delta: -40 adds 20px margins on both sides.");
        ImGui::EndPopup();
    }
    for (const char *key : {"anchorMin", "anchorMax", "pivot", "offset", "size"})
        EditorWidgets::fields(ctx, u[key], key);
    ImGui::SeparatorText("Appearance & Interaction");
    for (auto &[key, value] : u.object()) {
        if (key == "kind" || key == "parent" || key == "visible" || key == "anchorMin" || key == "anchorMax" ||
            key == "pivot" || key == "offset" || key == "size")
            continue;
        if ((key == "texture" && kind != "Image") || (key == "value" && kind != "Slider") ||
            ((key == "padding" || key == "spacing" || key == "direction") && kind != "Layout"))
            continue;
        EditorWidgets::fields(ctx, value, key);
    }
}

void InspectorPanel::draw([[maybe_unused]] EditorContext &ctx) {
    ImGui::Begin("Inspector");
    ImGui::BeginDisabled(ctx.player_ != nullptr);
    if (!ctx.scene_.selected().empty()) {
        ImGui::Button("Drop a C# script here", ImVec2(-1, 34));
        EditorWidgets::scriptDropTarget(ctx, ctx.scene_.selected());
        EditorWidgets::applyScriptDrop(ctx);
    }
    if (!ctx.scene_.draft().null()) {
        ImGui::TextWrapped("ID: %s", ctx.scene_.selected().c_str());
        EditorWidgets::fields(ctx, ctx.scene_.draft()["name"], "Name");
        std::string erase;
        for (auto &[key, value] : ctx.scene_.draft().object()) {
            if (key == "id" || key == "name")
                continue;
            ImGui::PushID(key.c_str());
            ImGui::Dummy(ImVec2(0, 10));
            ImGui::BeginChild("Component card", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
            const auto label = key == "transform" ? std::string("Transform")
                : key == "ui" ? "UI / " + value.text("kind")
                : key == "scripts" ? std::string("C# Behaviours")
                : ComponentSchema::label(key);
            const bool expanded = ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            if (expanded) {
                ImGui::Spacing();
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * .58F);
                if (key == "ui")
                    InspectorPanel::uiInspector(ctx, value);
                else if (value.isObject())
                    for (auto &[fieldName, child] : value.object())
                        EditorWidgets::fields(ctx, child, fieldName);
                else
                    EditorWidgets::fields(ctx, value, key);
                ImGui::PopItemWidth();
                if (key != "transform") {
                    ImGui::Spacing();
                    if (ImGui::SmallButton("Remove component"))
                        erase = key;
                }
            }
            ImGui::EndChild();
            ImGui::PopID();
        }
        if (!erase.empty())
            ctx.scene_.draft().object().erase(erase);
        ImGui::Dummy(ImVec2(0, 14));
        if (ImGui::Button("+ Add Component", ImVec2(-1, 38)))
            ImGui::OpenPopup("Components");
        if (ImGui::BeginPopup("Components")) {
            for(const auto& component:ComponentSchema::components()) {
                if(std::string_view(component.key)=="ui") continue;
                if(!ctx.scene_.draft().contains(component.key) && ImGui::MenuItem(component.label))
                    ctx.scene_.draft()[component.key]=component.defaults();
            }
            for(const char* shape:{"box","circle","polygon"})
                if(ctx.scene_.draft().contains("body") && ImGui::MenuItem((std::string("Add ")+shape+" collider").c_str()))
                    ctx.scene_.draft()["body"]["colliders"].array().push_back(ComponentSchema::colliderDefaults(shape));
            for(const char* kind:{"Canvas","Panel","Label","Image","Button","Slider","Layout"})
                if(!ctx.scene_.draft().contains("ui") && ImGui::MenuItem((std::string("UI / ")+kind).c_str()))
                    ctx.scene_.draft()["ui"]=ComponentSchema::uiDefaults(kind);
            ImGui::EndPopup();
        }
        ImGui::SeparatorText("C# Behaviour");
        ImGui::InputText("Type", &ctx.scriptType_);
        EditorWidgets::picker(ctx, "type", ctx.scriptType_);
        if (ImGui::Button("Attach C# script")) {
            if (!ctx.scene_.draft().contains("scripts"))
                ctx.scene_.draft()["scripts"] = Json::Array{};
            ctx.attempt([&] {
                ctx.scene_.draft()["scripts"].array().push_back(SceneDocument::scriptBinding(ctx.scriptType_));
            });
        }
        ImGui::Separator();
        if (ImGui::Button("Apply components"))
            ctx.attempt([&] {
                ctx.scene_.replaceEntity(ctx.scene_.selected(), ctx.scene_.draft());
                ctx.message_ = "COMPONENTS APPLIED";
            });
        ImGui::SameLine();
        if (ImGui::Button("Revert fields"))
            EditorActions::refreshSelection(ctx);
        ImGui::TextWrapped("Apply commits all fields. Invalid values keep the current scene. Scene coordinates use "
                           "world units and radians; UI offsets use reference pixels.");
    } else
        ImGui::TextWrapped("Select an object in Hierarchy or Scene. Create objects from the Create menu.");
    ImGui::EndDisabled();
    ImGui::End();
}

} // namespace twodog
