#pragma once
#include <filesystem>
#include <functional>
#include <glm/vec2.hpp>
#include <imgui.h>
#include <string>
#include <twodog/data/Json.hpp>
namespace twodog {
struct EditorContext;
class ScenePanel final {
  public:
    static void render(EditorContext &ctx);
    static glm::vec2 scenePoint([[maybe_unused]] EditorContext &ctx, ImVec2 p);
    static ImVec2 sceneScreen([[maybe_unused]] EditorContext &ctx, glm::vec2 world);
    static void selectionOutline([[maybe_unused]] EditorContext &ctx);
    static void draw([[maybe_unused]] EditorContext &ctx);
};
} // namespace twodog
