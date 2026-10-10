#include <stdexcept>
#include <twodog/data/ProjectCreation.hpp>
#include <twodog/editor/ScriptWorkspace.hpp>
#include <iostream>
// Generates a real solution for SDK/design-time checks without opening a graphics window.
int main(int argc, char **argv) {
    try {
        if (argc != 4)
            throw std::runtime_error("Expected destination parent, managed directory, EditorSupport");
        const auto root = twodog::createEmptyProject(argv[1], "Game [IDE] & test");
        twodog::ScriptWorkspace workspace(root, argv[2], argv[3]);
        const auto source = workspace.create(root / "assets/Scripts/Nested", "CompletionProbe");
        std::cout << root.string() << '\n' << source.string() << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
