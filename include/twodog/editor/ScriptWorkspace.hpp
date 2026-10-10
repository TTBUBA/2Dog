#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace twodog {
class ScriptWorkspace final {
  public:
    ScriptWorkspace(const std::filesystem::path &root, const std::filesystem::path &managed,
                    const std::filesystem::path &support);
    ~ScriptWorkspace();
    void tick(double now, bool forceScan = false);
    void rebuild();
    void refreshIDE();
    bool ready() const;
    bool busy() const;
    const std::string &status() const;
    std::filesystem::path assembly() const;
    std::filesystem::path create(const std::filesystem::path &folder, const std::string &name);
    std::vector<std::string> types(const std::filesystem::path &file) const;
    void open(const std::filesystem::path &file);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace twodog
