#include <twodog/platform/OpenGL.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void checkPixel(twodog::OpenGL& gl) {
    gl.clear(0.25F, 0.5F, 0.75F);
    const auto p = gl.readCenterPixel();
    require(std::abs(static_cast<int>(p[0]) - 64) <= 2 &&
            std::abs(static_cast<int>(p[1]) - 128) <= 2 &&
            std::abs(static_cast<int>(p[2]) - 191) <= 2, "Framebuffer readback mismatch");
}
}
int main() {
    try {
        // Repeat in one process: failed duplicate construction must not terminate
        // the active session, and destroying it must permit a clean second session.
        for (int cycle = 0; cycle < 2; ++cycle) {
            twodog::WindowConfig config;
            config.width = 256; config.height = 192; config.visible = false; config.vsync = false;
            twodog::Window window(config);
            twodog::OpenGL gl(window);
            bool duplicateRejected = false;
            try { twodog::Window duplicate(config); }
            catch (const std::logic_error&) { duplicateRejected = true; }
            require(duplicateRejected, "Duplicate GLFW ownership was allowed");
            checkPixel(gl);
            bool wrongThreadRejected = false;
            std::thread worker([&] {
                try { window.pollEvents(); }
                catch (const std::logic_error&) { wrongThreadRejected = true; }
            });
            worker.join();
            require(wrongThreadRejected, "Wrong-thread platform access was allowed");
            const auto before = window.framebufferSize();
            require(before.width > 0 && before.height > 0, "Framebuffer must exist");
            window.resize(384, 288);
            const double deadline = window.nowSeconds() + 2.0;
            while (window.nowSeconds() < deadline) {
                window.pollEvents();
                const auto size = window.framebufferSize();
                if (size.width != before.width && size.height != before.height) break;
                window.waitEvents(0.01);
            }
            const auto resized = window.framebufferSize();
            require(resized.width > before.width && resized.height > before.height, "Resize not reflected in framebuffer");
            checkPixel(gl); window.present();
            window.setTitle("2Dog - lifecycle check");
            require(!window.shouldClose(), "Window started closed");
            window.requestClose(); require(window.shouldClose(), "Close flag not set");
            std::cout << "PASS window lifecycle " << cycle + 1 << " | GL " << gl.version() << '\n';
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL graphics: " << e.what() << '\n'; return 1; }
}
