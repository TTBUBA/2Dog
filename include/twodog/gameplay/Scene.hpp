#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <compare>
#include <optional>
#include <vector>

namespace twodog {
struct Entity { std::uint32_t index = 0; std::uint32_t generation = 0; auto operator<=>(const Entity&) const = default; };
struct Transform2D { glm::vec2 position{0}; glm::vec2 size{1}; float rotation = 0; };
struct Sprite {
    std::uint32_t texture = 0; // 0 is the renderer's white texture
    glm::vec4 color{1};
    glm::vec4 uv{0, 0, 1, 1}; // bottom-left, top-right, in OpenGL texture coordinates
    int layer = 0;
    float depth = 0;
};
struct SceneObject { Transform2D transform; std::optional<Sprite> sprite; };
// Handles retain a generation, so deleting an object cannot alias its replacement.
class Scene final {
public:
    Entity create();
    bool alive(Entity entity) const noexcept;
    void destroy(Entity entity);
    SceneObject& get(Entity entity);
    const SceneObject& get(Entity entity) const;
    std::vector<Entity> entities() const;
private:
    struct Slot { std::uint32_t generation = 1; bool live = false; SceneObject object; };
    std::vector<Slot> slots_;
};
struct Camera2D {
    glm::vec2 center{0};
    float zoom = 1;
    float pixelsPerUnit = 64;
    int width = 1280, height = 720;
    void validate() const;
    glm::mat4 projection() const;
    glm::vec2 screenToWorld(glm::vec2 pixels) const; // top-left screen coordinates
    glm::vec2 worldToScreen(glm::vec2 world) const;
};
class SpriteAnimation final {
public:
    SpriteAnimation(std::vector<glm::vec4> frames, double fps, bool loop = true);
    void advance(double seconds);
    void reset() noexcept;
    glm::vec4 uv() const;
    std::size_t frame() const noexcept { return frame_; }
    bool finished() const noexcept { return finished_; }
private:
    std::vector<glm::vec4> frames_;
    double fps_, time_ = 0;
    std::size_t frame_ = 0;
    bool loop_, finished_ = false;
};
}
