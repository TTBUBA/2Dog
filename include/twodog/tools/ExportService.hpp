#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace twodog {
struct ExportSettings {
    std::string output, name = "Game", scene, runtime;
};
struct ExportStatus {
    std::string status = "Ready", details;
    std::filesystem::path log;
};
// Owns job launch, persistence and monitoring. No editor, window or ImGui dependency.
class ExportService final {
  public:
    ExportService(const std::filesystem::path &project, const std::filesystem::path &support);
    ~ExportService();
    ExportSettings &settings();
    const ExportStatus &state() const;
    bool busy() const;
    void poll();
    void start(bool run);
    void saveSettings();
    std::vector<std::string> scenes() const;
    std::filesystem::path logDirectory() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace twodog
