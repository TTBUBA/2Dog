#pragma once
#include <filesystem>
#include <functional>
#include <memory>
namespace twodog {
class WindowsExport final {
public:
    WindowsExport(const std::filesystem::path& project, const std::filesystem::path& support);
    ~WindowsExport();
    void open();
    void draw(bool canBuild, const std::function<void()>& saveScene);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
