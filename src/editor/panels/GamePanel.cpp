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
#include <twodog/editor/panels/GamePanel.hpp>
#include <utility>
namespace twodog {
void GamePanel::draw([[maybe_unused]] EditorContext &ctx) {
    ctx.gameVisible_ = ImGui::Begin("Game");
    ctx.gameActive_ = false;
    if (ctx.gameVisible_) {
        ImGui::TextUnformatted(ctx.player_ ? (ctx.paused_ ? "PAUSED" : "PLAY - click the viewport for game input")
                                           : "Preview - press Play to run C# and physics");
        ImGui::SetNextItemWidth(180);
        const char *resolutionsLabels[] = {"Free aspect", "1280 x 720", "1920 x 1080", "720 x 1280", "1024 x 768"};
        ImGui::Combo("Resolution", &ctx.gameResolution_, resolutionsLabels, IM_ARRAYSIZE(resolutionsLabels));
        ctx.gameOrigin_ = ImGui::GetCursorScreenPos();
        ctx.gameSize_ = ImGui::GetContentRegionAvail();
        ctx.gameSize_.x = std::max(1.0F, ctx.gameSize_.x);
        ctx.gameSize_.y = std::max(1.0F, ctx.gameSize_.y);
        const auto scale = ImGui::GetIO().DisplayFramebufferScale;
        const glm::ivec2 resolutions[] = {{0, 0}, {1280, 720}, {1920, 1080}, {720, 1280}, {1024, 768}};
        if (ctx.gameResolution_ == 0)
            ctx.gameTarget_.resize(static_cast<int>(ctx.gameSize_.x * scale.x),
                                   static_cast<int>(ctx.gameSize_.y * scale.y));
        else {
            const auto resolution = resolutions[ctx.gameResolution_];
            ctx.gameTarget_.resize(resolution.x, resolution.y);
            const float fit = std::min(ctx.gameSize_.x / resolution.x, ctx.gameSize_.y / resolution.y);
            ctx.gameSize_ = ImVec2(resolution.x * fit, resolution.y * fit);
        }
        ImGui::Image(static_cast<ImTextureID>(ctx.gameTarget_.texture()), ctx.gameSize_, ImVec2(0, 1), ImVec2(1, 0));
        ctx.gameActive_ =
            ctx.player_ && ImGui::IsItemHovered() && ImGui::IsWindowFocused() && !ImGui::GetIO().WantTextInput;
    }
    ImGui::End();
}

void GamePanel::render(EditorContext &ctx, double alpha) {
    if (ctx.gameVisible_) {
        ctx.gameTarget_.begin();
        ctx.attempt([&] {
            if (ctx.player_)
                ctx.player_->render(alpha);
            else {
                const auto old = ctx.view_->camera;
                if (ctx.scene_.document().contains("camera"))
                    ctx.view_->configure(ctx.scene_.document().at("camera"));
                else
                    ctx.view_->camera = Camera2D{};
                ctx.view_->camera.width = ctx.gameTarget_.width();
                ctx.view_->camera.height = ctx.gameTarget_.height();
                ctx.view_->draw(*ctx.preview_, ctx.renderer_, 1);
                ctx.gameUI_.draw(*ctx.preview_, *ctx.assets_, ctx.renderer_, ctx.gameTarget_.width(),
                                 ctx.gameTarget_.height());
                ctx.view_->camera = old;
            }
        });
        ctx.gameTarget_.end();
    }
}
} // namespace twodog
