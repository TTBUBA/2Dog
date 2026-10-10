#pragma once
#include <twodog/platform/Window.hpp>
#include <array>
#include <memory>
#include <string>

namespace twodog {
// Per-context function table. This phase needs only six core GL entry points.
// Window must outlive this object. All calls remain on the context's thread.
class OpenGL final {
public:
    explicit OpenGL(Window& window);
    ~OpenGL();
    OpenGL(const OpenGL&) = delete;
    OpenGL& operator=(const OpenGL&) = delete;
    void clear(float red, float green, float blue, float alpha = 1.0F);
    [[nodiscard]] std::array<unsigned char, 4> readCenterPixel();
    [[nodiscard]] const std::string& version() const noexcept;
    void checkError(const char* operation) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
