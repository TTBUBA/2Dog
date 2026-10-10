#include <twodog/core/WindowConfig.hpp>
#include <stdexcept>

namespace twodog {
void WindowConfig::validate() const {
    if (width <= 0 || height <= 0 || width > 32768 || height > 32768)
        throw std::invalid_argument("Window dimensions must be between 1 and 32768");
    if (title.empty() || title.find('\0') != std::string::npos)
        throw std::invalid_argument("Window title must be non-empty and contain no NUL");
}
}
