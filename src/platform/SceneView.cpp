#include <twodog/data/ComponentSchema.hpp>
#include <twodog/platform/SceneView.hpp>
#include <twodog/core/Utf8Path.hpp>
#include <algorithm>
#include <twodog/platform/GameUI.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace twodog {
    namespace {
        void check(bool ok,const char* message) {if(!ok) throw std::invalid_argument(message);
        }
        void colorCheck(glm::vec4 color) {for(int i=0;i<4;++i) check(color[i]>=0&&color[i]<=1,"Color/UV must be in 0..1");
        }
    }
    VisualAssets::VisualAssets(const Project& p,SpriteRenderer& r):project_(p),renderer_(r),bitmap_(Font::bitmap(r)) {
        for(const auto& [id,a]:project_.assets()) {
            if(a.type=="texture") {
                const auto image=loadImage(a.path);
                textures_.emplace(id,Texture{r.createTexture(image),a.path,std::filesystem::last_write_time(a.path),{image.width,image.height}});
            }
            if(a.type=="font") fonts_.emplace(id,FontAsset{Font::trueType(r,a.path),a.path,std::filesystem::last_write_time(a.path)});
        }
    }
    VisualAssets::~VisualAssets() {
        for(const auto& [id,t]:textures_) {static_cast<void>(id);renderer_.destroyTexture(t.handle);}
        for(const auto& [id,f]:fonts_) {static_cast<void>(id);renderer_.destroyTexture(f.font.texture());}
        renderer_.destroyTexture(bitmap_.texture());
    }
    std::uint32_t VisualAssets::texture(std::string_view id) const {if(id.empty()) return 0;
        const auto i=textures_.find(id);
        if(i==textures_.end()) throw std::invalid_argument("Texture asset not found: "+std::string(id));
        return i->second.handle;
    }
    glm::ivec2 VisualAssets::textureSize(std::string_view id) const {
        if(id.empty()) return {1,1};
        const auto it=textures_.find(id);
        if(it==textures_.end()) throw std::invalid_argument("Texture asset not found");
        return it->second.size;
    }
    std::uint32_t VisualAssets::texturePath(const std::filesystem::path& path) const {const auto canonical=std::filesystem::weakly_canonical(path);
        for(const auto& [id,a]:project_.assets()) if(a.path==canonical&&a.type=="texture") return texture(id);
        throw std::invalid_argument("Tiled image must be registered in project assets");
    }
    const Font& VisualAssets::font(std::string_view id) const {if(id.empty()||id=="bitmap") return bitmap_;
        const auto it=fonts_.find(id);
        if(it==fonts_.end()) throw std::invalid_argument("Font asset not found");
        return it->second.font;
    }
    void VisualAssets::poll() {
        for(auto& [id,t]:textures_) {
            try {const auto stamp=std::filesystem::last_write_time(t.path);
                if(stamp==t.stamp) continue;
                t.stamp=stamp;
                const auto image=loadImage(t.path);
                renderer_.replaceTexture(t.handle,image);
                t.size={image.width,image.height};
                std::cout<<"[2Dog][asset] Reloaded "<<id<<'\n';
            }
            catch(const std::exception& e) {std::cerr<<"[2Dog][asset] "<<id<<": "<<e.what()<<"; previous texture retained\n";
            }
        }
        for(auto& [id,f]:fonts_) {
            try {
                const auto stamp=std::filesystem::last_write_time(f.path);
                if(stamp==f.stamp) continue;
                f.stamp=stamp;
                auto candidate=Font::trueType(renderer_,f.path);
                renderer_.destroyTexture(f.font.texture());
                f.font=std::move(candidate);
                std::cout<<"[2Dog][asset] Reloaded font "<<id<<'\n';
            } catch(const std::exception& e) {std::cerr<<"[2Dog][asset] "<<id<<": "<<e.what()<<"; previous font retained\n";
            }
        }
    }
    SceneView::SceneView(const Project& p,VisualAssets& a,const Json& scene):project_(p),assets_(a) {
        if(scene.contains("camera")) {
            const auto& c=scene.at("camera");
            configure(c);
            camera.pixelsPerUnit=static_cast<float>(c.num("pixelsPerUnit",64));
            smoothing_=static_cast<float>(c.num("smoothing",8));
            if(c.contains("bounds")) bounds_=jsonVec4(c.at("bounds"));
            pixelPerfect_=c.flag("pixelPerfect");
            check(smoothing_>=0&&smoothing_<=100,"Camera smoothing must be 0..100");
            check(bounds_.x<=bounds_.z&&bounds_.y<=bounds_.w,"Invalid camera bounds");
            camera.validate();
        }
        if(scene.contains("tilemaps")) for(const auto& map:scene.at("tilemaps").array()) importMap(map);
        for(const auto& e:scene.at("entities").array()) {
            ComponentSchema::validateEntity(e);
            if(e.contains("ui")) {const auto& u=e.at("ui");assets_.font(u.text("font","bitmap"));if(u.text("kind")=="Image") assets_.texture(u.text("texture"));}
            if(e.contains("sprite")) assets_.texture(e.at("sprite").text("texture"));
            if(e.contains("text")) static_cast<void>(assets_.font(e.at("text").text("font","bitmap")));
        }
    }
    void SceneView::importMap(const Json& component) {
        const auto& asset=project_.asset(component.at("asset").string(),"tilemap");
        const auto map=Json::read(asset.path);
        check(map.text("orientation")=="orthogonal"&&!map.flag("infinite"),"Tiled: only finite orthogonal maps are supported");
        const int width=map.at("width").integer(),height=map.at("height").integer(),tileW=map.at("tilewidth").integer(),tileH=map.at("tileheight").integer();
        check(width>0&&height>0&&width<=1024&&height<=1024&&tileW>0&&tileH>0,"Invalid Tiled dimensions");
        const auto scale=static_cast<float>(component.num("pixelsPerUnit",64));
        check(scale>0&&scale<=4096,"Invalid tilemap scale");
        const auto origin=component.contains("origin")?jsonVec2(component.at("origin")):glm::vec2(0);
        const int baseLayer=component.contains("layer")?component.at("layer").integer():-10;
        struct Tileset {std::uint32_t first,texture;
            int columns,count,w,h,imageW,imageH,margin,spacing;
        };
        std::vector<Tileset> sets;
        for(const auto& entry:map.at("tilesets").array()) {
            auto tileset=entry;
            auto directory=asset.path.parent_path();
            if(entry.contains("source")) {const auto relative=(directory/pathFromUtf8(entry.at("source").string())).lexically_relative(project_.root());
                const auto path=containedPath(project_.root(),relative);
                tileset=Json::read(path);
                directory=path.parent_path();
            }
            check(tileset.contains("image"),"Tiled image collections are not supported");
            if(tileset.contains("tiles")) for(const auto& tile:tileset.at("tiles").array()) check(!tile.contains("animation")&&!tile.contains("objectgroup"),"Tiled per-tile animation/collision is not imported; use native animation/body components");
            const auto relative=(directory/pathFromUtf8(tileset.at("image").string())).lexically_relative(project_.root());
            const auto image=containedPath(project_.root(),relative);
            const int first=entry.at("firstgid").integer();
            check(first>0,"Invalid Tiled firstgid");
            Tileset t{static_cast<std::uint32_t>(first),assets_.texturePath(image),tileset.at("columns").integer(),tileset.at("tilecount").integer(),tileset.at("tilewidth").integer(),tileset.at("tileheight").integer(),tileset.at("imagewidth").integer(),tileset.at("imageheight").integer(),tileset.contains("margin")?tileset.at("margin").integer():0,tileset.contains("spacing")?tileset.at("spacing").integer():0};
            check(t.columns>0&&t.count>0&&t.imageW>0&&t.imageH>0&&t.w>0&&t.h>0&&t.margin>=0&&t.spacing>=0,"Invalid tileset");
            sets.push_back(t);
        }
        check(!sets.empty(),"Tiled map needs a tileset");
        std::sort(sets.begin(),sets.end(),[](const auto& a,const auto& b){return a.first<b.first;});
        int layer=baseLayer;
        for(const auto& l:map.at("layers").array()) {
            check(l.text("type")=="tilelayer","Tiled: only tile layers are imported (no object/group/image layers)");
            check(l.at("width").integer()==width&&l.at("height").integer()==height,"Tiled layer dimensions mismatch");
            check(!l.contains("encoding")&&!l.contains("compression"),"Tiled: export tile layer data as uncompressed JSON arrays");
            const auto& data=l.at("data").array();
            check(data.size()==static_cast<std::size_t>(width)*static_cast<std::size_t>(height),"Tiled layer size mismatch");
            const bool visible=l.flag("visible",true);
            const auto opacity=static_cast<float>(l.num("opacity",1));
            check(opacity>=0&&opacity<=1,"Tiled opacity must be 0..1");
            for(std::size_t i=0;i<data.size();++i) {
                const auto number=data[i].number();
                check(number>=0&&number<=4294967295.0&&std::trunc(number)==number,"Invalid Tiled GID");
                const auto raw=static_cast<std::uint32_t>(number);
                check((raw&0x30000000U)==0,"Tiled diagonal/hexagonal tile flips are not supported in 0.3");
                const auto gid=raw&0x0fffffffU;
                if(gid==0||!visible) continue;
                const Tileset* set=nullptr;
                for(const auto& t:sets) if(gid>=t.first) set=&t;
                check(set!=nullptr,"Tiled GID has no tileset");
                const auto local=gid-set->first;
                check(local<static_cast<std::uint32_t>(set->count),"Tiled GID outside tileset");
                const int x=set->margin+static_cast<int>(local%static_cast<std::uint32_t>(set->columns))*(set->w+set->spacing),y=set->margin+static_cast<int>(local/static_cast<std::uint32_t>(set->columns))*(set->h+set->spacing);
                check(x+set->w<=set->imageW&&y+set->h<=set->imageH,"Tile UV outside atlas");
                Tile tile;
                tile.draw.transform.position=origin+glm::vec2((static_cast<float>(i%static_cast<std::size_t>(width))*static_cast<float>(tileW)+static_cast<float>(set->w)*.5F+static_cast<float>(l.num("offsetx")))/scale,(-static_cast<float>(i/static_cast<std::size_t>(width))*static_cast<float>(tileH)-static_cast<float>(tileH)+static_cast<float>(set->h)*.5F-static_cast<float>(l.num("offsety")))/scale);
                tile.draw.transform.size={static_cast<float>(set->w)/scale,static_cast<float>(set->h)/scale};
                auto& s=tile.draw.sprite;
                s.texture=set->texture;
                s.layer=layer;
                s.color.a=opacity;
                s.uv={static_cast<float>(x)/static_cast<float>(set->imageW),1-static_cast<float>(y+set->h)/static_cast<float>(set->imageH),static_cast<float>(x+set->w)/static_cast<float>(set->imageW),1-static_cast<float>(y)/static_cast<float>(set->imageH)};
                if(raw&0x80000000U) std::swap(s.uv.x,s.uv.z);
                if(raw&0x40000000U) std::swap(s.uv.y,s.uv.w);
                tile.parallax={static_cast<float>(l.num("parallaxx",1)),static_cast<float>(l.num("parallaxy",1))};
                tiles_.push_back(tile);
                check(tiles_.size()<=100000,"Tilemap exceeds 100000 rendered tiles");
            }
            ++layer;
        }
    }
    void SceneView::configure(const Json& c) {
        auto next=camera;
        if(c.contains("center")) next.center=jsonVec2(c.at("center"));
        if(c.contains("zoom")) next.zoom=static_cast<float>(c.at("zoom").number());
        if(c.contains("pixelsPerUnit")) next.pixelsPerUnit=static_cast<float>(c.at("pixelsPerUnit").number());
        const auto bounds=c.contains("bounds")?jsonVec4(c.at("bounds")):bounds_;
        const auto smoothing=static_cast<float>(c.num("smoothing",smoothing_));
        const auto pixelPerfect=c.flag("pixelPerfect",pixelPerfect_);
        const auto follow=c.text("follow",follow_);
        const auto shake=static_cast<float>(c.num("shake",shake_));
        next.validate();
        check(bounds.x<=bounds.z&&bounds.y<=bounds.w,"Invalid camera bounds");
        check(std::isfinite(smoothing)&&smoothing>=0&&smoothing<=100,"Camera smoothing must be 0..100");
        check(std::isfinite(shake)&&shake>=0&&shake<=2,"Camera shake must be 0..2");
        camera=next;
        bounds_=bounds;
        smoothing_=smoothing;
        pixelPerfect_=pixelPerfect;
        follow_=follow;
        shake_=shake;
    }
    Json SceneView::cameraDocument() const {
        return Json::Object{{"center",toJson(camera.center)},{"zoom",camera.zoom},{"pixelsPerUnit",camera.pixelsPerUnit},
            {"bounds",toJson(bounds_)},{"smoothing",smoothing_},{"follow",follow_},{"pixelPerfect",pixelPerfect_}};
    }
    glm::vec2 SceneView::pointer(glm::vec2 pixels) const {
        auto rendered=camera;
        rendered.center+=shakeOffset_;
        const auto ppu=rendered.pixelsPerUnit*rendered.zoom;
        if(pixelPerfect_) rendered.center=glm::round(rendered.center*ppu)/ppu;
        return rendered.screenToWorld(pixels);
    }
    void SceneView::setState(World2D& world,std::string_view id,std::string_view state) {
        auto& a=world.get(id).document.at("animation");
        check(a.at("states").contains(state),"Unknown animation state");
        auto& runtime=animations_[std::string(id)];
        if(runtime.state!=state) {runtime.state=state;
            runtime.clock=0;
        }a["state"]=std::string(state);
    }
    void SceneView::emit(glm::vec2 position,int count,glm::vec4 color) {
        check(count>=0&&count<=512,"Particle burst must be 0..512");
        colorCheck(color);
        std::uniform_real_distribution<float> speed(-3,3);
        for(int i=0;i<count&&particles_.size()<4096;++i) particles_.push_back({position,{speed(random_),std::abs(speed(random_))+1},color,1});
    }
    void SceneView::tick(World2D& world,double dt) {
        const float seconds=static_cast<float>(dt);
        for(const auto& [id,e]:world.entities()) if(e.document.contains("animation")) {
            const auto& a=e.document.at("animation");
            auto& animation=animations_[id];
            const auto state=a.at("state").string();
            if(animation.state!=state) {animation.state=state;
                animation.clock=0;
            }
            animation.clock+=dt;
            const auto& clip=a.at("states").at(animation.state);
            const auto duration=static_cast<double>(clip.at("frames").array().size())/clip.num("fps",8);
            if(animation.clock>=duration) {if(clip.contains("next")) setState(world,id,clip.at("next").string());
                else if(clip.flag("loop",true)) animation.clock=std::fmod(animation.clock,duration);
                else animation.clock=duration;
            }
        }
        std::erase_if(animations_,[&world](const auto& item){return !world.alive(item.first);});
        for(auto& p:particles_) {p.life-=seconds;
            p.velocity.y-=6*seconds;
            p.position+=p.velocity*seconds;
        }std::erase_if(particles_,[](const auto& p){return p.life<=0;});
        if(!follow_.empty()&&world.alive(follow_)) {const auto target=world.transform(follow_).position;
            camera.center=glm::mix(camera.center,target,smoothing_==0?1.0F:1-std::exp(-smoothing_*seconds));
        }
        const glm::vec2 half{static_cast<float>(camera.width)/(2*camera.pixelsPerUnit*camera.zoom),static_cast<float>(camera.height)/(2*camera.pixelsPerUnit*camera.zoom)};
        for(int i=0;i<2;++i) {const auto low=bounds_[i]+half[i],high=bounds_[i+2]-half[i];
            camera.center[i]=low<=high?std::clamp(camera.center[i],low,high):(bounds_[i]+bounds_[i+2])*.5F;
        }
        shake_=std::max(0.0F,shake_-seconds*2);
        std::uniform_real_distribution<float> shake(-shake_,shake_);
        shakeOffset_={shake(random_),shake(random_)};
    }
    void SceneView::draw(World2D& world,SpriteRenderer& renderer,float alpha) {
        Camera2D rendered=camera;
        rendered.center+=shakeOffset_;
        const auto ppu=rendered.pixelsPerUnit*rendered.zoom;
        if(pixelPerfect_) rendered.center=glm::round(rendered.center*ppu)/ppu;
        std::vector<SpriteDraw> draws;
        draws.reserve(tiles_.size()+world.entities().size()+particles_.size());
        for(const auto& tile:tiles_) {auto draw=tile.draw;
            draw.transform.position+=camera.center*(glm::vec2(1)-tile.parallax);
            draws.push_back(draw);
        }
        for(const auto& [id,e]:world.entities()) {
            auto t=world.transform(id,alpha);
            if(pixelPerfect_) t.position=glm::round(t.position*ppu)/ppu;
            if(e.document.contains("sprite")) {
                const auto& s=e.document.at("sprite");
                Sprite sprite;
                sprite.texture=assets_.texture(s.text("texture"));
                if(s.contains("color")) sprite.color=jsonVec4(s.at("color"));
                if(s.contains("uv")) sprite.uv=jsonVec4(s.at("uv"));
                if(s.contains("layer")) sprite.layer=s.at("layer").integer();
                sprite.depth=static_cast<float>(s.num("depth"));
                if(e.document.contains("animation")) {const auto& a=e.document.at("animation");
                    const auto& clip=a.at("states").at(a.at("state").string());
                    const auto& frames=clip.at("frames").array();
                    const auto it=animations_.find(id);
                    const auto time=it==animations_.end()?0:it->second.clock;
                    const auto frame=std::min(static_cast<std::size_t>(time*clip.num("fps",8)),frames.size()-1);
                    sprite.uv=jsonVec4(frames[frame]);
                }
                if(s.contains("parallax")) t.position+=camera.center*(glm::vec2(1)-jsonVec2(s.at("parallax")));
                draws.push_back({t,sprite});
            }
            if(e.document.contains("text")) {const auto& text=e.document.at("text");
                const auto color=text.contains("color")?jsonVec4(text.at("color")):glm::vec4(1);
                assets_.font(text.text("font","bitmap")).append(draws,text.at("value").string(),t.position,static_cast<float>(text.num("scale",.03)),color);
            }
        }
        for(const auto& p:particles_) {SpriteDraw d;
            d.transform.position=p.position;
            d.transform.size={.07F,.07F};
            d.sprite.color=p.color;
            d.sprite.color.a*=p.life;
            d.sprite.layer=10;
            draws.push_back(d);
        }
        renderer.draw(rendered,draws);
    }
}
