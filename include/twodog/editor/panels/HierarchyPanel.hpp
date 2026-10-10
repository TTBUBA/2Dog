#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <twodog/data/Json.hpp>
namespace twodog {
struct EditorContext;
class HierarchyPanel final {
  public:
    static void draw([[maybe_unused]] EditorContext &ctx);
};
} // namespace twodog
