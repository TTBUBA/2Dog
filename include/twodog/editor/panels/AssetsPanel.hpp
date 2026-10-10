#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <twodog/data/Json.hpp>
namespace twodog {
struct EditorContext;
class AssetsPanel final {
  public:
    static void folderTree([[maybe_unused]] EditorContext &ctx, const std::filesystem::path &directory, int depth = 0);
    static void importDialog([[maybe_unused]] EditorContext &ctx);
    static void requestAssetCreation([[maybe_unused]] EditorContext &ctx, int kind,
                                     const std::filesystem::path &folder);
    static void assetCreationMenu([[maybe_unused]] EditorContext &ctx, const std::filesystem::path &folder);
    static void assetBrowser([[maybe_unused]] EditorContext &ctx);
    static void draw([[maybe_unused]] EditorContext &ctx);
};
} // namespace twodog
