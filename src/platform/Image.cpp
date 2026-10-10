#include <twodog/platform/Image.hpp>
#include <fstream>
#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#ifdef _MSC_VER
#pragma warning(push,0)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include <stb_image.h>
#ifdef _MSC_VER
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

namespace twodog {
Image loadImage(const std::filesystem::path& path) {
    std::ifstream stream(path,std::ios::binary|std::ios::ate);
    if(!stream) throw std::runtime_error("Cannot open image: "+path.string());
    const auto length=stream.tellg();
    if(length<=0||length>64*1024*1024) throw std::runtime_error("Image file size outside 1..64 MiB");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    stream.seekg(0);stream.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if(!stream) throw std::runtime_error("Could not read complete image");
    int w=0,h=0,channels=0;
    if(!stbi_info_from_memory(bytes.data(),static_cast<int>(bytes.size()),&w,&h,&channels)||w<=0||h<=0||w>8192||h>8192)
        throw std::runtime_error("Image must decode to dimensions in 1..8192");
    using Pixels=std::unique_ptr<unsigned char,decltype(&stbi_image_free)>;
    Pixels data(stbi_load_from_memory(bytes.data(),static_cast<int>(bytes.size()),&w,&h,&channels,4),stbi_image_free);
    if(!data) throw std::runtime_error("Image decoding failed: "+std::string(stbi_failure_reason()?stbi_failure_reason():"unknown"));
    Image image{w,h,std::vector<unsigned char>(static_cast<std::size_t>(w)*static_cast<std::size_t>(h)*4)};
    const auto row=static_cast<std::size_t>(w)*4;
    for(int y=0;y<h;++y) std::copy_n(data.get()+static_cast<std::size_t>(y)*row,row,image.rgba.data()+static_cast<std::size_t>(h-1-y)*row);
    return image;
}
}
