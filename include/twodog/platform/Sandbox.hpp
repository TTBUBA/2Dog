#pragma once
#include <twodog/platform/Window.hpp>
#include <filesystem>
#include <memory>
#include <glm/glm.hpp>
namespace twodog {
struct SandboxStatus {std::size_t bodyCount;glm::vec2 playerPosition;bool paused;};
class Sandbox final {
public:
    Sandbox(Window& window,const std::filesystem::path& assets);
    ~Sandbox();
    void poll();
    void fixedUpdate(double dt);
    void render(double alpha);
    void capture(const std::filesystem::path& path) const;
    SandboxStatus status() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
