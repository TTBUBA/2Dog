#pragma once
#include <filesystem>
namespace twodog {
// Authored content stays at root/assets; all regenerable development output stays under .twodog.
struct ProjectLayout final {
    std::filesystem::path root;
    explicit ProjectLayout(const std::filesystem::path &project) : root(std::filesystem::canonical(project)) {}
    std::filesystem::path development() const { return root / ".twodog"; }
    std::filesystem::path cache() const { return development() / "cache"; }
    std::filesystem::path settings() const { return development() / "settings"; }
    std::filesystem::path logs() const { return development() / "logs"; }
    std::filesystem::path scriptBuild() const { return cache() / "scripts"; }
    std::filesystem::path exportWork() const { return cache() / "exports"; }
    std::filesystem::path exportLogs() const { return logs() / "exports"; }
};
} // namespace twodog
