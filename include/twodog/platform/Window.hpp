#pragma once
#include <twodog/core/WindowConfig.hpp>
#include <memory>
#include <string>

namespace twodog {
class Input;
// One engine-owned GLFW window. All operations and destruction are main-thread only.
// No implicit copying or context movement. Platform headers remain private.
class Window final {
public:
    using Proc = void (*)();
    explicit Window(const WindowConfig& config);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;
    void pollEvents();
    void waitEvents(double timeoutSeconds);
    [[nodiscard]] bool shouldClose() const;
    void requestClose();
    void cancelClose();
    void setEscapeCloses(bool enabled);
    void* nativeHandle() const;
    [[nodiscard]] bool suspended() const;
    [[nodiscard]] FramebufferSize framebufferSize() const;
    void resize(int width, int height);
    void present();
    void setTitle(const std::string& title);
    [[nodiscard]] Proc procAddress(const char* name) const;
    [[nodiscard]] double nowSeconds() const;
    void requireCurrentContext() const;
    Input& input(); // Available with TWODOG_BUILD_GAMEPLAY
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
