#include <twodog/core/LaunchOptions.hpp>
#include <charconv>
#include <stdexcept>
#include <string>

namespace twodog {
LaunchOptions parseOptions(std::span<const std::string_view> args) {
    LaunchOptions options;
    bool hasFrames = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto arg = args[i];
        if (arg == "--help") options.help = true;
        else if (arg == "--hidden") options.hidden = true;
        else if (arg == "--no-vsync") options.vsync = false;
        else if (arg == "--smoke-test") options.smokeTest = true;
        else if (arg == "--window-only") options.windowOnly = true;
        else if (arg == "--no-scripts") options.noScripts=true;
        else if (arg == "--assets" || arg == "--capture" || arg == "--project" || arg == "--managed") {
            if (++i == args.size() || args[i].empty() || args[i].starts_with("--"))
                throw std::invalid_argument(std::string(arg)+" requires a path");
            auto& target=arg=="--assets"?options.assetsPath:arg=="--capture"?options.capturePath:arg=="--project"?options.projectPath:options.managedPath;
            if(!target.empty()) throw std::invalid_argument("Duplicate path option");
            target=std::string(args[i]);
        }
        else if (arg == "--frames") {
            if (hasFrames || ++i == args.size())
                throw std::invalid_argument("--frames requires one positive integer, specified once");
            hasFrames = true;
            const auto value = args[i];
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), options.maxFrames);
            if (error != std::errc{} || end != value.data() + value.size() || options.maxFrames == 0)
                throw std::invalid_argument("--frames requires a positive integer in uint64 range");
        } else throw std::invalid_argument("Unknown option: " + std::string(arg));
    }
    if (options.smokeTest) {
        if (hasFrames) throw std::invalid_argument("--smoke-test cannot be combined with --frames");
        options.hidden = true;
        options.vsync = false;
        options.maxFrames = 3;
    }
    if(!options.capturePath.empty() && (options.maxFrames==0 || options.windowOnly))
        throw std::invalid_argument("--capture requires --frames or --smoke-test and the playground");
    if(!options.projectPath.empty() && (options.windowOnly||options.smokeTest)) throw std::invalid_argument("--project cannot be combined with --window-only or --smoke-test");
    if(options.projectPath.empty() && (options.noScripts||!options.managedPath.empty())) throw std::invalid_argument("--no-scripts and --managed require --project");
    return options;
}
}
