#pragma once
#include <twodog/platform/Window.hpp>
#include <filesystem>
#include <twodog/data/Json.hpp>
#include <memory>
namespace twodog {
class ProjectPlayer final {
  public:
    ProjectPlayer(Window &window, const std::filesystem::path &project, const std::filesystem::path &managed,
                  bool scripts = true, const Json *initialScene = nullptr, bool embedded = false,
                  const std::filesystem::path &gameAssembly = {});
    ~ProjectPlayer();
    void poll(bool shortcuts = true, bool acceptInput = true);
    void setViewport(int width, int height);
    void setPaused(bool paused);
    void step();
    void fixedUpdate(double dt);
    void update(double dt);
    void render(double alpha);
    void capture(const std::filesystem::path &path);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace twodog
