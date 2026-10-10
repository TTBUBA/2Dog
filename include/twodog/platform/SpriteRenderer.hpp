#pragma once
#include <twodog/gameplay/Scene.hpp>
#include <twodog/platform/Image.hpp>
#include <twodog/platform/Window.hpp>
#include <filesystem>
#include <memory>
#include <span>

namespace twodog {
struct SpriteDraw {Transform2D transform;Sprite sprite;};
struct RenderStats {std::size_t sprites=0,drawCalls=0;};
class SpriteRenderer final {
public:
    explicit SpriteRenderer(Window& window,std::size_t batchCapacity=1024);
    ~SpriteRenderer();
    SpriteRenderer(const SpriteRenderer&)=delete;
    SpriteRenderer& operator=(const SpriteRenderer&)=delete;
    std::uint32_t createTexture(const Image& image);
    std::uint32_t loadTexture(const std::filesystem::path& path);
    void replaceTexture(std::uint32_t texture,const Image& image);
    void destroyTexture(std::uint32_t texture);
    std::uint32_t nativeTexture(std::uint32_t texture) const;
    RenderStats draw(const Camera2D& camera,std::span<const SpriteDraw> sprites);
    // Both operations perform the OpenGL read on the owning context thread.
    Image capture() const;
    void savePPM(const std::filesystem::path& path) const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
