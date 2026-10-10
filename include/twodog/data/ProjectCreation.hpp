#pragma once
#include <filesystem>
#include <string>
namespace twodog {
// Creates parent/name exclusively. Existing destinations are never overwritten.
std::filesystem::path createEmptyProject(const std::filesystem::path &parent, const std::string &name);
} // namespace twodog
