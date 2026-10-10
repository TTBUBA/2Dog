#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>

namespace twodog {
enum class InputDevice { Keyboard, Mouse, Gamepad };
struct InputBinding { InputDevice device; int code; };
struct ActionState { bool down=false, pressed=false, released=false; };
using InputSnapshot = std::map<std::string, ActionState>;
// Event transitions survive zero-update frames. consumeFixed clears edges once.
class Input final {
public:
    void bind(std::string name, std::vector<InputBinding> bindings);
    void rebind(std::string name, std::vector<InputBinding> bindings);
    void beginFrame() noexcept;
    void button(InputDevice device, int code, bool down);
    void releaseAll() noexcept;
    ActionState action(const std::string& name) const;
    InputSnapshot consumeFixed();
    void gamepadAxis(int axis, float value);
    float axis(int index) const;
    void setBlocked(bool keyboard,bool mouse,bool gamepad) noexcept {blocked_={keyboard,mouse,gamepad};}
    bool blocked(const std::vector<InputBinding>& bindings) const;
    void cursor(double x,double y) noexcept {cursorX=x;cursorY=y;}
    void scroll(double y) noexcept {scrollY+=y;}
    double cursorX=0,cursorY=0,scrollY=0;
private:
    struct Action {std::vector<InputBinding> bindings;ActionState frame,pending;};
    std::array<bool,3> blocked_{};
    std::array<bool,512> keys_{};
    std::array<bool,8> mouse_{};
    std::array<bool,15> gamepad_{};
    std::array<float,6> axes_{};
    std::map<std::string,Action> actions_;
    bool get(InputBinding binding) const;
    void update() noexcept;
};
}
