#include <twodog/tools/ExportService.hpp>
#include <twodog/data/ProjectCreation.hpp>
#include <twodog/data/ProjectLayout.hpp>
#include <twodog/data/Json.hpp>
#include <chrono>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value, const char *text) {
    if (!value)
        throw std::runtime_error(text);
}
struct Temp {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("2dog-export-service-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() { std::filesystem::create_directory(path); }
    ~Temp() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};
} // namespace
int main(int argc, char **argv) {
    try {
        check(argc == 2, "Expected case");
        const std::string test = argv[1];
        Temp temp;
        const auto root = twodog::createEmptyProject(temp.path, "Game");
        const auto support = temp.path / "engine/build/bin/Debug/EditorSupport";
        twodog::ProjectLayout layout(root);
        if (test == "legacy_settings") {
            std::filesystem::create_directories(root / ".twodog");
            twodog::Json(twodog::Json::Object{{"name", "OldName"}, {"output", "old-output"}})
                .save(root / ".twodog/export-settings.json");
        }
        twodog::ExportService service(root, support);
        check(!service.busy(), "Idle service is busy");
        if (test == "defaults") {
            check(service.scenes() == std::vector<std::string>{"scene.main"}, "Scene choices missing");
            check(service.logDirectory() == layout.exportLogs(), "Logs mixed with jobs");
            check(std::filesystem::path(service.settings().runtime) ==
                      temp.path / "engine/build/export-templates/windows-x64/Release",
                  "Template still points at editor bin");
            check(!std::filesystem::exists(layout.exportWork()), "Reading settings created an export job");
        } else if (test == "settings_roundtrip" || test == "legacy_settings") {
            if (test == "legacy_settings")
                check(service.settings().name == "OldName", "Legacy settings lost");
            service.settings().name = "New Game";
            service.settings().output = "new-output";
            service.saveSettings();
            twodog::ExportService restored(root, support);
            check(restored.settings().name == "New Game" && restored.settings().output == "new-output",
                  "Settings not restored");
            check(std::filesystem::is_regular_file(layout.settings() / "export.json"), "Settings not isolated");
            check(!std::filesystem::exists(root / "assets/export.json"), "Generated settings entered game assets");
        } else
            throw std::runtime_error("Unknown case");
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
