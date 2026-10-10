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
#include <twodog/editor/panels/HierarchyPanel.hpp>
#include <utility>
namespace twodog {
void HierarchyPanel::draw([[maybe_unused]] EditorContext &ctx) {
    ImGui::Begin("Hierarchy");
    ImGui::BeginDisabled(ctx.player_ != nullptr);
    if (ImGui::Button("+ Create", ImVec2(-1, 32)))
        ImGui::OpenPopup("Create object");
    if (ImGui::BeginPopup("Create object")) {
        EditorWidgets::createObjectMenu(ctx);
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopupContextWindow("Hierarchy context",
                                       ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        EditorWidgets::createObjectMenu(ctx);
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();
    ImGui::Text("%zu objects%s", ctx.scene_.document().at("entities").array().size(), ctx.scene_.dirty() ? " *" : "");
    ImGui::InputText("Search", &ctx.filter_);
    ImGui::BeginDisabled(ctx.player_ != nullptr);
    bool objectContext = false;
    const auto &objects = ctx.scene_.document().at("entities").array();
    std::function<void(const std::string &, int)> drawChildren;
    drawChildren = [&](const std::string &parentId, int depth) {
        if (depth > 64)
            return;
        for (const auto &e : objects) {
            const auto id = e.at("id").string(), name = e.text("name", id);
            const auto parent = e.contains("ui") ? e.at("ui").text("parent") : "";
            if (ctx.filter_.empty() ? parent != parentId : name.find(ctx.filter_) == std::string::npos)
                continue;
            const bool hasChildren =
                ctx.filter_.empty() && std::any_of(objects.begin(), objects.end(), [&](const Json &child) {
                    return child.contains("ui") && child.at("ui").text("parent") == id;
                });
            const auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_DefaultOpen |
                               (ctx.scene_.selected() == id ? ImGuiTreeNodeFlags_Selected : 0) |
                               (hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf);
            const bool open = ImGui::TreeNodeEx((name + "##" + id).c_str(), flags);
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                ctx.scene_.select(id);
            }
            EditorWidgets::scriptDropTarget(ctx, id);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                ctx.scene_.select(id);
                objectContext = true;
            }
            if (open) {
                if (hasChildren)
                    drawChildren(id, depth + 1);
                ImGui::TreePop();
            }
        }
    };
    drawChildren("", 0);
    if (objectContext)
        ImGui::OpenPopup("Create object");
    EditorWidgets::applyScriptDrop(ctx);
    if (ImGui::Button("Duplicate"))
        ctx.attempt([&] { EditorActions::duplicate(ctx); });
    ImGui::SameLine();
    if (ImGui::Button("Delete"))
        ImGui::OpenPopup("Delete object?");
    if (ImGui::BeginPopup("Delete object?")) {
        ImGui::TextUnformatted("Delete selected object and its joints?");
        if (ImGui::Button("Delete now")) {
            ctx.attempt([&] { EditorActions::remove(ctx); });
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();
    ImGui::End();
}

} // namespace twodog
