#pragma once
#include <cstdint>
#include <memory>
#include <twodog/platform/Window.hpp>
namespace twodog {
class RenderTarget final {
  public:
    explicit RenderTarget(Window &window);
    ~RenderTarget();
    void resize(int width, int height);
    void begin();
    void end();
    std::uint32_t texture() const;
    int width() const;
    int height() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace twodog
