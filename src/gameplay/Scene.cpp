#include <twodog/gameplay/Scene.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace twodog {
Entity Scene::create() {
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        auto& slot = slots_[i];
        if (!slot.live && slot.generation != 0) {
            slot.object = {}; slot.live = true;
            return {static_cast<std::uint32_t>(i), slot.generation};
        }
    }
    if (slots_.size() >= std::numeric_limits<std::uint32_t>::max()) throw std::length_error("Scene capacity reached");
    slots_.push_back({1, true, {}});
    return {static_cast<std::uint32_t>(slots_.size()-1), 1};
}
bool Scene::alive(Entity e) const noexcept { return e.index < slots_.size() && slots_[e.index].live && slots_[e.index].generation == e.generation; }
void Scene::destroy(Entity e) {
    if (!alive(e)) throw std::invalid_argument("Expired entity handle");
    auto& s = slots_[e.index]; s.object = {}; s.live = false; ++s.generation;
    // Generation wrapping to 0 permanently retires a slot.
}
SceneObject& Scene::get(Entity e) { if (!alive(e)) throw std::invalid_argument("Expired entity handle"); return slots_[e.index].object; }
const SceneObject& Scene::get(Entity e) const { if (!alive(e)) throw std::invalid_argument("Expired entity handle"); return slots_[e.index].object; }
std::vector<Entity> Scene::entities() const {
    std::vector<Entity> result;
    for (std::size_t i=0;i<slots_.size();++i) if(slots_[i].live) result.push_back({static_cast<std::uint32_t>(i),slots_[i].generation});
    return result;
}
void Camera2D::validate() const {
    if (width<=0 || height<=0 || !std::isfinite(zoom) || zoom<0.01F || zoom>100.0F ||
        !std::isfinite(pixelsPerUnit) || pixelsPerUnit<1 || pixelsPerUnit>10000 ||
        !std::isfinite(center.x) || !std::isfinite(center.y)) throw std::invalid_argument("Invalid camera");
}
glm::mat4 Camera2D::projection() const {
    validate();
    const float halfW=static_cast<float>(width)/(2*pixelsPerUnit*zoom), halfH=static_cast<float>(height)/(2*pixelsPerUnit*zoom);
    return glm::ortho(center.x-halfW,center.x+halfW,center.y-halfH,center.y+halfH,-1.0F,1.0F);
}
glm::vec2 Camera2D::screenToWorld(glm::vec2 p) const {
    validate(); return center + glm::vec2(p.x-static_cast<float>(width)*0.5F,static_cast<float>(height)*0.5F-p.y)/(pixelsPerUnit*zoom);
}
glm::vec2 Camera2D::worldToScreen(glm::vec2 p) const {
    validate(); const auto d=(p-center)*(pixelsPerUnit*zoom);
    return {static_cast<float>(width)*0.5F+d.x,static_cast<float>(height)*0.5F-d.y};
}
SpriteAnimation::SpriteAnimation(std::vector<glm::vec4> frames,double fps,bool loop):frames_(std::move(frames)),fps_(fps),loop_(loop) {
    if(frames_.empty() || !std::isfinite(fps) || fps<=0 || fps>1000) throw std::invalid_argument("Invalid animation");
    for(const auto uv:frames_) for(int i=0;i<4;++i)
        if(!std::isfinite(uv[i])||uv[i]<0||uv[i]>1) throw std::invalid_argument("Invalid animation UV");
}
void SpriteAnimation::advance(double seconds) {
    if(!std::isfinite(seconds) || seconds<0 || seconds>3600) throw std::invalid_argument("Invalid animation delta");
    if(finished_) return;
    const double duration=static_cast<double>(frames_.size())/fps_;
    time_+=seconds;
    if(loop_) {
        time_=std::fmod(time_,duration);
        if(duration-time_<1e-10/fps_) time_=0; // Exact cycle represented just below duration
    }
    else if(time_>=duration) { frame_=frames_.size()-1; finished_=true; return; }
    frame_=std::min(frames_.size()-1,static_cast<std::size_t>(time_*fps_+1e-9));
}
void SpriteAnimation::reset() noexcept {time_=0;frame_=0;finished_=false;}
glm::vec4 SpriteAnimation::uv() const {return frames_[frame_];}
}
