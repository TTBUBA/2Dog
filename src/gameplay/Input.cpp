#include <twodog/gameplay/Input.hpp>
#include <cmath>
#include <stdexcept>

namespace twodog {
bool Input::get(InputBinding b) const {
    if(b.code<0) throw std::invalid_argument("Negative input code");
    const auto i=static_cast<std::size_t>(b.code);
    switch(b.device) {
    case InputDevice::Keyboard: if(i<keys_.size()) return keys_[i]; break;
    case InputDevice::Mouse: if(i<mouse_.size()) return mouse_[i]; break;
    case InputDevice::Gamepad: if(i<gamepad_.size()) return gamepad_[i]; break;
    }
    throw std::invalid_argument("Input code out of range");
}
void Input::bind(std::string name,std::vector<InputBinding> bindings) {
    if(name.empty() || bindings.empty()) throw std::invalid_argument("Action requires name and bindings");
    for(const auto b:bindings) static_cast<void>(get(b));
    if(actions_.contains(name)) throw std::invalid_argument("Action already bound");
    actions_.emplace(std::move(name),Action{std::move(bindings),{}, {}}); update();
}
void Input::rebind(std::string name,std::vector<InputBinding> bindings) {
    if(name.empty()||bindings.empty()) throw std::invalid_argument("Action requires name and bindings");
    for(const auto binding:bindings) static_cast<void>(get(binding));
    actions_.insert_or_assign(std::move(name),Action{std::move(bindings),{}, {}});update();
}
void Input::beginFrame() noexcept {
    scrollY=0;
    for(auto& [name,a]:actions_) {static_cast<void>(name);a.frame.pressed=false;a.frame.released=false;}
}
void Input::button(InputDevice device,int code,bool down) {
    static_cast<void>(get({device,code}));
    const auto i=static_cast<std::size_t>(code);
    switch(device) {case InputDevice::Keyboard:keys_[i]=down;break;case InputDevice::Mouse:mouse_[i]=down;break;case InputDevice::Gamepad:gamepad_[i]=down;break;}
    update();
}
void Input::update() noexcept {
    for(auto& [name,a]:actions_) {
        static_cast<void>(name); bool down=false;
        for(const auto b:a.bindings) down=down||get(b);
        if(down!=a.frame.down) {
            if(down) {a.frame.pressed=true;a.pending.pressed=true;}
            else {a.frame.released=true;a.pending.released=true;}
        }
        a.frame.down=down;a.pending.down=down;
    }
}
void Input::releaseAll() noexcept {
    keys_.fill(false);mouse_.fill(false);gamepad_.fill(false);axes_.fill(0);
    for(auto& [name,a]:actions_) {static_cast<void>(name);a.frame.pressed=false;a.pending.pressed=false;}
    update();
}
bool Input::blocked(const std::vector<InputBinding>& bindings) const {
    for(const auto b:bindings) if(blocked_[static_cast<std::size_t>(b.device)]) return true;
    return false;
}
ActionState Input::action(const std::string& name) const {const auto& a=actions_.at(name);return blocked(a.bindings)?ActionState{}:a.frame;}
InputSnapshot Input::consumeFixed() {
    InputSnapshot s;
    for(auto& [name,a]:actions_) {s.emplace(name,blocked(a.bindings)?ActionState{}:a.pending);a.pending.pressed=false;a.pending.released=false;}
    return s;
}
void Input::gamepadAxis(int index,float value) {
    if(index<0 || index>=6 || !std::isfinite(value) || value<-1 || value>1) throw std::invalid_argument("Invalid gamepad axis");
    axes_[static_cast<std::size_t>(index)]=std::abs(value)<0.18F?0:value;
}
float Input::axis(int index) const {
    if(index<0 || index>=6) throw std::invalid_argument("Invalid gamepad axis");
    return blocked_[2]?0:axes_[static_cast<std::size_t>(index)];
}
}
