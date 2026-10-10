#include "../platform/GLApi.hpp"
#include <algorithm>
#include <twodog/editor/RenderTarget.hpp>
namespace twodog {
struct RenderTarget::Impl {
    detail::GLApi gl;
    unsigned int fbo = 0, color = 0;
    int w = 0, h = 0;
    using GenFn = void(TDGL_CALL *)(int, unsigned int *);
    using DeleteFn = void(TDGL_CALL *)(int, const unsigned int *);
    using BindFn = void(TDGL_CALL *)(unsigned int, unsigned int);
    using AttachFn = void(TDGL_CALL *)(unsigned int, unsigned int, unsigned int, unsigned int, int);
    using StatusFn = unsigned int(TDGL_CALL *)(unsigned int);
    using ClearColorFn = void(TDGL_CALL *)(float, float, float, float);
    using ClearFn = void(TDGL_CALL *)(unsigned int);
    GenFn gen;
    DeleteFn remove;
    BindFn bind;
    AttachFn attach;
    StatusFn status;
    ClearColorFn clearColor;
    ClearFn clear;
    explicit Impl(Window &window) : gl(window) {
        gen = reinterpret_cast<GenFn>(window.procAddress("glGenFramebuffers"));
        remove = reinterpret_cast<DeleteFn>(window.procAddress("glDeleteFramebuffers"));
        bind = reinterpret_cast<BindFn>(window.procAddress("glBindFramebuffer"));
        attach = reinterpret_cast<AttachFn>(window.procAddress("glFramebufferTexture2D"));
        status = reinterpret_cast<StatusFn>(window.procAddress("glCheckFramebufferStatus"));
        clearColor = reinterpret_cast<ClearColorFn>(window.procAddress("glClearColor"));
        clear = reinterpret_cast<ClearFn>(window.procAddress("glClear"));
        if (!gen || !remove || !bind || !attach || !status || !clearColor || !clear)
            throw std::runtime_error("Missing framebuffer functions");
        gen(1, &fbo);
        gl.GenTextures(1, &color);
    }
    ~Impl() {
        gl.DeleteTextures(1, &color);
        remove(1, &fbo);
    }
};
RenderTarget::RenderTarget(Window &w) : impl_(std::make_unique<Impl>(w)) { resize(640, 360); }
RenderTarget::~RenderTarget() = default;
void RenderTarget::resize(int w, int h) {
    auto &g = *impl_;
    w = std::clamp(w, 1, 4096);
    h = std::clamp(h, 1, 4096);
    if (w == g.w && h == g.h)
        return;
    g.gl.BindTexture(0x0DE1, g.color);
    g.gl.TexImage2D(0x0DE1, 0, 0x8058, w, h, 0, 0x1908, 0x1401, nullptr);
    g.gl.TexParameteri(0x0DE1, 0x2801, 0x2601);
    g.gl.TexParameteri(0x0DE1, 0x2800, 0x2601);
    g.gl.TexParameteri(0x0DE1, 0x2802, 0x812F);
    g.gl.TexParameteri(0x0DE1, 0x2803, 0x812F);
    g.bind(0x8D40, g.fbo);
    g.attach(0x8D40, 0x8CE0, 0x0DE1, g.color, 0);
    const bool complete = g.status(0x8D40) == 0x8CD5;
    g.bind(0x8D40, 0);
    if (!complete)
        throw std::runtime_error("Editor framebuffer incomplete");
    g.w = w;
    g.h = h;
}
void RenderTarget::begin() {
    auto &g = *impl_;
    g.bind(0x8D40, g.fbo);
    g.gl.Viewport(0, 0, g.w, g.h);
    g.gl.Disable(0x0C11);
    g.clearColor(.045F, .065F, .095F, 1);
    g.clear(0x00004000);
}
void RenderTarget::end() { impl_->bind(0x8D40, 0); }
std::uint32_t RenderTarget::texture() const { return impl_->color; }
int RenderTarget::width() const { return impl_->w; }
int RenderTarget::height() const { return impl_->h; }
} // namespace twodog
