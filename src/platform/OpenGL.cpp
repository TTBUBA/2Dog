#include <twodog/platform/OpenGL.hpp>
#include <cmath>
#include <stdexcept>

#ifdef _WIN32
#define TWODOG_GL_CALL __stdcall
#else
#define TWODOG_GL_CALL
#endif

namespace twodog {
namespace {
// OpenGL ABI types used by the minimal phase-one table. No extra loader dependency.
using Enum = unsigned int;
using Bitfield = unsigned int;
constexpr Enum colorBufferBit = 0x00004000;
constexpr Enum rgba = 0x1908;
constexpr Enum unsignedByte = 0x1401;
constexpr Enum versionString = 0x1F02;
template<typename T> T load(Window& window, const char* name) {
    const auto address = window.procAddress(name);
    if (!address) throw std::runtime_error(std::string("Missing OpenGL function: ") + name);
    return reinterpret_cast<T>(address);
}
}
struct OpenGL::Impl {
    Window& window;
    using Viewport = void (TWODOG_GL_CALL *)(int, int, int, int);
    using ClearColor = void (TWODOG_GL_CALL *)(float, float, float, float);
    using Clear = void (TWODOG_GL_CALL *)(Bitfield);
    using ReadPixels = void (TWODOG_GL_CALL *)(int, int, int, int, Enum, Enum, void*);
    using GetError = Enum (TWODOG_GL_CALL *)();
    using GetString = const unsigned char* (TWODOG_GL_CALL *)(Enum);
    Viewport viewport;
    ClearColor clearColor;
    Clear clear;
    ReadPixels readPixels;
    GetError getError;
    GetString getString;
    std::string version;
    explicit Impl(Window& owner) : window(owner),
        viewport(load<Viewport>(owner, "glViewport")), clearColor(load<ClearColor>(owner, "glClearColor")),
        clear(load<Clear>(owner, "glClear")), readPixels(load<ReadPixels>(owner, "glReadPixels")),
        getError(load<GetError>(owner, "glGetError")), getString(load<GetString>(owner, "glGetString")) {
        const auto* value = getString(versionString);
        if (!value) throw std::runtime_error("OpenGL returned no version string");
        version = reinterpret_cast<const char*>(value);
    }
};
OpenGL::OpenGL(Window& window) : impl_(std::make_unique<Impl>(window)) { checkError("Load OpenGL"); }
OpenGL::~OpenGL() = default;
void OpenGL::clear(float red, float green, float blue, float alpha) {
    impl_->window.requireCurrentContext();
    for (const float value : {red, green, blue, alpha})
        if (!std::isfinite(value) || value < 0.0F || value > 1.0F)
            throw std::invalid_argument("Clear color channels must be finite and in [0, 1]");
    const auto size = impl_->window.framebufferSize();
    if (size.width <= 0 || size.height <= 0) return;
    impl_->viewport(0, 0, size.width, size.height);
    impl_->clearColor(red, green, blue, alpha);
    impl_->clear(colorBufferBit);
    checkError("Clear framebuffer");
}
std::array<unsigned char, 4> OpenGL::readCenterPixel() {
    impl_->window.requireCurrentContext();
    const auto size = impl_->window.framebufferSize();
    if (size.width <= 0 || size.height <= 0) throw std::runtime_error("Cannot read an empty framebuffer");
    std::array<unsigned char, 4> pixel{};
    impl_->readPixels(size.width / 2, size.height / 2, 1, 1, rgba, unsignedByte, pixel.data());
    checkError("Read framebuffer pixel");
    return pixel;
}
const std::string& OpenGL::version() const noexcept { return impl_->version; }
void OpenGL::checkError(const char* operation) const {
    impl_->window.requireCurrentContext();
    const Enum error = impl_->getError();
    if (error != 0) throw std::runtime_error(std::string(operation) + ": OpenGL error " + std::to_string(error));
}
}
