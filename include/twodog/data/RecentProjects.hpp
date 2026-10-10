#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace twodog {
struct RecentProject {
  std::filesystem::path root;
  std::string name;
  double opened = 0;
};
class RecentProjects {
public:
  explicit RecentProjects(std::filesystem::path storage);
  void load();
  void remember(const std::filesystem::path &root);
  void remove(std::size_t index);
  const std::vector<RecentProject> &entries() const { return entries_; }

private:
  void save() const;
  std::filesystem::path storage_;
  std::vector<RecentProject> entries_;
};
} // namespace twodog
