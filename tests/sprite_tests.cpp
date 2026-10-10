#include <twodog/platform/SpriteRenderer.hpp>
#include <twodog/platform/OpenGL.hpp>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace twodog;
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
void pixel(const Image& i,int x,int y,std::array<int,3> expected) {
    const auto index=static_cast<std::size_t>(y*i.width+x)*4;
    for(int c=0;c<3;++c)require(std::abs(static_cast<int>(i.rgba[index+static_cast<std::size_t>(c)])-expected[static_cast<std::size_t>(c)])<=3,"Framebuffer pixel mismatch");
}
SpriteDraw quad(glm::vec4 color){SpriteDraw d;d.transform.size={2,2};d.sprite.color=color;return d;}
template<class F> void rejects(F f){try{f();}catch(const std::invalid_argument&){return;}throw std::runtime_error("Invalid renderer input accepted");}
}
int main() {
    try {
        WindowConfig config;config.width=128;config.height=128;config.visible=false;config.vsync=false;
        Window window(config);OpenGL gl(window);SpriteRenderer renderer(window,4);
        Camera2D camera;camera.width=128;camera.height=128;camera.pixelsPerUnit=32;
        std::vector<SpriteDraw> d{quad({1,0,0,1})};gl.clear(0,0,0);renderer.draw(camera,d);
        pixel(renderer.capture(),64,64,{255,0,0});pixel(renderer.capture(),4,4,{0,0,0});
        // Submission order differs from layer order. Alpha requires back-to-front order.
        d={quad({0,0,1,0.5F}),quad({1,0,0,1})};d[0].sprite.layer=1;
        gl.clear(0,0,0);renderer.draw(camera,d);pixel(renderer.capture(),64,64,{128,0,128});
        Image corners{2,2,{255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255}};
        const auto texture=renderer.createTexture(corners);d={quad({1,1,1,1})};d[0].sprite.texture=texture;
        gl.clear(0,0,0);renderer.draw(camera,d);auto image=renderer.capture();
        pixel(image,48,48,{255,0,0});pixel(image,80,48,{0,255,0});pixel(image,48,80,{0,0,255});pixel(image,80,80,{255,255,0});
        d[0].sprite.uv={0.5F,0,1,0.5F};renderer.draw(camera,d);pixel(renderer.capture(),64,64,{0,255,0});
        d.assign(11,quad({1,1,1,1}));const auto stats=renderer.draw(camera,d);require(stats.sprites==11&&stats.drawCalls==3,"Batch capacity split");
        d.assign(3,quad({1,1,1,1}));d[0].sprite.texture=texture;d[2].sprite.texture=texture;
        require(renderer.draw(camera,d).drawCalls==3,"Texture runs must retain order");
        renderer.replaceTexture(texture,{1,1,{0,255,255,255}});d={quad({1,1,1,1})};d[0].sprite.texture=texture;
        renderer.draw(camera,d);pixel(renderer.capture(),64,64,{0,255,255});
        rejects([&]{renderer.replaceTexture(texture,{1,1,{0}});});renderer.draw(camera,d);pixel(renderer.capture(),64,64,{0,255,255});
        renderer.destroyTexture(texture);rejects([&]{renderer.draw(camera,d);});
        // Non-square rotated sprite and translated camera exercise CPU transform + projection.
        d={quad({1,0,0,1})};d[0].transform.position={1,0};d[0].transform.size={2,0.5F};d[0].transform.rotation=1.57079632679F;camera.center={1,0};
        gl.clear(0,0,0);renderer.draw(camera,d);image=renderer.capture();pixel(image,64,86,{255,0,0});pixel(image,86,64,{0,0,0});
        gl.checkError("Sprite suite");
        std::cout<<"PASS sprite pixels, alpha/layer order, UV atlas, batch splits, texture lifetime, replacement, camera and rotation\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL graphics: "<<e.what()<<'\n';return 1;}
}
