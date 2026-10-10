#include <twodog/platform/Desktop.hpp>
#include <twodog/core/Utf8Path.hpp>
#include <twodog/data/Json.hpp>
#include <twodog/platform/OpenGL.hpp>
#include <twodog/core/Application.hpp>
#include <twodog/core/LaunchOptions.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <filesystem>
#ifdef TWODOG_GAMEPLAY
#include <twodog/platform/Sandbox.hpp>
#include <twodog/platform/ProjectPlayer.hpp>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace twodog {
namespace {
class DesktopHost final : public FrameHost {
public:
    DesktopHost(const WindowConfig& config, const LaunchOptions& options, const std::filesystem::path& assets,const std::filesystem::path& executable) : window_(config), gl_(window_), smoke_(options.smokeTest), options_(options) {
        std::cout << "[2Dog] OpenGL " << gl_.version() << '\n';
#ifdef TWODOG_GAMEPLAY
        if(!options.projectPath.empty()) {
            const auto project=options.projectPath=="bundled"?executable/"projects"/"CSharpDemo":pathFromUtf8(options.projectPath);
            const auto managed=options.managedPath.empty()?executable/"managed":pathFromUtf8(options.managedPath);
            project_=std::make_unique<ProjectPlayer>(window_,project,managed,!options.noScripts);
        } else if(!options.windowOnly) sandbox_=std::make_unique<Sandbox>(window_,assets);
#else
        static_cast<void>(assets);static_cast<void>(executable);
#endif
    }
    void pollEvents() override { window_.pollEvents();
#ifdef TWODOG_GAMEPLAY
        if(sandbox_) sandbox_->poll();
        if(project_) project_->poll();
#endif
    }
    bool shouldClose() const override { return window_.shouldClose(); }
    bool suspended() const override { return window_.suspended(); }
    void waitEvents(double timeout) override { window_.waitEvents(timeout); }
    double nowSeconds() const override { return window_.nowSeconds(); }
    void fixedUpdate(double dt) override { previousTime_ = simulationTime_; simulationTime_ += dt;
#ifdef TWODOG_GAMEPLAY
        if(sandbox_) sandbox_->fixedUpdate(dt);
        if(project_) project_->fixedUpdate(dt);
#endif
    }
    void update(double dt) override {
#ifdef TWODOG_GAMEPLAY
        if(project_) project_->update(dt);
#else
        static_cast<void>(dt);
#endif
    }
    void render(double alpha) override {
        const auto interpolated = previousTime_ + (simulationTime_ - previousTime_) * alpha;
        const float blue = smoke_ ? 0.75F : 0.15F + 0.02F * static_cast<float>(std::sin(interpolated));
        gl_.clear(0.125F, 0.25F, blue);
        if (smoke_) {
            const auto pixel = gl_.readCenterPixel();
            if (std::abs(static_cast<int>(pixel[0]) - 32) > 2 ||
                std::abs(static_cast<int>(pixel[1]) - 64) > 2 ||
                std::abs(static_cast<int>(pixel[2]) - 191) > 2)
                throw std::runtime_error("Smoke test: framebuffer color mismatch");
        }
#ifdef TWODOG_GAMEPLAY
        if(project_) {
            gl_.clear(0.07F,0.10F,0.15F);project_->render(alpha);
            if(!options_.capturePath.empty() && rendered_+1==options_.maxFrames) project_->capture(options_.capturePath);
        }
        if(sandbox_) {
            gl_.clear(0.07F,0.10F,0.15F);sandbox_->render(alpha);
            if(!options_.capturePath.empty() && rendered_+1==options_.maxFrames) sandbox_->capture(options_.capturePath);
        }
#endif
        ++rendered_;
    }
    void present() override { window_.present(); }
private:
    Window window_;
    OpenGL gl_;
    bool smoke_;
    double previousTime_ = 0.0;
    double simulationTime_ = 0.0;
    LaunchOptions options_;
    std::uint64_t rendered_=0;
#ifdef TWODOG_GAMEPLAY
    std::unique_ptr<Sandbox> sandbox_;
    std::unique_ptr<ProjectPlayer> project_;
#endif
};
std::filesystem::path executablePath(const char* arg) {
#ifdef _WIN32
    static_cast<void>(arg);std::vector<wchar_t> buffer(32768);
    const auto count=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
    if(count==0 || count>=buffer.size()) throw std::runtime_error("Cannot locate executable directory");
    return std::filesystem::path(std::wstring(buffer.data(),count));
#else
    return std::filesystem::absolute(arg);
#endif
}
}
int desktopMain(int argc, char** argv, ApplicationKind kind) {
    try {
        std::vector<std::string_view> args;
        for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
        auto options = parseOptions(args);
        const auto executableDirectory=executablePath(argv[0]).parent_path();
        std::string exportedTitle;
        const auto exportSettings=executableDirectory/"build-settings.json";
        if(kind==ApplicationKind::Runtime && options.projectPath.empty() && !options.windowOnly &&
           !options.smokeTest && !options.help && std::filesystem::exists(exportSettings)) {
            const auto settings=Json::read(exportSettings);
            if(settings.at("version").integer()!=1) throw std::runtime_error("Unsupported export format");
            options.projectPath=pathToUtf8(executableDirectory/"data");
            options.managedPath=pathToUtf8(executableDirectory/"managed");
            exportedTitle=settings.at("name").string();
        }
        if (options.help) { std::cout << usage; return 0; }
#ifndef TWODOG_GAMEPLAY
        if(!options.projectPath.empty()) throw std::invalid_argument("Project requires TWODOG_BUILD_GAMEPLAY=ON");
        if(!options.capturePath.empty()) throw std::invalid_argument("Capture requires TWODOG_BUILD_GAMEPLAY=ON");
#endif
        WindowConfig config;
        config.title = kind == ApplicationKind::Runtime ? "2Dog | Sprite + Input + Physics Playground" :
            "2Dog Editor | Playground preview - authoring tools pending";
        if(!exportedTitle.empty()) config.title=exportedTitle;
        config.visible = !options.hidden;
        config.vsync = options.vsync;
#ifndef NDEBUG
        config.debugContext = true;
#endif
        const auto assets=options.assetsPath.empty()?executablePath(argv[0]).parent_path()/"assets":std::filesystem::path(options.assetsPath);
        DesktopHost host(config, options,assets,executablePath(argv[0]).parent_path());
        const auto stats = runApplication(host, options.maxFrames);
        if (options.smokeTest && stats.frames != options.maxFrames)
            throw std::runtime_error("Smoke test ended before all frames were rendered");
        std::cout << "[2Dog] frames=" << stats.frames << " fixed_updates=" << stats.fixedUpdates
                  << " dropped_seconds=" << stats.droppedSeconds << '\n';
        if (options.smokeTest) std::cout << "[2Dog] Graphics smoke test PASSED\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[2Dog][error] " << error.what() << '\n';
        return 1;
    }
}
}
