#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <twodog/data/Json.hpp>
namespace twodog {
struct EditorContext;
class EditorActions final {
  public:
    static void refreshSelection([[maybe_unused]] EditorContext &ctx);
    static void preview([[maybe_unused]] EditorContext &ctx, const Json &doc);
    static void commit([[maybe_unused]] EditorContext &ctx, Json next, bool record = true);
    static void loadScene([[maybe_unused]] EditorContext &ctx, const std::filesystem::path &path);
    static void save([[maybe_unused]] EditorContext &ctx);
    static void protect([[maybe_unused]] EditorContext &ctx, std::function<void()> action);
    static void play([[maybe_unused]] EditorContext &ctx);
    static void stop([[maybe_unused]] EditorContext &ctx);
    static void history([[maybe_unused]] EditorContext &ctx, bool back);
    static void create([[maybe_unused]] EditorContext &ctx, const std::string &kind);
    static void duplicate([[maybe_unused]] EditorContext &ctx);
    static void remove([[maybe_unused]] EditorContext &ctx);
    static void attachScript([[maybe_unused]] EditorContext &ctx, const std::filesystem::path &path,
                             const std::string &target);
};
} // namespace twodog
