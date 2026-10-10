#include <twodog/platform/SpriteRenderer.hpp>
#include "GLApi.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace twodog {
namespace {
struct Vertex {glm::vec2 position,uv;glm::vec4 color;};
constexpr unsigned int arrayBuffer=0x8892,elementBuffer=0x8893,texture2D=0x0DE1;
constexpr const char* vertexSource=R"GLSL(#version 410 core
layout(location=0) in vec2 aPosition;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
uniform mat4 uProjection;
out vec2 vUV;
out vec4 vColor;
void main(){gl_Position=uProjection*vec4(aPosition,0.0,1.0);vUV=aUV;vColor=aColor;}
)GLSL";
constexpr const char* fragmentSource=R"GLSL(#version 410 core
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTexture;
out vec4 color;
void main(){color=texture(uTexture,vUV)*vColor;}
)GLSL";
unsigned int compile(detail::GLApi& gl,unsigned int type,const char* source) {
    const auto id=gl.CreateShader(type);
    if(!id) throw std::runtime_error("Cannot allocate shader");
    gl.ShaderSource(id,1,&source,nullptr);gl.CompileShader(id);
    int success=0;gl.GetShaderiv(id,0x8B81,&success);
    if(!success) {char log[4096]{};gl.GetShaderInfoLog(id,4096,nullptr,log);gl.DeleteShader(id);throw std::runtime_error(std::string("Shader compile failed: ")+log);}
    return id;
}
void validate(const SpriteDraw& d) {
    for(const auto n:{d.transform.position.x,d.transform.position.y,d.transform.size.x,d.transform.size.y,d.transform.rotation,d.sprite.depth})
        if(!std::isfinite(n)) throw std::invalid_argument("Non-finite sprite transform/order");
    for(int i=0;i<4;++i) if(!std::isfinite(d.sprite.color[i])||d.sprite.color[i]<0||d.sprite.color[i]>1||
        !std::isfinite(d.sprite.uv[i])||d.sprite.uv[i]<0||d.sprite.uv[i]>1) throw std::invalid_argument("Invalid sprite color/UV");
}
}
struct SpriteRenderer::Impl {
    detail::GLApi gl;
    std::size_t capacity;
    unsigned int vao=0,vbo=0,ebo=0,program=0;
    int projection=-1;
    std::vector<unsigned int> textures;
    std::vector<Vertex> vertices;
    explicit Impl(Window& window,std::size_t size):gl(window),capacity(size) {
        if(size==0||size>65536) throw std::invalid_argument("Batch capacity must be 1..65536");
    }
    ~Impl() {
        gl.window.requireCurrentContext();
        if(!textures.empty()) gl.DeleteTextures(static_cast<int>(textures.size()),textures.data());
        if(program) gl.DeleteProgram(program);
        if(ebo) gl.DeleteBuffers(1,&ebo);
        if(vbo) gl.DeleteBuffers(1,&vbo);
        if(vao) gl.DeleteVertexArrays(1,&vao);
    }
    void init() {
        const auto vs=compile(gl,0x8B31,vertexSource);
        unsigned int fs=0;
        try {fs=compile(gl,0x8B30,fragmentSource);} catch(...) {gl.DeleteShader(vs);throw;}
        program=gl.CreateProgram();gl.AttachShader(program,vs);gl.AttachShader(program,fs);gl.LinkProgram(program);
        gl.DeleteShader(vs);gl.DeleteShader(fs);
        int success=0;gl.GetProgramiv(program,0x8B82,&success);
        if(!success) {char log[4096]{};gl.GetProgramInfoLog(program,4096,nullptr,log);throw std::runtime_error(std::string("Shader link failed: ")+log);}
        projection=gl.GetUniformLocation(program,"uProjection");
        const int textureUniform=gl.GetUniformLocation(program,"uTexture");
        if(projection<0||textureUniform<0) throw std::runtime_error("Missing shader uniforms");
        gl.UseProgram(program);gl.Uniform1i(textureUniform,0);
        gl.GenVertexArrays(1,&vao);gl.BindVertexArray(vao);
        gl.GenBuffers(1,&vbo);gl.BindBuffer(arrayBuffer,vbo);
        gl.BufferData(arrayBuffer,static_cast<std::ptrdiff_t>(capacity*4*sizeof(Vertex)),nullptr,0x88E8);
        gl.GenBuffers(1,&ebo);gl.BindBuffer(elementBuffer,ebo);
        std::vector<unsigned int> indices;indices.reserve(capacity*6);
        for(std::size_t i=0;i<capacity;++i) {const auto v=static_cast<unsigned int>(i*4);for(const auto n:{v,v+1,v+2,v+2,v+3,v}) indices.push_back(n);}
        gl.BufferData(elementBuffer,static_cast<std::ptrdiff_t>(indices.size()*sizeof(unsigned int)),indices.data(),0x88E4);
        gl.EnableVertexAttribArray(0);gl.VertexAttribPointer(0,2,0x1406,0,static_cast<int>(sizeof(Vertex)),reinterpret_cast<void*>(offsetof(Vertex,position)));
        gl.EnableVertexAttribArray(1);gl.VertexAttribPointer(1,2,0x1406,0,static_cast<int>(sizeof(Vertex)),reinterpret_cast<void*>(offsetof(Vertex,uv)));
        gl.EnableVertexAttribArray(2);gl.VertexAttribPointer(2,4,0x1406,0,static_cast<int>(sizeof(Vertex)),reinterpret_cast<void*>(offsetof(Vertex,color)));
        vertices.reserve(capacity*4);gl.BindVertexArray(0);gl.check("Create sprite renderer");
    }
    unsigned int upload(const Image& image) {
        gl.window.requireCurrentContext();
        if(image.width<=0||image.height<=0||image.width>8192||image.height>8192||
            image.rgba.size()!=static_cast<std::size_t>(image.width)*static_cast<std::size_t>(image.height)*4) throw std::invalid_argument("Invalid RGBA texture");
        unsigned int id=0;gl.GenTextures(1,&id);
        try {
            gl.BindTexture(texture2D,id);gl.PixelStorei(0x0CF5,1);
            gl.TexParameteri(texture2D,0x2801,0x2600);gl.TexParameteri(texture2D,0x2800,0x2600); // nearest
            gl.TexParameteri(texture2D,0x2802,0x812F);gl.TexParameteri(texture2D,0x2803,0x812F); // clamp
            gl.TexImage2D(texture2D,0,0x8058,image.width,image.height,0,0x1908,0x1401,image.rgba.data());
            gl.check("Upload sprite texture");
        } catch(...) {if(id) gl.DeleteTextures(1,&id);throw;}
        return id;
    }
    unsigned int texture(std::uint32_t handle) const {
        if(handle>=textures.size()||textures[handle]==0) throw std::invalid_argument("Invalid or destroyed texture handle");
        return textures[handle];
    }
};
SpriteRenderer::SpriteRenderer(Window& window,std::size_t capacity):impl_(std::make_unique<Impl>(window,capacity)) {
    impl_->init();static_cast<void>(createTexture({1,1,{255,255,255,255}}));
}
SpriteRenderer::~SpriteRenderer()=default;
std::uint32_t SpriteRenderer::createTexture(const Image& image) {
    if(impl_->textures.size()>=std::numeric_limits<std::uint32_t>::max()) throw std::length_error("Texture handle capacity exhausted");
    const auto id=impl_->upload(image);
    try {impl_->textures.push_back(id);} catch(...) {impl_->gl.DeleteTextures(1,&id);throw;}
    return static_cast<std::uint32_t>(impl_->textures.size()-1);
}
std::uint32_t SpriteRenderer::loadTexture(const std::filesystem::path& path) {return createTexture(loadImage(path));}
void SpriteRenderer::replaceTexture(std::uint32_t handle,const Image& image) {
    if(handle==0) throw std::invalid_argument("Cannot replace white texture");
    const auto previous=impl_->texture(handle);const auto replacement=impl_->upload(image);
    impl_->textures[handle]=replacement;impl_->gl.DeleteTextures(1,&previous);
}
void SpriteRenderer::destroyTexture(std::uint32_t handle) {
    impl_->gl.window.requireCurrentContext();if(handle==0) throw std::invalid_argument("Cannot destroy white texture");
    const auto id=impl_->texture(handle);impl_->gl.DeleteTextures(1,&id);impl_->textures[handle]=0;
}
std::uint32_t SpriteRenderer::nativeTexture(std::uint32_t texture) const {return impl_->texture(texture);}
RenderStats SpriteRenderer::draw(const Camera2D& camera,std::span<const SpriteDraw> sprites) {
    auto& gl=impl_->gl;gl.window.requireCurrentContext();const auto matrix=camera.projection();
    std::vector<SpriteDraw> ordered(sprites.begin(),sprites.end());
    for(const auto& item:ordered) {validate(item);static_cast<void>(impl_->texture(item.sprite.texture));}
    std::stable_sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){
        if(a.sprite.layer!=b.sprite.layer) return a.sprite.layer<b.sprite.layer;
        return a.sprite.depth<b.sprite.depth;
    });
    gl.Viewport(0,0,camera.width,camera.height);gl.Disable(0x0B71);gl.Disable(0x0B44);gl.Disable(0x0C11);
    gl.Enable(0x0BE2);gl.BlendFunc(0x0302,0x0303);gl.UseProgram(impl_->program);
    gl.UniformMatrix4fv(impl_->projection,1,0,glm::value_ptr(matrix));gl.ActiveTexture(0x84C0);
    gl.BindVertexArray(impl_->vao);gl.BindBuffer(arrayBuffer,impl_->vbo);
    auto& vertices=impl_->vertices;vertices.clear();RenderStats stats{sprites.size(),0};
    std::uint32_t currentTexture=0;
    auto flush=[&] {
        if(vertices.empty()) return;
        gl.BindTexture(texture2D,impl_->texture(currentTexture));
        gl.BufferSubData(arrayBuffer,0,static_cast<std::ptrdiff_t>(vertices.size()*sizeof(Vertex)),vertices.data());
        gl.DrawElements(0x0004,static_cast<int>(vertices.size()/4*6),0x1405,nullptr);++stats.drawCalls;vertices.clear();
    };
    constexpr glm::vec2 corners[4]={{-0.5F,-0.5F},{0.5F,-0.5F},{0.5F,0.5F},{-0.5F,0.5F}};
    for(const auto& item:ordered) {
        if(!vertices.empty()&&(currentTexture!=item.sprite.texture||vertices.size()==impl_->capacity*4)) flush();
        currentTexture=item.sprite.texture;
        const auto& t=item.transform;const float c=std::cos(t.rotation),s=std::sin(t.rotation);const auto uv=item.sprite.uv;
        const glm::vec2 uvs[4]={{uv.x,uv.y},{uv.z,uv.y},{uv.z,uv.w},{uv.x,uv.w}};
        for(int i=0;i<4;++i) {const auto p=corners[i]*t.size;vertices.push_back({t.position+glm::vec2(c*p.x-s*p.y,s*p.x+c*p.y),uvs[i],item.sprite.color});}
    }
    flush();gl.BindVertexArray(0);gl.check("Draw sprites");return stats;
}
Image SpriteRenderer::capture() const {
    auto& gl=impl_->gl;gl.window.requireCurrentContext();const auto size=gl.window.framebufferSize();
    if(size.width<=0||size.height<=0) throw std::runtime_error("Cannot capture empty framebuffer");
    Image image{size.width,size.height,std::vector<unsigned char>(static_cast<std::size_t>(size.width)*static_cast<std::size_t>(size.height)*4)};
    gl.PixelStorei(0x0D05,1);gl.ReadPixels(0,0,size.width,size.height,0x1908,0x1401,image.rgba.data());gl.check("Capture framebuffer");return image;
}
void SpriteRenderer::savePPM(const std::filesystem::path& path) const {
    const auto image=capture();std::ofstream stream(path,std::ios::binary);
    if(!stream) throw std::runtime_error("Cannot create capture file");
    stream<<"P6\n"<<image.width<<" "<<image.height<<"\n255\n";
    for(int y=image.height-1;y>=0;--y) for(int x=0;x<image.width;++x) {
        const auto i=(static_cast<std::size_t>(y)*static_cast<std::size_t>(image.width)+static_cast<std::size_t>(x))*4;
        stream.write(reinterpret_cast<const char*>(image.rgba.data()+i),3);
    }
    if(!stream) throw std::runtime_error("Capture write failed");
}
}
