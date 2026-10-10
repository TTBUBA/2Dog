#include <twodog/data/ProjectCreation.hpp>
#include <twodog/data/RecentProjects.hpp>
#include <twodog/data/Json.hpp>
#include <twodog/editor/ScriptWorkspace.hpp>
#include <twodog/core/Utf8Path.hpp>
#ifdef TWODOG_WORKSPACE_GAMEPLAY
#include <twodog/gameplay/World2D.hpp>
#endif
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value, const char *text) {
    if (!value)
        throw std::runtime_error(text);
}
struct Temp {
    std::filesystem::path path;
    Temp() {
        const auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int i = 0; i < 100; ++i) {
            path = std::filesystem::temp_directory_path() /
                   ("2dog-workspace-" + std::to_string(seed) + "-" + std::to_string(i));
            if (std::filesystem::create_directory(path))
                return;
        }
        throw std::runtime_error("Cannot create isolated test directory");
    }
    ~Temp() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
std::string read(const std::filesystem::path &path) {
    std::ifstream in(path);
    return {std::istreambuf_iterator<char>(in), {}};
}
template <class F> void rejects(F action) {
    bool thrown = false;
    try {
        action();
    } catch (const std::exception &) {
        thrown = true;
    }
    require(thrown, "Invalid operation accepted");
}
} // namespace
int main(int argc, char **argv) {
    try {
        require(argc == 2, "Expected test name");
        const std::string test = argv[1];
        Temp temp;
        if (test == "invalid_name") {
            for (const auto *name : {"", "..", "../escape", "a/b", "a\\b", "Bad:", "Trailing.", "CON", "con.txt",
                                     "LPT1", "COM9", " spaced"})
                rejects([&] { twodog::createEmptyProject(temp.path, name); });
            require(std::filesystem::is_empty(temp.path), "Rejected names changed parent directory");
        } else {
            const auto root = twodog::createEmptyProject(temp.path, "My Game");
            const auto config = twodog::Json::read(root / "project.json");
            if (test == "recent_projects") {
                const auto store = temp.path / "preferences/recent.json";
                twodog::RecentProjects recent(store);
                recent.load();
                require(recent.entries().empty(), "First use must have no recent projects");
                const auto second = twodog::createEmptyProject(temp.path, "Second Game");
                recent.remember(root); recent.remember(second); recent.remember(root / ".");
                require(recent.entries().size() == 2 && recent.entries()[0].root == root,
                        "Recent projects must be unique and most-recent-first");
                twodog::RecentProjects restored(store); restored.load();
                require(restored.entries().size() == 2 && restored.entries()[0].name == "My Game",
                        "Recent project persistence lost names/order");
                restored.remove(0);
                require(std::filesystem::exists(root / "project.json"), "Removing a recent project deleted game files");
                std::filesystem::remove_all(second);
                restored.load();
                require(restored.entries().size() == 1, "Missing projects must remain removable in the UI");
                rejects([&] { restored.remember(second); });
                for (int i = 0; i < 22; ++i) {
                    const auto game = twodog::createEmptyProject(temp.path, "Game" + std::to_string(i));
                    restored.remember(game);
                }
                require(restored.entries().size() == 20, "Recent list exceeds its bound");
                twodog::RecentProjects capped(store); capped.load();
                require(capped.entries().size() == 20 && capped.entries()[0].name == "Game21", "Capped order changed after reload");
                std::ofstream(store) << "invalid JSON";
                rejects([&] { capped.load(); });
                require(capped.entries().size() == 20, "Invalid history destroyed in-memory entries");
            } else if (test == "create_empty") {
                require(config.text("name") == "My Game", "Wrong project name");
                require(!config.flag("includeDemoScripts", true), "Empty project includes demo scripts");
                const auto scene = twodog::Json::read(root / "assets/Scenes/Main.scene.json");
                require(scene.at("entities").array().empty(), "New scene is not empty");
#ifdef TWODOG_WORKSPACE_GAMEPLAY
                twodog::Project project(root);
                twodog::World2D world(project, project.scene("scene.main"));
                require(world.entities().empty(), "Runtime rejected empty project");
#endif
                require(config.at("assets").array().size() == 1, "Unexpected default assets");
                require(std::filesystem::is_directory(root / "assets/Scripts"), "Script directory missing");
                const auto other = twodog::createEmptyProject(temp.path, "Second Game");
                require(root != other && std::filesystem::exists(other / "project.json"),
                        "Projects are not independent");
            } else if (test == "refuse_overwrite") {
                const auto original = read(root / "project.json");
                std::ofstream(root / "keep.txt") << "keep";
                rejects([&] { twodog::createEmptyProject(temp.path, "My Game"); });
                require(read(root / "project.json") == original && read(root / "keep.txt") == "keep",
                        "Existing project overwritten");
                std::filesystem::create_directory(temp.path / "Empty");
                rejects([&] { twodog::createEmptyProject(temp.path, "Empty"); });
            } else if (test == "workspace_tree" || test == "workspace_empty") {
                const auto support = temp.path / "support", managed = temp.path / "managed";
                std::filesystem::create_directories(support / "Defaults");
                std::filesystem::create_directory(managed);
                std::filesystem::create_directories(support / "API");
                std::ofstream(support / "API/Debug.cs") << "namespace TwoDog; public static class Debug {}";
                std::ofstream(support / "Defaults/Demo.cs")
                    << "namespace GameScripts; public class Demo : TwoDog.Behaviour {}";
                if (test == "workspace_tree") {
                    std::filesystem::create_directories(root / "Custom Logic/Deep");
                    std::ofstream(root / "Custom Logic/Deep/Existing.cs") << "class Existing {}";
                    for (const auto *folder : {"obj", "bin", "build", ".vs", ".twodog"}) {
                        std::filesystem::create_directories(root / folder);
                        std::ofstream(root / folder / "Generated.cs") << "invalid generated text";
                    }
                }
                twodog::ScriptWorkspace workspace(root, managed, support);
                const auto csproj = read(root / ".twodog/GameScripts.csproj");
                require(csproj.find("<Compile Include=\"../assets/**/*.cs") == std::string::npos,
                        "IDE compilation still depends on external wildcard enumeration");
                require(read(root / ".twodog/GameScripts.sln").find("GameScripts.csproj") != std::string::npos,
                        "Solution does not include project");
                require(csproj.find("<ProjectReference Include=\"EngineAPI/TwoDog.API.csproj\"") != std::string::npos,
                        "GameScripts must reference the navigable API project");
                require(read(root / ".twodog/EngineAPI/TwoDog.API.csproj").find("Debug.cs") != std::string::npos,
                        "API definition source missing from workspace");
                require(read(root / ".twodog/GameScripts.sln").find("EngineAPI/TwoDog.API.csproj") != std::string::npos,
                        "API project missing from solution");
                require(csproj.find("<TargetFramework>net8.0</TargetFramework>")!=std::string::npos,
                        "Visual Studio 2022 workspace targets unsupported runtime");
                const auto runtime=read(root/".twodog/cache/scripts/RuntimeScripts.csproj");
                require(runtime.find("<TargetFramework>net10.0</TargetFramework>")!=std::string::npos &&
                        runtime.find("<Reference Include=\"TwoDog.API\">")!=std::string::npos &&
                        runtime.find("<ProjectReference")==std::string::npos,"Runtime build must reference shipped API DLL");
                require(read(root/".twodog/Directory.Build.props").find("cache/ide/$(MSBuildProjectName)")!=std::string::npos,
                        "IDE intermediates not isolated by project");
                require(read(root/".twodog/EngineAPI/Sources/Debug.cs").find("class Debug")!=std::string::npos,
                        "API sources unavailable without engine source path");
                require(read(root/".twodog/global.json").find("8.0.100")!=std::string::npos &&
                        read(root/".twodog/cache/scripts/global.json").find("10.0.100")!=std::string::npos,
                        "IDE and runtime SDKs not isolated");
                std::ofstream(support/"API/Components.cs")<<"namespace TwoDog; public class Rigidbody2D {}";
                workspace.refreshIDE();
                require(read(root/".twodog/EngineAPI/TwoDog.API.csproj").find("Components.cs")!=std::string::npos,
                        "New component API not added to existing IDE solution");
                if (test == "workspace_empty") {
                    require(!std::filesystem::exists(root / ".twodog/Builtin/Demo.cs"),
                            "Demo script copied into empty project");
                    require(csproj.find("<Compile ") == std::string::npos, "Empty game compiles unexpected sources");
                } else {
                    auto a = workspace.create(root / "assets/Scripts/X", "ScriptA");
                    auto b = workspace.create(root / "assets/Scripts/Y", "ScriptB");
                    require(a.parent_path() != b.parent_path(), "Script paths collapsed");
                    auto tree = read(root / ".twodog/GameScripts.csproj");
                    require(tree.find("<Link>assets\\Scripts\\X\\ScriptA.cs</Link>") != std::string::npos,
                            "Script A is missing its folder hierarchy");
                    require(tree.find("<Link>assets\\Scripts\\Y\\ScriptB.cs</Link>") != std::string::npos,
                            "Script B is missing its folder hierarchy");
                    require(tree.find("<Link>Custom Logic\\Deep\\Existing.cs</Link>") != std::string::npos,
                            "Existing source outside assets was not included");
                    require(tree.find("Generated.cs") == std::string::npos, "Generated caches leaked into project");
                    require(tree.find("<Visible>true</Visible>") != std::string::npos, "Source is hidden from IDE");
                    const auto timestamp = std::filesystem::last_write_time(root / ".twodog/GameScripts.csproj");
                    std::ofstream(a, std::ios::app) << "\n// code-only edit";
                    workspace.tick(0, true);
                    require(read(root / ".twodog/GameScripts.csproj") == tree &&
                                std::filesystem::last_write_time(root / ".twodog/GameScripts.csproj") == timestamp,
                            "Code-only edit rewrote the IDE project");
                    std::filesystem::rename(root / "Custom Logic/Deep/Existing.cs", root / "Custom Logic/Moved.cs");
                    workspace.tick(0, true);
                    tree = read(root / ".twodog/GameScripts.csproj");
                    require(tree.find("Existing.cs") == std::string::npos &&
                                tree.find("<Link>Custom Logic\\Moved.cs</Link>") != std::string::npos,
                            "Moved source was not synchronized");
                    std::filesystem::remove(root / "Custom Logic/Moved.cs");
                    std::filesystem::create_directories(root / "Custom Logic/A & B; 100% $(test)");
                    std::ofstream(root / "Custom Logic/A & B; 100% $(test)/Special.cs") << "class Special {}";
                    workspace.tick(0, true);
                    tree = read(root / ".twodog/GameScripts.csproj");
                    require(tree.find("Moved.cs") == std::string::npos && tree.find("Special.cs") != std::string::npos,
                            "External add/delete was not synchronized");
                    require(tree.find("A &amp; B%3B 100%25 %24%28test%29") != std::string::npos,
                            "MSBuild/XML path characters were not escaped");
                    require(workspace.types(a).at(0) == "GameScripts.ScriptA", "Script type not recognized");
                    rejects([&] { workspace.create(root / "assets/Scripts/X", "ScriptA"); });
                    rejects([&] { workspace.create(root / "../outside", "Escape"); });
                    require(!std::filesystem::exists(temp.path / "outside"), "Script escaped project");
                }
            } else
                throw std::runtime_error("Unknown workspace test");
        }
        std::cout << "PASS " << test << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
