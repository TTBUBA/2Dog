#include <twodog/platform/Sandbox.hpp>
#include <twodog/platform/SpriteRenderer.hpp>
#include <twodog/gameplay/Physics.hpp>
#include <twodog/gameplay/Input.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>

namespace twodog {
namespace {
Image fontImage() {
    Image image{96,48,std::vector<unsigned char>(96*48*4,0)};
    // Original compact bitmap alphabet. Text is a demo overlay, not the phase-6 UI system.
    struct Glyph {char c;std::array<unsigned char,7> rows;};
    constexpr Glyph glyphs[]={
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
SpriteDraw quad(glm::vec2 pos,glm::vec2 size,glm::vec4 color,int layer=0) {
    SpriteDraw d;d.transform.position=pos;d.transform.size=size;d.sprite.color=color;d.sprite.layer=layer;return d;
}
}
struct Sandbox::Impl {
    struct Object {Entity entity;BodyHandle body;};
    Window& window;SpriteRenderer renderer;Scene scene;std::unique_ptr<Physics> physics;
    Camera2D camera;std::vector<Object> objects;BodyHandle player,anchor,pendulum;
    std::uint32_t atlas=0,font=0;std::filesystem::path atlasPath;std::filesystem::file_time_type writeTime{};
    SpriteAnimation animation{{{0,0,0.25F,1},{0.25F,0,0.5F,1},{0.5F,0,0.75F,1},{0.75F,0,1,1}},6};
    bool paused=false,singleStep=false;std::size_t contacts=0;double reloadClock=0;std::string assetStatus="ATLAS READY";
    explicit Impl(Window& w,const std::filesystem::path& assets):window(w),renderer(w),atlasPath(assets/"demo_atlas.tga") {
        atlas=renderer.loadTexture(atlasPath);font=renderer.createTexture(fontImage());writeTime=std::filesystem::last_write_time(atlasPath);
        auto& input=window.input();
        input.bind("left",{{InputDevice::Keyboard,65},{InputDevice::Keyboard,263},{InputDevice::Gamepad,14}});
        input.bind("right",{{InputDevice::Keyboard,68},{InputDevice::Keyboard,262},{InputDevice::Gamepad,12}});
        input.bind("jump",{{InputDevice::Keyboard,32},{InputDevice::Keyboard,87},{InputDevice::Gamepad,0}});
        input.bind("spawn",{{InputDevice::Mouse,0}});input.bind("reset",{{InputDevice::Keyboard,82}});
        input.bind("pause",{{InputDevice::Keyboard,80}});input.bind("step",{{InputDevice::Keyboard,78}});
        reset();
    }
    BodyHandle add(BodyConfig config,glm::vec4 color,int tile=0) {
        const auto body=physics->create(config);const auto entity=scene.create();
        auto& object=scene.get(entity);object.transform.position=config.position;object.transform.rotation=config.angle;
        object.transform.size=config.collider==ColliderType::Circle?glm::vec2(config.radius*2):config.halfSize*2.0F;
        Sprite sprite;sprite.texture=atlas;sprite.color=color;sprite.uv={static_cast<float>(tile)*0.25F,0,static_cast<float>(tile+1)*0.25F,1};sprite.layer=2;
        if(config.type==BodyType::Static&&!config.sensor) {sprite.texture=0;sprite.layer=1;}
        object.sprite=sprite;objects.push_back({entity,body});return body;
    }
    void reset() {
        physics=std::make_unique<Physics>();scene=Scene{};objects.clear();animation.reset();contacts=0;
        BodyConfig c;c.type=BodyType::Static;c.position={0,-3.8F};c.halfSize={12,0.25F};add(c,{0.14F,0.25F,0.32F,1});
        c.position={-10,1};c.halfSize={0.2F,5};add(c,{0.14F,0.25F,0.32F,1});c.position={10,1};add(c,{0.14F,0.25F,0.32F,1});
        c.position={-5,-1.8F};c.halfSize={2,0.15F};add(c,{0.24F,0.36F,0.42F,1});
        c.position={3,-0.6F};c.halfSize={1.5F,0.15F};c.angle=-0.22F;add(c,{0.24F,0.36F,0.42F,1});
        c={};c.position={-7,-2.8F};c.halfSize={0.35F,0.45F};c.fixedRotation=true;c.restitution=0;player=add(c,{0.35F,1,0.78F,1});
        for(int i=0;i<12;++i) {
            c={};c.position={-2.5F+static_cast<float>(i%4)*1.15F,0.4F+static_cast<float>(i/4)*1.0F};c.halfSize={0.35F,0.35F};
            c.angle=static_cast<float>(i)*0.13F;c.restitution=0.4F;
            if(i%3==0) {c.collider=ColliderType::Circle;c.radius=0.35F;}
            add(c,i%2?glm::vec4{0.64F,0.54F,1,1}:glm::vec4{1,0.7F,0.32F,1},c.collider==ColliderType::Circle?1:0);
        }
        c={};c.type=BodyType::Static;c.position={7,3};c.halfSize={0.1F,0.1F};anchor=add(c,{0.4F,0.7F,1,1});
        c={};c.position={8.4F,1.6F};c.collider=ColliderType::Circle;c.radius=0.4F;pendulum=add(c,{1,0.4F,0.53F,1},1);
        physics->distanceJoint(anchor,pendulum,2.0F);
        c={};c.type=BodyType::Static;c.sensor=true;c.position={6,-3};c.halfSize={1.3F,0.5F};add(c,{0.3F,0.9F,0.8F,0.22F});
        c={};c.position={-5,2.8F};c.collider=ColliderType::Polygon;c.vertices={{-0.45F,-0.4F},{0.45F,-0.4F},{0,0.4F}};
        c.halfSize={0.45F,0.4F};add(c,{0.4F,0.74F,1,1},2);
    }
    void poll() {
        auto& input=window.input();const auto size=window.framebufferSize();
        camera.width=std::max(1,size.width);camera.height=std::max(1,size.height);
        camera.pixelsPerUnit=std::max(1.0F,64.0F*static_cast<float>(camera.height)/720.0F);
        camera.zoom=std::clamp(camera.zoom*std::pow(1.1F,static_cast<float>(input.scrollY)),0.5F,2.0F);
        if(input.action("reset").pressed) reset();
        if(input.action("pause").pressed) paused=!paused;
        singleStep=singleStep||input.action("step").pressed;
        if(input.action("spawn").pressed&&objects.size()<250) {
            const auto p=camera.screenToWorld({static_cast<float>(input.cursorX),static_cast<float>(input.cursorY)});
            if(input.cursorY>100&&input.cursorY<camera.height-90) {BodyConfig c;c.position=p;c.halfSize={0.3F,0.3F};c.restitution=0.5F;add(c,{1,0.78F,0.3F,1});}
        }
        const auto now=window.nowSeconds();
        if(now>reloadClock) {
            reloadClock=now+0.5;
            try {
                const auto stamp=std::filesystem::last_write_time(atlasPath);
                if(stamp!=writeTime) {renderer.replaceTexture(atlas,loadImage(atlasPath));writeTime=stamp;assetStatus="ATLAS RELOADED";}
            } catch(const std::exception&) {assetStatus="ATLAS ERROR - OLD KEPT";}
        }
    }
    void fixed(double dt) {
        const auto input=window.input().consumeFixed();
        if(paused&&!singleStep) return;
        singleStep=false;
        float direction=(input.at("right").down?1.0F:0.0F)-(input.at("left").down?1.0F:0.0F);
        if(std::abs(window.input().axis(0))>0.18F) direction=window.input().axis(0);
        const auto pose=physics->pose(player);auto velocity=physics->velocity(player);velocity.x=direction*4.5F;
        // Ignore the player's own shapes explicitly, including rounded Box2D skins.
        const auto ground=physics->raycast(pose.position,{0,-0.58F},~std::uint64_t{0},player);
        if(input.at("jump").pressed&&ground&&ground->normal.y>.5F) velocity.y=6;
        physics->setVelocity(player,velocity);physics->step(static_cast<float>(dt));animation.advance(dt);
        for(const auto& event:physics->events()) if(event.kind==PhysicsEvent::Kind::ContactBegin) ++contacts;
        if(physics->pose(player).position.y<-12) reset();
    }
    void text(std::vector<SpriteDraw>& draws,const std::string& str,float x,float y,float scale,glm::vec4 color) {
        for(const char c:str) {
            const int code=static_cast<unsigned char>(c)-32;
            if(code>=0&&code<96&&c!=' ') {
                auto d=quad({x+3*scale,static_cast<float>(camera.height)-y-4*scale},{6*scale,8*scale},color,20);
                d.sprite.texture=font;const int col=code%16,row=code/16;
                d.sprite.uv={static_cast<float>(col*6)/96,static_cast<float>(48-(row+1)*8)/48,static_cast<float>((col+1)*6)/96,static_cast<float>(48-row*8)/48};draws.push_back(d);
            }
            x+=6*scale;
        }
    }
    void render(double alpha) {
        std::vector<SpriteDraw> draws;
        for(int x=-20;x<=20;++x) draws.push_back(quad({static_cast<float>(x),0},{0.012F,30},{0.12F,0.17F,0.23F,1},-10));
        for(int y=-12;y<=12;++y) draws.push_back(quad({0,static_cast<float>(y)},{40,0.012F},{0.12F,0.17F,0.23F,1},-10));
        for(const auto& object:objects) {
            auto& data=scene.get(object.entity);const auto pose=physics->pose(object.body,paused?1.0F:static_cast<float>(alpha));
            data.transform.position=pose.position;data.transform.rotation=pose.angle;
            draws.push_back({data.transform,*data.sprite});
        }
        const auto a=physics->pose(anchor).position,b=physics->pose(pendulum,paused?1.0F:static_cast<float>(alpha)).position;
        auto rope=quad((a+b)*0.5F,{glm::length(b-a),0.035F},{0.52F,0.69F,0.85F,1},1);rope.transform.rotation=std::atan2(b.y-a.y,b.x-a.x);draws.push_back(rope);
        // The spinning atlas card exercises all four UV regions independently of physics.
        auto card=quad({-8,3},{0.9F,0.9F},{1,1,1,1},3);card.sprite.texture=atlas;card.sprite.uv=animation.uv();draws.push_back(card);
        const auto stats=renderer.draw(camera,draws);
        Camera2D ui;ui.width=camera.width;ui.height=camera.height;ui.center={static_cast<float>(ui.width)*0.5F,static_cast<float>(ui.height)*0.5F};ui.pixelsPerUnit=1;
        draws.clear();const float width=static_cast<float>(ui.width),height=static_cast<float>(ui.height);
        draws.push_back(quad({width/2,height-43},{width,86},{0.055F,0.075F,0.11F,0.97F},10));
        draws.push_back(quad({width/2,40},{width,80},{0.055F,0.075F,0.11F,0.97F},10));
        draws.push_back(quad({width/2,height-86},{width,2},{0.25F,0.9F,0.7F,1},11));
        text(draws,"2DOG / PLAYGROUND",24,18,3,{0.4F,1,0.79F,1});
        text(draws,"SPRITES + INPUT + BOX2D",24,53,1.5F,{0.62F,0.7F,0.8F,1});
        if(width>850) {text(draws,paused?"[ PAUSED ]":"[ RUNNING ]",width-250,20,2,{1,0.78F,0.35F,1});text(draws,assetStatus,width-290,51,1.3F,{0.64F,0.74F,0.87F,1});}
        text(draws,"A/D OR ARROWS: MOVE   SPACE: JUMP   CLICK: SPAWN   WHEEL: ZOOM",24,height-63,1.5F,{0.91F,0.94F,1,1});
        text(draws,"P: PAUSE   N: STEP   R: RESET   ESC: EXIT",24,height-37,1.5F,{0.6F,0.72F,0.86F,1});
        if(width>1100) text(draws,"BODIES "+std::to_string(physics->bodyCount())+" / DRAWS "+std::to_string(stats.drawCalls),width-340,height-37,1.5F,{0.4F,1,0.79F,1});
        renderer.draw(ui,draws);
    }
};
Sandbox::Sandbox(Window& w,const std::filesystem::path& assets):impl_(std::make_unique<Impl>(w,assets)){}
Sandbox::~Sandbox()=default;
void Sandbox::poll(){impl_->poll();}
void Sandbox::fixedUpdate(double dt){impl_->fixed(dt);}
void Sandbox::render(double alpha){impl_->render(alpha);}
void Sandbox::capture(const std::filesystem::path& path) const {impl_->renderer.savePPM(path);}
SandboxStatus Sandbox::status() const {return {impl_->physics->bodyCount(),impl_->physics->pose(impl_->player).position,impl_->paused};}
}
