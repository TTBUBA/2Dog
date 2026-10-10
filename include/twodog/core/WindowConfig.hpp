#pragma once
#include <string>

namespace twodog {
struct WindowConfig {
    int width = 1280;
    int height = 720;
    std::string title = "2Dog";
    bool visible = true;
    bool vsync = true;
    bool debugContext = false;
    void validate() const;
};
struct FramebufferSize { int width = 0; int height = 0; };
}
