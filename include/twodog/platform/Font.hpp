#pragma once
#include <twodog/platform/SpriteRenderer.hpp>
#include <array>
namespace twodog {
class Font final {
public:
    static Font bitmap(SpriteRenderer& renderer);
    static Font trueType(SpriteRenderer& renderer,const std::filesystem::path& path,float pixelHeight=28);
    std::uint32_t texture() const {return texture_;}
    void append(std::vector<SpriteDraw>& draws,std::string_view text,glm::vec2 baseline,float scale,glm::vec4 color,int layer=100) const;
private:
    struct Glyph {glm::vec4 uv{0};glm::vec2 offset{0},size{0};float advance=0;};
    std::uint32_t texture_=0;std::array<Glyph,96> glyphs_{};float lineHeight_=8;
};
}
