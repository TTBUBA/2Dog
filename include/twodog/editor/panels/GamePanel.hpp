#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <twodog/data/Json.hpp>
namespace twodog {
struct EditorContext;
class GamePanel final {
  public:
    static void render(EditorContext &ctx, double alpha);
    static void draw([[maybe_unused]] EditorContext &ctx);
};
} // namespace twodog
