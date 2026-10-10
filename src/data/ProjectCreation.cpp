#include <twodog/data/ProjectCreation.hpp>
#include <twodog/data/Json.hpp>
#include <twodog/core/Utf8Path.hpp>
#include <algorithm>
#include <cctype>
#include <stdexcept>
namespace twodog {
std::filesystem::path createEmptyProject(const std::filesystem::path &parent, const std::string &name) {
    if (name.empty() || name.size() > 80 || name == "." || name == ".." || name.front() == ' ' || name.back() == ' ' ||
        name.back() == '.' || name.find_first_of("<>:\"/\\|?*") != std::string::npos ||
        std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32; }))
        throw std::invalid_argument("Choose a project name without reserved characters or trailing spaces/dots");
    auto base = name.substr(0, name.find('.'));
    std::transform(base.begin(), base.end(), base.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL" ||
        (base.size() == 4 && (base.starts_with("COM") || base.starts_with("LPT")) && base[3] >= '0' && base[3] <= '9'))
        throw std::invalid_argument("This project name is reserved by Windows");
    if (!std::filesystem::is_directory(parent))
        throw std::invalid_argument("Choose an existing parent folder");
    const auto target = std::filesystem::canonical(parent) / pathFromUtf8(name);
    if (!std::filesystem::create_directory(target))
        throw std::invalid_argument("Destination already exists; choose a different project name");
    try {
        std::filesystem::create_directory(target / "assets");
        std::filesystem::create_directory(target / "assets" / "Scenes");
        std::filesystem::create_directory(target / "assets" / "Scripts");
        Json(Json::Object{{"version", 1},
                          {"id", "scene.main"},
                          {"name", "Main"},
                          {"gravity", Json::Array{0, -9.81}},
                          {"entities", Json::Array{}},
                          {"joints", Json::Array{}}})
            .save(target / "assets" / "Scenes" / "Main.scene.json");
        Json(Json::Object{
                 {"version", 1},
                 {"name", name},
                 {"startScene", "scene.main"},
                 {"includeDemoScripts", false},
                 {"scriptTypes", Json::Array{}},
                 {"input", Json::Array{}},
                 {"assets", Json::Array{Json::Object{
                                {"id", "scene.main"}, {"type", "scene"}, {"path", "assets/Scenes/Main.scene.json"}}}}})
            .save(target / "project.json");
    } catch (...) {
        // Remove only files/directories owned by this operation; never recursively delete a destination.
        std::error_code ignored;
        for (const auto &rel :
             {"project.json", "assets/Scenes/Main.scene.json", "assets/Scenes", "assets/Scripts", "assets"})
            std::filesystem::remove(target / rel, ignored);
        std::filesystem::remove(target, ignored);
        throw;
    }
    return target;
}
} // namespace twodog
