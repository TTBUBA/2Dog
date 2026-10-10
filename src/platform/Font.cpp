#include <twodog/platform/Font.hpp>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#define STB_TRUETYPE_IMPLEMENTATION
#ifdef _MSC_VER
#pragma warning(push,0)
#endif
#include <stb_truetype.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
namespace twodog {
    namespace {
        Image fontImage() {
            Image image{96,48,std::vector<unsigned char>(96*48*4,0)};
            // Original compact bitmap alphabet. Text is a demo overlay, not the phase-6 UI system.
            struct Glyph {char c;
                std::array<unsigned char,7> rows;
            };
            constexpr Glyph glyphs[]={
                {'#',{10,31,10,10,31,10,0}},
                {'A',{14,17,17,31,17,17,17}},{'B',{30,17,17,30,17,17,30}},{'C',{14,17,16,16,16,17,14}},
                {'D',{30,17,17,17,17,17,30}},{'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},
                {'G',{14,17,16,23,17,17,14}},{'H',{17,17,17,31,17,17,17}},{'I',{14,4,4,4,4,4,14}},
                {'J',{7,2,2,2,2,18,12}},{'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},
                {'M',{17,27,21,21,17,17,17}},{'N',{17,25,21,19,17,17,17}},{'O',{14,17,17,17,17,17,14}},
                {'P',{30,17,17,30,16,16,16}},{'Q',{14,17,17,17,21,18,13}},{'R',{30,17,17,30,20,18,17}},
                {'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},{'U',{17,17,17,17,17,17,14}},
                {'V',{17,17,17,17,17,10,4}},{'W',{17,17,17,21,21,21,10}},{'X',{17,17,10,4,10,17,17}},
                {'Y',{17,17,10,4,4,4,4}},{'Z',{31,1,2,4,8,16,31}},
                {'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},{'2',{14,17,1,2,4,8,31}},
                {'3',{30,1,1,14,1,1,30}},{'4',{2,6,10,18,31,2,2}},{'5',{31,16,16,30,1,1,30}},
                {'6',{14,16,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},{'8',{14,17,17,14,17,17,14}},
                {'9',{14,17,17,15,1,1,14}},{'-',{0,0,0,31,0,0,0}},{'/',{1,2,2,4,8,8,16}},
                {'.',{0,0,0,0,0,6,6}},{':',{0,6,6,0,6,6,0}},{'+',{0,4,4,31,4,4,0}},
                {'[',{14,8,8,8,8,8,14}},{']',{14,2,2,2,2,2,14}},{'(',{2,4,8,8,8,4,2}},{')',{8,4,2,2,2,4,8}}
            };
            for(const auto& g:glyphs) {
                const int slot=g.c-32,col=slot%16,row=slot/16;
                for(int y=0;y<7;++y) for(int x=0;x<5;++x) if((g.rows[static_cast<std::size_t>(y)]&(1<<(4-x)))!=0) {
                    const auto p=static_cast<std::size_t>((47-(row*8+y))*96+col*6+x)*4;
                    for(int c=0;c<4;++c) image.rgba[p+static_cast<std::size_t>(c)]=255;
                }
            }
            return image;
        }
    }
    Font Font::bitmap(SpriteRenderer& renderer) {
        Font result;
        result.texture_=renderer.createTexture(fontImage());
        for(int i=0;i<96;++i) {const int col=i%16,row=i/16;
            auto& g=result.glyphs_[static_cast<std::size_t>(i)];
            g.uv={static_cast<float>(col*6)/96,static_cast<float>(48-(row+1)*8)/48,static_cast<float>((col+1)*6)/96,static_cast<float>(48-row*8)/48};
            g.size={6,8};
            g.advance=6;
        }
        return result;
    }
    Font Font::trueType(SpriteRenderer& renderer,const std::filesystem::path& path,float pixelHeight) {
        if(pixelHeight<6||pixelHeight>96) throw std::invalid_argument("Font height must be 6..96 pixels");
        const auto size=std::filesystem::file_size(path);
        if(size<12||size>16*1024*1024) throw std::invalid_argument("Invalid font size");
        std::ifstream file(path,std::ios::binary);
        std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
        file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(size));
        if(!file) throw std::runtime_error("Cannot read font");
        stbtt_fontinfo info{};
        const int offset=stbtt_GetFontOffsetForIndex(bytes.data(),0);
        if(offset<0||!stbtt_InitFont(&info,bytes.data(),offset)) throw std::invalid_argument("Invalid TTF font");
        constexpr int width=1024,height=1024;
        std::vector<unsigned char> bitmap(width*height);
        stbtt_bakedchar chars[96]{};
        if(stbtt_BakeFontBitmap(bytes.data(),offset,pixelHeight,bitmap.data(),width,height,32,96,chars)<=0) throw std::invalid_argument("Font atlas is full");
        Image image{width,height,std::vector<unsigned char>(width*height*4)};
        for(int y=0;y<height;++y) for(int x=0;x<width;++x) {const auto dst=static_cast<std::size_t>(((height-1-y)*width+x)*4);
            image.rgba[dst]=image.rgba[dst+1]=image.rgba[dst+2]=255;
            image.rgba[dst+3]=bitmap[static_cast<std::size_t>(y*width+x)];
        }
        Font result;
        result.texture_=renderer.createTexture(image);
        result.lineHeight_=pixelHeight*1.3F;
        for(std::size_t i=0;i<96;++i) {const auto& c=chars[i];
            auto& g=result.glyphs_[i];
            g.uv={static_cast<float>(c.x0)/width,1-static_cast<float>(c.y1)/height,static_cast<float>(c.x1)/width,1-static_cast<float>(c.y0)/height};
            g.offset={c.xoff,c.yoff};
            g.size={static_cast<float>(c.x1-c.x0),static_cast<float>(c.y1-c.y0)};
            g.advance=c.xadvance;
        }
        return result;
    }
    void Font::append(std::vector<SpriteDraw>& draws,std::string_view text,glm::vec2 baseline,float scale,glm::vec4 color,int layer) const {
        const auto left=baseline.x;
        for(unsigned char c:text) {
            if(c=='\n') {baseline.x=left;
                baseline.y-=lineHeight_*scale;
                continue;
            }
            if(c<32||c>127) c='?';
            const auto& g=glyphs_[static_cast<std::size_t>(c-32)];
            if(c!=' ') {SpriteDraw draw;
                draw.transform.position={baseline.x+(g.offset.x+g.size.x*.5F)*scale,baseline.y-(g.offset.y+g.size.y*.5F)*scale};
                draw.transform.size=g.size*scale;
                draw.sprite.texture=texture_;
                draw.sprite.uv=g.uv;
                draw.sprite.color=color;
                draw.sprite.layer=layer;
                draws.push_back(draw);
            }
            baseline.x+=g.advance*scale;
        }
    }
}
