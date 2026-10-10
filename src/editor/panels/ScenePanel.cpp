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
#include <twodog/editor/panels/ScenePanel.hpp>
#include <utility>
namespace twodog {
glm::vec2 ScenePanel::scenePoint([[maybe_unused]] EditorContext &ctx, ImVec2 p) {
    return {
        (p.x - ctx.sceneOrigin_.x) * static_cast<float>(ctx.sceneTarget_.width()) / std::max(1.0F, ctx.sceneSize_.x),
        (p.y - ctx.sceneOrigin_.y) * static_cast<float>(ctx.sceneTarget_.height()) / std::max(1.0F, ctx.sceneSize_.y)};
}

ImVec2 ScenePanel::sceneScreen([[maybe_unused]] EditorContext &ctx, glm::vec2 world) {
    const auto p = ctx.camera_.worldToScreen(world);
    return {ctx.sceneOrigin_.x + p.x * ctx.sceneSize_.x / static_cast<float>(ctx.sceneTarget_.width()),
            ctx.sceneOrigin_.y + p.y * ctx.sceneSize_.y / static_cast<float>(ctx.sceneTarget_.height())};
}

void ScenePanel::selectionOutline([[maybe_unused]] EditorContext &ctx) {
    if (ctx.scene_.selected().empty() || !ctx.preview_->alive(ctx.scene_.selected()))
        return;
    auto *draw = ImGui::GetWindowDrawList();
    const auto *e = SceneDocument::find(ctx.scene_.document(), ctx.scene_.selected());
    if (ctx.showUI_ && e && e->contains("ui")) {
        for (const auto &item :
             GameUI::layout(ctx.preview_->snapshot(), ctx.sceneTarget_.width(), ctx.sceneTarget_.height()))
            if (item.id == ctx.scene_.selected()) {
                const auto &r = item.rect;
                const float sx = ctx.sceneSize_.x / static_cast<float>(ctx.sceneTarget_.width()),
                            sy = ctx.sceneSize_.y / static_cast<float>(ctx.sceneTarget_.height());
                draw->AddRect(ImVec2(ctx.sceneOrigin_.x + r.x * sx, ctx.sceneOrigin_.y + r.y * sy),
                              ImVec2(ctx.sceneOrigin_.x + (r.x + r.w) * sx, ctx.sceneOrigin_.y + (r.y + r.h) * sy),
                              IM_COL32(70, 235, 175, 255), 0, 0, 2);
            }
        return;
    }
    const auto t = ctx.preview_->transform(ctx.scene_.selected());
    ImVec2 corners[4];
    const glm::vec2 local[4] = {{-.5F, -.5F}, {.5F, -.5F}, {.5F, .5F}, {-.5F, .5F}};
    const float c = std::cos(t.rotation), s = std::sin(t.rotation);
    for (int i = 0; i < 4; ++i) {
        const auto v = local[i] * t.size;
        corners[i] = ScenePanel::sceneScreen(ctx, t.position + glm::vec2(c * v.x - s * v.y, s * v.x + c * v.y));
    }
    draw->AddPolyline(corners, 4, IM_COL32(70, 235, 175, 255), ImDrawFlags_Closed, 2);
    const auto center = ScenePanel::sceneScreen(ctx, t.position);
    if (ctx.tool_ == 0) {
        draw->AddLine(center, ImVec2(center.x + 40, center.y), IM_COL32(250, 100, 100, 255), 3);
        draw->AddLine(center, ImVec2(center.x, center.y - 40), IM_COL32(100, 250, 130, 255), 3);
    } else if (ctx.tool_ == 1)
        draw->AddCircle(center, 40, IM_COL32(250, 195, 75, 255), 40, 2);
    else
        draw->AddRectFilled(ImVec2(corners[2].x - 4, corners[2].y - 4), ImVec2(corners[2].x + 4, corners[2].y + 4),
                            IM_COL32(250, 195, 75, 255));
}

void ScenePanel::draw([[maybe_unused]] EditorContext &ctx) {
    ctx.sceneVisible_ = ImGui::Begin("Scene");
    if (ctx.sceneVisible_) {
        ImGui::RadioButton("Move (W)", &ctx.tool_, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Rotate (E)", &ctx.tool_, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Scale (R)", &ctx.tool_, 2);
        ImGui::Checkbox("Snap", &ctx.snap_);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(75);
        ImGui::DragFloat("Grid", &ctx.snapSize_, .05F, .05F, 10, "%.2f");
        ctx.snapSize_ = std::clamp(ctx.snapSize_, .05F, 10.0F);
        ImGui::SameLine();
        ImGui::Checkbox("UI overlay", &ctx.showUI_);
        ImGui::SameLine();
        if (ImGui::SmallButton("Frame selected") && !ctx.scene_.selected().empty() &&
            ctx.preview_->alive(ctx.scene_.selected()))
            ctx.camera_.center = ctx.preview_->transform(ctx.scene_.selected()).position;
        ctx.sceneOrigin_ = ImGui::GetCursorScreenPos();
        ctx.sceneSize_ = ImGui::GetContentRegionAvail();
        ctx.sceneSize_.x = std::max(1.0F, ctx.sceneSize_.x);
        ctx.sceneSize_.y = std::max(1.0F, ctx.sceneSize_.y);
        const auto scale = ImGui::GetIO().DisplayFramebufferScale;
        ctx.sceneTarget_.resize(static_cast<int>(ctx.sceneSize_.x * scale.x),
                                static_cast<int>(ctx.sceneSize_.y * scale.y));
        ctx.camera_.width = ctx.sceneTarget_.width();
        ctx.camera_.height = ctx.sceneTarget_.height();
        ImGui::Image(static_cast<ImTextureID>(ctx.sceneTarget_.texture()), ctx.sceneSize_, ImVec2(0, 1), ImVec2(1, 0));
        const bool hovered = ImGui::IsItemHovered();
        auto &io = ImGui::GetIO();
        const auto mouse = ScenePanel::scenePoint(ctx, io.MousePos);
        if (hovered) {
            const auto before = ctx.camera_.screenToWorld(mouse);
            ctx.camera_.zoom = std::clamp(ctx.camera_.zoom * std::pow(1.1F, io.MouseWheel), .05F, 20.0F);
            ctx.camera_.center += before - ctx.camera_.screenToWorld(mouse);
            if (ImGui::IsMouseDragging(2))
                ctx.camera_.center +=
                    glm::vec2(-io.MouseDelta.x * static_cast<float>(ctx.camera_.width) / ctx.sceneSize_.x,
                              io.MouseDelta.y * static_cast<float>(ctx.camera_.height) / ctx.sceneSize_.y) /
                    (ctx.camera_.pixelsPerUnit * ctx.camera_.zoom);
            if (!io.WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_W))
                    ctx.tool_ = 0;
                if (ImGui::IsKeyPressed(ImGuiKey_E))
                    ctx.tool_ = 1;
                if (ImGui::IsKeyPressed(ImGuiKey_R))
                    ctx.tool_ = 2;
            }
        }
        if (!ctx.player_ && hovered && ImGui::IsMouseClicked(0)) {
            std::string pick;
            int topLayer = -2147483647;
            const auto world = ctx.camera_.screenToWorld(mouse);
            for (const auto &e : ctx.scene_.document().at("entities").array()) {
                if (e.contains("ui"))
                    continue;
                const auto t = readTransform(e.at("transform"));
                const auto delta = world - t.position;
                const float c = std::cos(-t.rotation), s = std::sin(-t.rotation);
                const glm::vec2 local{c * delta.x - s * delta.y, s * delta.x + c * delta.y};
                const int layer = e.contains("sprite") ? static_cast<int>(e.at("sprite").num("layer")) : 0;
                if (std::abs(local.x) <= t.size.x * .5F && std::abs(local.y) <= t.size.y * .5F && layer >= topLayer) {
                    pick = e.at("id").string();
                    topLayer = layer;
                }
            }
            if (ctx.showUI_)
                for (const auto &item :
                     GameUI::layout(ctx.scene_.document(), ctx.sceneTarget_.width(), ctx.sceneTarget_.height())) {
                    const auto &r = item.rect;
                    if (item.data.text("kind") != "Canvas" && mouse.x >= r.x && mouse.y >= r.y && mouse.x < r.x + r.w &&
                        mouse.y < r.y + r.h)
                        pick = item.id;
                }
            ctx.scene_.select(pick);
            ctx.dragging_ =
                !ctx.scene_.selected().empty() &&
                !(SceneDocument::find(ctx.scene_.document(), ctx.scene_.selected())->contains("ui") &&
                  SceneDocument::find(ctx.scene_.document(), ctx.scene_.selected())->at("ui").text("kind") == "Canvas");
            ctx.dragMouse_ = mouse;
            if (ctx.dragging_)
                ctx.dragStart_ = ctx.preview_->transform(ctx.scene_.selected());
        }
        if (ctx.dragging_ && !ctx.player_ && ImGui::IsMouseDragging(0)) {
            const auto *original = SceneDocument::find(ctx.scene_.document(), ctx.scene_.selected());
            if (original && original->contains("ui")) {
                float scaleUI = 1;
                for (const auto &item : GameUI::layout(ctx.scene_.document(), ctx.camera_.width, ctx.camera_.height))
                    if (item.id == ctx.scene_.selected())
                        scaleUI = item.scale;
                const auto offset = jsonVec2(original->at("ui").at("offset")) + (mouse - ctx.dragMouse_) / scaleUI;
                ctx.scene_.draft()["ui"]["offset"] = toJson(offset);
                ctx.preview_->get(ctx.scene_.selected()).document["ui"] = ctx.scene_.draft().at("ui");
            } else if (original) {
                const auto delta = ctx.camera_.screenToWorld(mouse) - ctx.camera_.screenToWorld(ctx.dragMouse_);
                auto t = ctx.dragStart_;
                if (ctx.tool_ == 0) {
                    t.position += delta;
                    if (ctx.snap_)
                        t.position = glm::round(t.position / ctx.snapSize_) * ctx.snapSize_;
                }
                if (ctx.tool_ == 1) {
                    const auto a = ctx.camera_.screenToWorld(ctx.dragMouse_) - t.position,
                               b = ctx.camera_.screenToWorld(mouse) - t.position;
                    t.rotation += std::atan2(b.y, b.x) - std::atan2(a.y, a.x);
                    if (ctx.snap_)
                        t.rotation = std::round(t.rotation / .2617994F) * .2617994F;
                }
                if (ctx.tool_ == 2) {
                    t.size = glm::max(glm::vec2(.02F), t.size + glm::vec2(delta.x, delta.y) * 2.0F);
                    if (ctx.snap_)
                        t.size = glm::max(glm::vec2(ctx.snapSize_), glm::round(t.size / ctx.snapSize_) * ctx.snapSize_);
                }
                ctx.preview_->transform(ctx.scene_.selected(), t);
                ctx.scene_.draft()["transform"] = writeTransform(t);
            }
        }
        if (ctx.dragging_ && ImGui::IsMouseReleased(0)) {
            ctx.dragging_ = false;
            if (ImGui::GetMouseDragDelta(0).x != 0 || ImGui::GetMouseDragDelta(0).y != 0)
                ctx.attempt([&] { ctx.scene_.replaceEntity(ctx.scene_.selected(), ctx.scene_.draft()); });
        }
        auto *draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(ctx.sceneOrigin_,
                           ImVec2(ctx.sceneOrigin_.x + ctx.sceneSize_.x, ctx.sceneOrigin_.y + ctx.sceneSize_.y), true);
        ScenePanel::selectionOutline(ctx);
        draw->PopClipRect();
    }
    ImGui::End();
}

void ScenePanel::render(EditorContext &ctx) {
    if (ctx.sceneVisible_) {
        ctx.sceneTarget_.begin();
        ctx.view_->camera = ctx.camera_;
        std::vector<SpriteDraw> grid;
        const auto tl = ctx.camera_.screenToWorld({0, 0}),
                   br = ctx.camera_.screenToWorld(
                       {static_cast<float>(ctx.camera_.width), static_cast<float>(ctx.camera_.height)});
        const float spacing =
            std::max(ctx.snapSize_, std::pow(10.0F, std::floor(std::log10(std::max(
                                                        .01F, 20 / (ctx.camera_.pixelsPerUnit * ctx.camera_.zoom))))));
        const float thickness = 1 / (ctx.camera_.pixelsPerUnit * ctx.camera_.zoom);
        for (float x = std::floor(tl.x / spacing) * spacing; x < br.x && grid.size() < 400; x += spacing) {
            SpriteDraw d;
            d.transform.position = {x, (tl.y + br.y) * .5F};
            d.transform.size = {thickness, tl.y - br.y};
            d.sprite.color = {.12F, .17F, .23F, 1};
            grid.push_back(d);
        }
        for (float y = std::floor(br.y / spacing) * spacing; y < tl.y && grid.size() < 800; y += spacing) {
            SpriteDraw d;
            d.transform.position = {(tl.x + br.x) * .5F, y};
            d.transform.size = {br.x - tl.x, thickness};
            d.sprite.color = {.12F, .17F, .23F, 1};
            grid.push_back(d);
        }
        ctx.renderer_.draw(ctx.camera_, grid);
        ctx.attempt([&] {
            ctx.view_->draw(*ctx.preview_, ctx.renderer_, 1);
            if (ctx.showUI_)
                ctx.gameUI_.draw(*ctx.preview_, *ctx.assets_, ctx.renderer_, ctx.camera_.width, ctx.camera_.height);
        });
        ctx.sceneTarget_.end();
    }
}
} // namespace twodog
