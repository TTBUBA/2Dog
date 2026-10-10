#include <algorithm>
#include <ctime>
#include <cwctype>
#include <stdexcept>
#include <twodog/core/Utf8Path.hpp>
#include <twodog/data/Json.hpp>
#include <twodog/data/RecentProjects.hpp>
namespace twodog {
namespace {
std::string utf(const std::filesystem::path &path) {
  const auto value = path.generic_u8string();
  return {value.begin(), value.end()};
}
bool same(const std::filesystem::path &a, const std::filesystem::path &b) {
#ifdef _WIN32
  auto left = a.wstring(), right = b.wstring();
  std::transform(left.begin(), left.end(), left.begin(), [](wchar_t c) {
    return static_cast<wchar_t>(std::towlower(c));
  });
  std::transform(right.begin(), right.end(), right.begin(), [](wchar_t c) {
    return static_cast<wchar_t>(std::towlower(c));
  });
  return left == right;
#else
  return a == b;
#endif
}
} // namespace
RecentProjects::RecentProjects(std::filesystem::path storage)
    : storage_(std::move(storage)) {}
void RecentProjects::load() {
  if (!std::filesystem::exists(storage_)) {
    entries_.clear();
    return;
  }
  const auto data = Json::read(storage_);
  if (data.at("version").integer() != 1)
    throw std::runtime_error("Unsupported recent projects format");
  std::vector<RecentProject> next;
  for (const auto &item : data.at("projects").array()) {
    const auto raw = pathFromUtf8(item.text("path"));
    if (!raw.is_absolute())
      continue;
    std::error_code error;
    const auto resolved = std::filesystem::weakly_canonical(raw, error);
    if (error)
      continue;
    if (std::any_of(next.begin(), next.end(),
                    [&](const auto &p) { return same(p.root, resolved); }))
      continue;
    const double opened = item.num("opened", 0);
    next.push_back({resolved, item.text("name", utf(resolved.filename())),
                    opened >= 0 && opened <= 32503680000.0 ? opened : 0});
    if (next.size() == 20)
      break;
  }
  entries_ = std::move(next);
}
void RecentProjects::save() const {
  Json::Array projects;
  for (const auto &entry : entries_)
    projects.emplace_back(Json::Object{{"path", utf(entry.root)},
                                       {"name", entry.name},
                                       {"opened", entry.opened}});
  std::filesystem::create_directories(storage_.parent_path());
  Json(Json::Object{{"version", 1}, {"projects", std::move(projects)}})
      .save(storage_);
}
void RecentProjects::remember(const std::filesystem::path &root) {
  const auto resolved = std::filesystem::canonical(root);
  const auto project = Json::read(resolved / "project.json");
  if (project.at("version").integer() != 1)
    throw std::runtime_error("Unsupported project format");
  auto old = entries_;
  entries_.erase(
      std::remove_if(entries_.begin(), entries_.end(),
                     [&](const auto &p) { return same(p.root, resolved); }),
      entries_.end());
  entries_.insert(entries_.begin(),
                  {resolved, project.text("name", utf(resolved.filename())),
                   static_cast<double>(std::time(nullptr))});
  if (entries_.size() > 20)
    entries_.resize(20);
  try {
    save();
  } catch (...) {
    entries_ = std::move(old);
    throw;
  }
}
void RecentProjects::remove(std::size_t index) {
  if (index >= entries_.size())
    throw std::out_of_range("No recent project selected");
  auto old = entries_;
  entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index));
  try {
    save();
  } catch (...) {
    entries_ = std::move(old);
    throw;
  }
}
} // namespace twodog
