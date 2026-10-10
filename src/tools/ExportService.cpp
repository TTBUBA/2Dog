#include <twodog/tools/ExportService.hpp>
#include <twodog/data/ProjectLayout.hpp>
#include <twodog/data/Json.hpp>
#include <twodog/core/Utf8Path.hpp>
#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
namespace twodog {
namespace {
#ifdef _WIN32
double seconds() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
std::wstring argument(const std::wstring &value) {
    std::wstring out = L"\"";
    std::size_t slashes = 0;
    for (const wchar_t c : value) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        out.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        out += c;
        slashes = 0;
    }
    out.append(slashes * 2, L'\\');
    out += L'"';
    return out;
}
#endif
} // namespace
struct ExportService::Impl : ExportSettings, ExportStatus {
    std::filesystem::path project, support;
    ProjectLayout layout;
    double nextRead = 0;
#ifdef _WIN32
    HANDLE process = nullptr;
#endif
    Impl(const std::filesystem::path &p, const std::filesystem::path &s)
        : project(std::filesystem::canonical(p)), support(std::filesystem::absolute(s)), layout(project) {
        // EditorSupport sits under bin/<configuration>; prefer the Release sibling.
        runtime = pathToUtf8(support.parent_path().parent_path().parent_path() / "export-templates" / "windows-x64" /
                             "Release");
        output = pathToUtf8(project.parent_path() / "Game-Windows");
        scene = Json::read(project / "project.json").text("startScene");
        auto settings = layout.settings() / "export.json";
        if (!std::filesystem::exists(settings))
            settings = project / ".twodog" / "export-settings.json";
        if (std::filesystem::exists(settings)) {
            try {
                const auto j = Json::read(settings);
                output = j.text("output", output);
                name = j.text("name", name);
                scene = j.text("scene", scene);
                runtime = j.text("runtime", runtime);
            } catch (const std::exception &) {
                status = "Saved export settings were invalid; using defaults.";
            }
        }
    }
    ~Impl() {
#ifdef _WIN32
        // A started export finishes even when the editor closes.
        if (process)
            CloseHandle(process);
#endif
    }
    bool busy() const {
#ifdef _WIN32
        return process != nullptr;
#else
        return false;
#endif
    }
    void readLog() {
        std::ifstream in(log, std::ios::binary);
        if (!in)
            return;
        in.seekg(0, std::ios::end);
        const auto size = static_cast<std::streamoff>(in.tellg());
        const std::streamoff limit = 64 * 1024;
        in.seekg(size > limit ? size - limit : std::streamoff(0));
        details.assign(std::istreambuf_iterator<char>(in), {});
    }
    void poll() {
#ifdef _WIN32
        if (!process)
            return;
        DWORD code = STILL_ACTIVE;
        if (!GetExitCodeProcess(process, &code)) {
            status = "Cannot read exporter process status.";
            code = 1;
        }
        if (code != STILL_ACTIVE) {
            CloseHandle(process);
            process = nullptr;
            readLog();
            status = code == 0 ? "Build completed. The output folder is ready to distribute."
                               : "Build failed. Read the log below; existing builds were preserved.";
        } else if (seconds() >= nextRead) {
            readLog();
            nextRead = seconds() + .5;
        }
#endif
    }
    void saveSettings() const {
        std::filesystem::create_directories(layout.settings());
        Json(Json::Object{{"output", output}, {"name", name}, {"scene", scene}, {"runtime", runtime}})
            .save(layout.settings() / "export.json");
    }
    void start(bool run) {
#ifdef _WIN32
        if (busy())
            return;
        const auto script = support / "export-windows.ps1";
        if (!std::filesystem::is_regular_file(script))
            throw std::runtime_error("Exporter missing. Rebuild the editor.");
        const auto folder = layout.exportWork();
        std::filesystem::create_directories(layout.exportLogs());
        std::filesystem::create_directories(folder);
        const auto id = std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64());
        const auto request = folder / ("request-" + id + ".json");
        log = layout.exportLogs() / ("editor-" + id + ".log");
        const Json settings(Json::Object{{"project", pathToUtf8(project)},
                                         {"output", output},
                                         {"name", name},
                                         {"scene", scene},
                                         {"runtime", runtime},
                                         { "run",
                                           run }});
        settings.save(request);
        saveSettings();
        wchar_t system[MAX_PATH]{};
        if (GetSystemDirectoryW(system, MAX_PATH) == 0)
            throw std::runtime_error("Cannot locate Windows PowerShell.");
        const auto powershell = std::filesystem::path(system) / L"WindowsPowerShell" / L"v1.0" / L"powershell.exe";
        auto command = argument(powershell.wstring()) + L" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File " +
                       argument(script.wstring()) + L" -Request " + argument(request.wstring());
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        const HANDLE outputHandle = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (outputHandle == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot create export log.");
        const HANDLE inputHandle = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (inputHandle == INVALID_HANDLE_VALUE) {
            CloseHandle(outputHandle);
            throw std::runtime_error("Cannot create exporter input handle.");
        }
        STARTUPINFOW start{};
        start.cb = sizeof(start);
        start.dwFlags = STARTF_USESTDHANDLES;
        start.hStdOutput = outputHandle;
        start.hStdError = outputHandle;
        start.hStdInput = inputHandle;
        PROCESS_INFORMATION info{};
        const BOOL ok = CreateProcessW(powershell.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                       nullptr, project.c_str(), &start, &info);
        const auto error = GetLastError();
        CloseHandle(outputHandle);
        CloseHandle(inputHandle);
        if (!ok)
            throw std::runtime_error("Cannot start PowerShell. Windows error " + std::to_string(error));
        CloseHandle(info.hThread);
        process = info.hProcess;
        status = "Building Windows x64...";
        details.clear();
        nextRead = 0;
#else
        static_cast<void>(run);
        throw std::runtime_error("Windows export is available on Windows only.");
#endif
    }
};

ExportService::ExportService(const std::filesystem::path &p, const std::filesystem::path &s)
    : impl_(std::make_unique<Impl>(p, s)) {}
ExportService::~ExportService() = default;
ExportSettings &ExportService::settings() { return *impl_; }
const ExportStatus &ExportService::state() const { return *impl_; }
bool ExportService::busy() const { return impl_->busy(); }
void ExportService::poll() { impl_->poll(); }
void ExportService::start(bool run) { impl_->start(run); }
void ExportService::saveSettings() { impl_->saveSettings(); }
std::filesystem::path ExportService::logDirectory() const { return impl_->layout.exportLogs(); }
std::vector<std::string> ExportService::scenes() const {
    std::vector<std::string> result;
    const auto document = Json::read(impl_->project / "project.json");
    for (const auto &asset : document.at("assets").array())
        if (asset.text("type") == "scene")
            result.push_back(asset.at("id").string());
    return result;
}
} // namespace twodog
