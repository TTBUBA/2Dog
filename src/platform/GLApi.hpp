#pragma once
#include <twodog/platform/Window.hpp>
#include <cstddef>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define TDGL_CALL __stdcall
#else
#define TDGL_CALL
#endif
#define TDGL_FUNCTIONS(X) \
 X(void,GenVertexArrays,(int,unsigned int*)) \
 X(void,DeleteVertexArrays,(int,const unsigned int*)) \
 X(void,BindVertexArray,(unsigned int)) \
 X(void,GenBuffers,(int,unsigned int*)) \
 X(void,DeleteBuffers,(int,const unsigned int*)) \
 X(void,BindBuffer,(unsigned int,unsigned int)) \
 X(void,BufferData,(unsigned int,std::ptrdiff_t,const void*,unsigned int)) \
 X(void,BufferSubData,(unsigned int,std::ptrdiff_t,std::ptrdiff_t,const void*)) \
 X(void,EnableVertexAttribArray,(unsigned int)) \
 X(void,VertexAttribPointer,(unsigned int,int,unsigned int,unsigned char,int,const void*)) \
 X(unsigned int,CreateShader,(unsigned int)) \
 X(void,ShaderSource,(unsigned int,int,const char* const*,const int*)) \
 X(void,CompileShader,(unsigned int)) \
 X(void,GetShaderiv,(unsigned int,unsigned int,int*)) \
 X(void,GetShaderInfoLog,(unsigned int,int,int*,char*)) \
 X(void,DeleteShader,(unsigned int)) \
 X(unsigned int,CreateProgram,()) \
 X(void,AttachShader,(unsigned int,unsigned int)) \
 X(void,LinkProgram,(unsigned int)) \
 X(void,GetProgramiv,(unsigned int,unsigned int,int*)) \
 X(void,GetProgramInfoLog,(unsigned int,int,int*,char*)) \
 X(void,DeleteProgram,(unsigned int)) \
 X(void,UseProgram,(unsigned int)) \
 X(int,GetUniformLocation,(unsigned int,const char*)) \
 X(void,UniformMatrix4fv,(int,int,unsigned char,const float*)) \
 X(void,Uniform1i,(int,int)) \
 X(void,GenTextures,(int,unsigned int*)) \
 X(void,DeleteTextures,(int,const unsigned int*)) \
 X(void,BindTexture,(unsigned int,unsigned int)) \
 X(void,TexImage2D,(unsigned int,int,int,int,int,int,unsigned int,unsigned int,const void*)) \
 X(void,TexParameteri,(unsigned int,unsigned int,int)) \
 X(void,PixelStorei,(unsigned int,int)) \
 X(void,ActiveTexture,(unsigned int)) \
 X(void,Enable,(unsigned int)) \
 X(void,Disable,(unsigned int)) \
 X(void,BlendFunc,(unsigned int,unsigned int)) \
 X(void,DrawElements,(unsigned int,int,unsigned int,const void*)) \
 X(void,Viewport,(int,int,int,int)) \
 X(unsigned int,GetError,()) \
 X(void,ReadPixels,(int,int,int,int,unsigned int,unsigned int,void*))

namespace twodog::detail {
struct GLApi {
    Window& window;
#define DECLARE_GL(result,name,args) using name##Fn = result (TDGL_CALL *) args; name##Fn name=nullptr;
    TDGL_FUNCTIONS(DECLARE_GL)
#undef DECLARE_GL
    explicit GLApi(Window& owner):window(owner) {
#define LOAD_GL(result,name,args) name=reinterpret_cast<name##Fn>(owner.procAddress("gl" #name)); if(!name) throw std::runtime_error("Missing gl" #name);
        TDGL_FUNCTIONS(LOAD_GL)
#undef LOAD_GL
    }
    void check(const char* operation) const {
        window.requireCurrentContext();const auto error=GetError();
        if(error) throw std::runtime_error(std::string(operation)+" GL error "+std::to_string(error));
    }
};
}
#undef TDGL_FUNCTIONS
