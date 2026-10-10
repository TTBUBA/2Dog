#pragma once
#include <filesystem>
#include <vector>
namespace twodog {
struct Image {int width=0,height=0;std::vector<unsigned char> rgba;};
Image loadImage(const std::filesystem::path& path); // RGBA8, first row = bottom
}
