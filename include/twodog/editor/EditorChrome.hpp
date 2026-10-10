#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <twodog/data/Json.hpp>
namespace twodog {
struct EditorContext;
class EditorChrome final {
  public:
    static void tileEditor([[maybe_unused]] EditorContext &ctx);
    static void settingsPanel([[maybe_unused]] EditorContext &ctx);
    static void toolbar([[maybe_unused]] EditorContext &ctx);
    static void docking([[maybe_unused]] EditorContext &ctx);
    static void discardDialog([[maybe_unused]] EditorContext &ctx);
};
} // namespace twodog
