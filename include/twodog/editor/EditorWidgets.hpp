#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <twodog/data/Json.hpp>
namespace twodog {
struct EditorContext;
class EditorWidgets final {
  public:
    static void picker([[maybe_unused]] EditorContext &ctx, const std::string &key, std::string &value);
    static void fields([[maybe_unused]] EditorContext &ctx, Json &v, const std::string &key, int depth = 0);
    static void createObjectMenu([[maybe_unused]] EditorContext &ctx);
    static void scriptDropTarget([[maybe_unused]] EditorContext &ctx, const std::string &id);
    static void applyScriptDrop([[maybe_unused]] EditorContext &ctx);
};
} // namespace twodog
