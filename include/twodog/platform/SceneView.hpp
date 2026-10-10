#pragma once
#include <twodog/gameplay/World2D.hpp>
#include <twodog/platform/Font.hpp>
#include <random>
namespace twodog {
class VisualAssets final {
public:
    VisualAssets(const Project& project,SpriteRenderer& renderer);
    ~VisualAssets();
    VisualAssets(const VisualAssets&)=delete;
    VisualAssets& operator=(const VisualAssets&)=delete;
    std::uint32_t texture(std::string_view id) const;
    glm::ivec2 textureSize(std::string_view id) const;
    std::uint32_t texturePath(const std::filesystem::path& path) const;
    const Font& font(std::string_view id) const;
    void poll();
private:
    struct Texture {std::uint32_t handle;std::filesystem::path path;std::filesystem::file_time_type stamp;glm::ivec2 size{1,1};};
    const Project& project_;SpriteRenderer& renderer_;Font bitmap_;
    std::map<std::string,Texture,std::less<>> textures_;
    struct FontAsset {Font font;std::filesystem::path path;std::filesystem::file_time_type stamp;};
    std::map<std::string,FontAsset,std::less<>> fonts_;
};
class SceneView final {
public:
    SceneView(const Project& project,VisualAssets& assets,const Json& scene);
    void tick(World2D& world,double dt);
    void draw(World2D& world,SpriteRenderer& renderer,float alpha);
    void setState(World2D& world,std::string_view entity,std::string_view state);
    void emit(glm::vec2 position,int count,glm::vec4 color);
    void configure(const Json& request);
    Json cameraDocument() const;
    glm::vec2 pointer(glm::vec2 pixels) const;
    Camera2D camera;
private:
    struct Tile {SpriteDraw draw;glm::vec2 parallax{1};};
    struct Animation {std::string state;double clock=0;};
    struct Particle {glm::vec2 position,velocity;glm::vec4 color;float life=1;};
    const Project& project_;VisualAssets& assets_;
    std::vector<Tile> tiles_;
    std::map<std::string,Animation,std::less<>> animations_;
    std::vector<Particle> particles_;
    std::minstd_rand random_{1729};
    std::string follow_;
    glm::vec4 bounds_{-100000,-100000,100000,100000};
    glm::vec2 shakeOffset_{0};float shake_=0,smoothing_=8;
    bool pixelPerfect_=false;
    void importMap(const Json& component);
};
}
