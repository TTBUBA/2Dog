#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdexcept>
#include <string>
#include <vector>
namespace twodog {
// Windows CRT narrow argv uses the active ANSI code page. Convert wmain arguments explicitly.
template <class F> int withUtf8Arguments(int argc, wchar_t **argv, F entry) {
    std::vector<std::string> strings;
    for (int i = 0; i < argc; ++i) {
        const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1, nullptr, 0, nullptr, nullptr);
        if (count <= 0)
            return 1;
        std::string value(static_cast<std::size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, value.data(), count, nullptr, nullptr);
        value.resize(static_cast<std::size_t>(count - 1));
        strings.push_back(std::move(value));
    }
    std::vector<char *> pointers;
    for (auto &value : strings)
        pointers.push_back(value.data());
    pointers.push_back(nullptr);
    return entry(argc, pointers.data());
}
} // namespace twodog
#endif
