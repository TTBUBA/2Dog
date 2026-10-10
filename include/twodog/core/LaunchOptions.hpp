#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace twodog {
struct LaunchOptions {
    bool help = false;
    bool hidden = false;
    bool vsync = true;
    bool smokeTest = false;
    std::uint64_t maxFrames = 0;
    bool windowOnly = false;
    std::string assetsPath;
    std::string capturePath;
    std::string projectPath;
    std::string managedPath;
    bool noScripts=false;
};
[[nodiscard]] LaunchOptions parseOptions(std::span<const std::string_view> args);
inline constexpr std::string_view usage =
    "2Dog - sprite and physics playground\n"
    "  --help         Print help without initializing graphics\n"
    "  --frames N     Exit after N rendered frames (N > 0)\n"
    "  --hidden       Create an invisible window\n"
    "  --no-vsync     Disable presentation synchronization\n"
    "  --smoke-test   Hidden window, pixel readback, three frames, exit\n"
    "  --window-only  Run the original window-only demonstration\n"
    "  --assets DIR   Load the demonstration atlas from DIR\n"
    "  --project DIR  Open a persistent project (use bundled for the sample next to the EXE)\n"
    "  --managed DIR  C# bootstrap and game DLL directory (default: EXE/managed)\n"
    "  --no-scripts   Open project with native physics/rendering only\n"
    "  --capture FILE Save final rendered frame as PPM (requires --frames)\n";
}
