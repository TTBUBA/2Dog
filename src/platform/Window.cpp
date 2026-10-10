#include <twodog/platform/Window.hpp>
#include <GLFW/glfw3.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <thread>
#ifdef TWODOG_GAMEPLAY
#include <twodog/gameplay/Input.hpp>
#endif

namespace twodog {
namespace {
std::atomic_bool active{false};
void errorCallback(int code, const char* text) noexcept {
    std::fprintf(stderr, "[2Dog][GLFW][%d] %s\n", code, text ? text : "Unknown error");
}
std::runtime_error glfwFailure(const char* operation) {
    const char* text = nullptr;
    const int code = glfwGetError(&text);
    return std::runtime_error(std::string(operation) + " failed (GLFW " + std::to_string(code) + "): " +
                              (text ? text : "no diagnostic"));
}
void checkGlfw(const char* operation) {
    const char* text = nullptr;
    const int code = glfwGetError(&text);
    if (code != GLFW_NO_ERROR)
        throw std::runtime_error(std::string(operation) + ": " + (text ? text : "GLFW error"));
}
}
struct Window::Impl {
    std::thread::id thread = std::this_thread::get_id();
    GLFWwindow* handle = nullptr;
    bool initialized = false;
    bool ownsSession = false;
    bool iconified = false;
    bool escapeCloses = true;
    FramebufferSize framebuffer;
#ifdef TWODOG_GAMEPLAY
    Input input;
    bool focused = true;
#endif
    void requireThread() const {
        if (std::this_thread::get_id() != thread)
            throw std::logic_error("Window operation must run on its creating main thread");
    }
    ~Impl() noexcept {
        if (std::this_thread::get_id() != thread) std::terminate();
        if (handle) glfwDestroyWindow(handle);
        if (initialized) glfwTerminate();
        if (ownsSession) active.store(false);
    }
};
Window::Window(const WindowConfig& config) : impl_(std::make_unique<Impl>()) {
    config.validate();
    bool expected = false;
    if (!active.compare_exchange_strong(expected, true))
        throw std::logic_error("Only one engine Window may own GLFW at a time");
    impl_->ownsSession = true;
    glfwSetErrorCallback(errorCallback);
#ifdef TWODOG_NULL_PLATFORM
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_NULL);
#endif
    if (!glfwInit()) throw glfwFailure("glfwInit");
    impl_->initialized = true;
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef TWODOG_NULL_PLATFORM
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE); // OSMesa does not expose this flag
#else
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, config.debugContext ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, config.visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    impl_->handle = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
    if (!impl_->handle) throw glfwFailure("OpenGL 4.1 core window creation; check graphics drivers");
    glfwMakeContextCurrent(impl_->handle);
    checkGlfw("Make context current");
    glfwSwapInterval(config.vsync ? 1 : 0);
    checkGlfw("Set presentation interval");
    glfwSetWindowUserPointer(impl_->handle, impl_.get());
    glfwGetFramebufferSize(impl_->handle, &impl_->framebuffer.width, &impl_->framebuffer.height);
    glfwSetFramebufferSizeCallback(impl_->handle, [](GLFWwindow* window, int width, int height) noexcept {
        static_cast<Impl*>(glfwGetWindowUserPointer(window))->framebuffer = {width, height};
    });
    glfwSetWindowIconifyCallback(impl_->handle, [](GLFWwindow* window, int value) noexcept {
        static_cast<Impl*>(glfwGetWindowUserPointer(window))->iconified = value == GLFW_TRUE;
    });
    // Escape is a phase-one convenience, not the future input/action mapping system.
    glfwSetKeyCallback(impl_->handle, [](GLFWwindow* window, int key, int, int action, int) noexcept {
#ifdef TWODOG_GAMEPLAY
        if(key>=0 && key<512 && action!=GLFW_REPEAT)
            static_cast<Impl*>(glfwGetWindowUserPointer(window))->input.button(InputDevice::Keyboard,key,action==GLFW_PRESS);
#endif
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && static_cast<Impl*>(glfwGetWindowUserPointer(window))->escapeCloses) glfwSetWindowShouldClose(window, GLFW_TRUE);
    });
#ifdef TWODOG_GAMEPLAY
    glfwSetMouseButtonCallback(impl_->handle,[](GLFWwindow* w,int button,int action,int) noexcept {
        if(button>=0&&button<8) static_cast<Impl*>(glfwGetWindowUserPointer(w))->input.button(InputDevice::Mouse,button,action==GLFW_PRESS);
    });
    glfwSetScrollCallback(impl_->handle,[](GLFWwindow* w,double,double y) noexcept {
        static_cast<Impl*>(glfwGetWindowUserPointer(w))->input.scroll(y);
    });
    glfwSetWindowFocusCallback(impl_->handle,[](GLFWwindow* w,int focused) noexcept {
        auto* self=static_cast<Impl*>(glfwGetWindowUserPointer(w));self->focused=focused==GLFW_TRUE;
        if(!self->focused) self->input.releaseAll();
    });
#endif
    checkGlfw("Install window callbacks");
}
Window::~Window() = default;
void Window::pollEvents() {
    impl_->requireThread();
#ifdef TWODOG_GAMEPLAY
    impl_->input.beginFrame();
#endif
    glfwPollEvents(); checkGlfw("Poll events");
#ifdef TWODOG_GAMEPLAY
    double x=0,y=0;int w=0,h=0;
    glfwGetCursorPos(impl_->handle,&x,&y);glfwGetWindowSize(impl_->handle,&w,&h);
    if(w>0&&h>0) impl_->input.cursor(x*impl_->framebuffer.width/w,y*impl_->framebuffer.height/h);
    GLFWgamepadstate gamepad{};
    const bool connected=impl_->focused&&glfwGetGamepadState(GLFW_JOYSTICK_1,&gamepad)==GLFW_TRUE;
    for(int i=0;i<15;++i) impl_->input.button(InputDevice::Gamepad,i,connected&&gamepad.buttons[i]==GLFW_PRESS);
    for(int i=0;i<6;++i) impl_->input.gamepadAxis(i,connected?gamepad.axes[i]:0.0F);
#endif
}
#ifdef TWODOG_GAMEPLAY
Input& Window::input() {impl_->requireThread();return impl_->input;}
#endif
void Window::waitEvents(double timeoutSeconds) {
    impl_->requireThread();
    if (!std::isfinite(timeoutSeconds) || timeoutSeconds <= 0.0)
        throw std::invalid_argument("Event wait timeout must be finite and positive");
    glfwWaitEventsTimeout(timeoutSeconds);
    checkGlfw("Wait events");
}
bool Window::shouldClose() const {
    impl_->requireThread();
    return glfwWindowShouldClose(impl_->handle) == GLFW_TRUE;
}
void Window::requestClose() { impl_->requireThread(); glfwSetWindowShouldClose(impl_->handle, GLFW_TRUE); }
void Window::cancelClose() {impl_->requireThread();glfwSetWindowShouldClose(impl_->handle,GLFW_FALSE);}
void Window::setEscapeCloses(bool enabled) {impl_->requireThread();impl_->escapeCloses=enabled;}
void* Window::nativeHandle() const {impl_->requireThread();return impl_->handle;}
bool Window::suspended() const {
    impl_->requireThread();
    return impl_->iconified || impl_->framebuffer.width == 0 || impl_->framebuffer.height == 0;
}
FramebufferSize Window::framebufferSize() const { impl_->requireThread(); return impl_->framebuffer; }
void Window::resize(int width, int height) {
    impl_->requireThread();
    WindowConfig config; config.width = width; config.height = height; config.validate();
    glfwSetWindowSize(impl_->handle, width, height);
    checkGlfw("Resize window");
    glfwGetFramebufferSize(impl_->handle, &impl_->framebuffer.width, &impl_->framebuffer.height);
}
void Window::present() { requireCurrentContext(); glfwSwapBuffers(impl_->handle); checkGlfw("Swap buffers"); }
void Window::setTitle(const std::string& title) {
    impl_->requireThread();
    WindowConfig config; config.title = title; config.validate();
    glfwSetWindowTitle(impl_->handle, title.c_str());
    checkGlfw("Set title");
}
Window::Proc Window::procAddress(const char* name) const {
    requireCurrentContext();
    if (!name || !*name) throw std::invalid_argument("OpenGL procedure name must not be empty");
    return glfwGetProcAddress(name);
}
double Window::nowSeconds() const { impl_->requireThread(); return glfwGetTime(); }
void Window::requireCurrentContext() const {
    impl_->requireThread();
    if (glfwGetCurrentContext() != impl_->handle) throw std::logic_error("Window's OpenGL context is not current");
}
}
