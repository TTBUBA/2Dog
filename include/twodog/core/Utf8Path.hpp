#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace twodog {

inline std::string pathToUtf8(const std::filesystem::path& path) {
    const auto bytes=path.u8string();
    return {bytes.begin(),bytes.end()};
}

// Use the C++20 UTF-8 constructor, including on Windows with a non-UTF-8 code page.
inline std::filesystem::path pathFromUtf8(std::string_view text) {
    std::u8string bytes;
    bytes.reserve(text.size());
    for (const char byte : text) {
        bytes.push_back(static_cast<char8_t>(static_cast<unsigned char>(byte)));
    }
    return std::filesystem::path(bytes);
}

} // namespace twodog
