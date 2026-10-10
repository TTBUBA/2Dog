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
void EditorWidgets::picker([[maybe_unused]] EditorContext &ctx, const std::string &key, std::string &value) {
    const bool asset = key == "texture" || key == "font" || key == "asset";
    const bool reference = key == "parent" || key == "target" || key == "follow" || key == "a" || key == "b";
    if (!asset && !reference && key != "type")
        return;
    ImGui::SameLine();
    if (ImGui::SmallButton("..."))
        ImGui::OpenPopup("Choose");
    if (ImGui::BeginPopup("Choose")) {
        if (ImGui::Selectable("(none)"))
            value = "";
        if (key == "font" && ImGui::Selectable("bitmap"))
            value = "bitmap";
        if (asset)
            for (const auto &[id, a] : ctx.project_->assets()) {
                if ((key == "texture" && a.type != "texture") || (key == "font" && a.type != "font"))
                    continue;
                if (ImGui::Selectable(id.c_str()))
                    value = id;
            }
        if (reference)
            for (const auto &e : ctx.scene_.document().at("entities").array()) {
                const auto id = e.at("id").string();
                if (key == "parent" && (id == ctx.scene_.selected() || !e.contains("ui")))
                    continue;
                if (ImGui::Selectable((e.text("name") + "##" + id).c_str()))
                    value = id;
            }
        if (key == "type") {
            for (const char *type : {"static", "dynamic", "kinematic"})
                if (ImGui::Selectable(type))
                    value = type;
            for (const auto &file : ctx.assetFiles_)
                if (file.extension() == ".cs")
                    for (const auto &type : ctx.scripts_->types(file))
                        if (ImGui::Selectable(type.c_str()))
                            value = type;
            if (ctx.project_->settings().contains("scriptTypes"))
                for (const auto &type : ctx.project_->settings().at("scriptTypes").array())
                    if (ImGui::Selectable(type.string().c_str()))
                        value = type.string();
        }
        ImGui::EndPopup();
    }
}

void EditorWidgets::fields([[maybe_unused]] EditorContext &ctx, Json &v, const std::string &key, int depth) {
    if (depth > 12) {
        ImGui::TextUnformatted("Maximum inspector depth reached");
        return;
    }
    ImGui::PushID(key.c_str());
    if (v.isBoolean()) {
        bool b = v.boolean();
        if (ImGui::Checkbox(key.c_str(), &b))
            v = b;
    } else if (v.isNumber()) {
        double n = v.number();
        if (ImGui::InputDouble(key.c_str(), &n, 0, 0, "%.4f")) {
            if (std::isfinite(n))
                v = n;
        }
    } else if (v.isString()) {
        std::string text = v.string();
        ImGui::SetNextItemWidth(-65);
        ImGui::InputText(key.c_str(), &text);
        EditorWidgets::picker(ctx, key, text);
        v = std::move(text);
    } else if (v.isArray()) {
        auto &a = v.array();
        bool numeric = (a.size() == 2 || a.size() == 4) &&
                       std::all_of(a.begin(), a.end(), [](const Json &x) { return x.isNumber(); });
        if (numeric) {
            float n[4]{};
            for (std::size_t i = 0; i < a.size(); ++i)
                n[i] = static_cast<float>(a[i].number());
            const bool color = key == "color" || key == "textColor";
            const bool changed = a.size() == 2 ? ImGui::DragFloat2(key.c_str(), n, .02F)
                                 : color       ? ImGui::ColorEdit4(key.c_str(), n)
                                               : ImGui::DragFloat4(key.c_str(), n, .01F);
            if (changed)
                for (std::size_t i = 0; i < a.size(); ++i)
                    if (std::isfinite(n[i]))
                        a[i] = n[i];
        } else if (ImGui::TreeNodeEx(key.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            std::size_t removeAt = a.size();
            for (std::size_t i = 0; i < a.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::SmallButton("Remove item"))
                    removeAt = i;
                EditorWidgets::fields(ctx, a[i], "Item " + std::to_string(i), depth + 1);
                ImGui::PopID();
            }
            if (removeAt < a.size())
                a.erase(a.begin() + static_cast<std::ptrdiff_t>(removeAt));
            if (!a.empty() && ImGui::SmallButton("Duplicate last item")) {
                auto copy = a.back();
                if (copy.isObject() && copy.contains("slot"))
                    copy["slot"] = newPersistentId();
                a.push_back(std::move(copy));
            }
            ImGui::TreePop();
        }
    } else if (v.isObject() && ImGui::TreeNodeEx(key.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto &[name, child] : v.object())
            EditorWidgets::fields(ctx, child, name, depth + 1);
        if (key == "properties" || key == "states") {
            ImGui::InputText("New key", &ctx.propertyName_);
            if (!ctx.propertyName_.empty() && !v.contains(ctx.propertyName_)) {
                if (ImGui::SmallButton("Add number"))
                    v[ctx.propertyName_] = 0;
                ImGui::SameLine();
                if (ImGui::SmallButton("Add text"))
                    v[ctx.propertyName_] = "";
                ImGui::SameLine();
                if (ImGui::SmallButton("Add bool"))
                    v[ctx.propertyName_] = false;
                if (key == "states" && ImGui::SmallButton("Add animation state"))
                    v[ctx.propertyName_] = Json::parse(R"({"fps":8,"loop":true,"frames":[[0,0,1,1]]})");
            }
        }
        ImGui::TreePop();
    } else if (v.null())
        ImGui::Text("%s: null", key.c_str());
    ImGui::PopID();
}

void EditorWidgets::createObjectMenu([[maybe_unused]] EditorContext &ctx) {
    for (const char *kind : {"Empty", "Sprite", "Canvas", "Panel", "Label", "Image", "Button", "Slider", "Layout"})
        if (ImGui::MenuItem(std::string(kind) == "Label" ? "Text (Label)" : kind))
            ctx.attempt([&] { EditorActions::create(ctx, kind); });
}

void EditorWidgets::scriptDropTarget([[maybe_unused]] EditorContext &ctx, const std::string &id) {
    if (!ctx.player_ && ImGui::BeginDragDropTarget()) {
        if (const auto *payload = ImGui::AcceptDragDropPayload("TWODOG_SCRIPT")) {
            ctx.attachFile_ = static_cast<const char *>(payload->Data);
            ctx.attachEntity_ = id;
        }
        ImGui::EndDragDropTarget();
    }
}

void EditorWidgets::applyScriptDrop([[maybe_unused]] EditorContext &ctx) {
    if (ctx.attachFile_.empty())
        return;
    const auto file = std::exchange(ctx.attachFile_, {}), id = std::exchange(ctx.attachEntity_, {});
    ctx.attempt([&] { EditorActions::attachScript(ctx, containedPath(ctx.root_, pathFromUtf8(file)), id); });
}

} // namespace twodog
