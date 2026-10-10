#pragma once
#include <functional>
#include <twodog/platform/SceneView.hpp>

namespace twodog {
// Runtime UI is rendered by the engine, independently of Dear ImGui.
class GameUI final {
  public:
    struct Rect {
        float x = 0, y = 0, w = 0, h = 0;
    };
    struct Item {
        std::string id;
        Rect rect;
        Json data;
        float scale = 1; // Pixels per reference unit, inherited from the root Canvas.
    };
    static void validate(const Json &entity);
    static std::vector<Item> layout(const Json &scene, int width, int height);
    void draw(World2D &world, VisualAssets &assets, SpriteRenderer &renderer, int width, int height);
    void poll(World2D &world, Window &window, int width, int height, const std::function<void(Json)> &send,
              bool acceptInput = true);
    void reset();
    bool capturesMouse() const { return mouseCapture_; }
    bool capturesKeyboard() const { return !focus_.empty(); }

  private:
    std::string hovered_, pressed_, focus_;
    bool mouseCapture_ = false;
};
} // namespace twodog
