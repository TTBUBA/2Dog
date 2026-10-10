#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <twodog/data/Json.hpp>
namespace twodog {
struct EditorContext;
class EditorAssets final {
  public:
    static void reloadProject([[maybe_unused]] EditorContext &ctx);
    static void registerAsset([[maybe_unused]] EditorContext &ctx, const std::string &id, const std::string &type,
                              const std::filesystem::path &path);
    static std::string assetKind([[maybe_unused]] EditorContext &ctx, const std::filesystem::path &path);
    static std::string assetID([[maybe_unused]] EditorContext &ctx, const std::filesystem::path &path);
    static std::filesystem::path writableAssetFolder([[maybe_unused]] EditorContext &ctx);
    static void refreshBrowser([[maybe_unused]] EditorContext &ctx);
    static void openAsset([[maybe_unused]] EditorContext &ctx, const std::filesystem::path &path);
};
} // namespace twodog
