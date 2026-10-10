#pragma once
#include <twodog/data/Json.hpp>
#include <twodog/gameplay/Scene.hpp>
#include <map>

namespace twodog {
glm::vec2 jsonVec2(const Json& value);
glm::vec4 jsonVec4(const Json& value);
Json toJson(glm::vec2 value);
Json toJson(glm::vec4 value);
Transform2D readTransform(const Json& value);
Json writeTransform(const Transform2D& value);
std::string newPersistentId();

struct AssetRecord { std::string id, type; std::filesystem::path path; };
class Project final {
public:
    explicit Project(const std::filesystem::path& directory);
    const AssetRecord& asset(std::string_view id, std::string_view type = {}) const;
    const std::map<std::string, AssetRecord, std::less<>>& assets() const noexcept { return assets_; }
    const std::filesystem::path& root() const noexcept { return root_; }
    const Json& settings() const noexcept { return settings_; }
    Json scene(std::string_view id) const;
    Json prefab(std::string_view id, glm::vec2 position) const;
    static void validateEntity(const Json& entity);
    static void validateScene(const Json& scene);
private:
    std::filesystem::path root_;
    Json settings_;
    std::map<std::string, AssetRecord, std::less<>> assets_;
};
}
