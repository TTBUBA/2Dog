#include <twodog/editor/WindowsExport.hpp>
#include <twodog/tools/ExportService.hpp>
#include <twodog/core/Utf8Path.hpp>
#include <imgui.h>
#include <imgui_stdlib.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#endif
namespace twodog {
namespace {
#ifdef _WIN32
std::filesystem::path chooseFolder() {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IFileOpenDialog *dialog = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        DWORD flags = 0;
        if (SUCCEEDED(dialog->GetOptions(&flags)) &&
            SUCCEEDED(dialog->SetOptions(flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM)) &&
            SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem *item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR value = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &value))) {
                    result = value;
                    CoTaskMemFree(value);
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    if (SUCCEEDED(initialized))
        CoUninitialize();
    return result;
}
#endif
} // namespace
struct WindowsExport::Impl {
    ExportService service;
    bool visible = false;
    std::string error;
    Impl(const std::filesystem::path &p, const std::filesystem::path &s) : service(p, s) {}
};
WindowsExport::WindowsExport(const std::filesystem::path &p, const std::filesystem::path &s)
    : impl_(std::make_unique<Impl>(p, s)) {}
WindowsExport::~WindowsExport() = default;
void WindowsExport::open() { impl_->visible = true; }
void WindowsExport::draw(bool canBuild, const std::function<void()> &saveScene) {
    auto &panel = *impl_;
    auto &service = panel.service;
    service.poll();
    auto &s = service.settings();
    if (!panel.visible)
        return;
    ImGui::SetNextWindowSize(ImVec2(690, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Build for Windows", &panel.visible)) {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted("WINDOWS / x64 / RELEASE");
    ImGui::TextWrapped("Portable game with bundled .NET 10. All registered scenes and assets are included. The current "
                       "scene is saved before building.");
    ImGui::Separator();
    ImGui::BeginDisabled(service.busy());
    ImGui::InputText("Executable name", &s.name);
    ImGui::InputText("Output folder (new)", &s.output);
#ifdef _WIN32
    if (ImGui::Button("Choose output parent...")) {
        const auto folder = chooseFolder();
        if (!folder.empty())
            s.output = pathToUtf8(folder / pathFromUtf8(s.name + "-Windows"));
    }
#endif
    if (ImGui::BeginCombo("Start scene", s.scene.c_str())) {
        try {
            for (const auto &id : service.scenes())
                if (ImGui::Selectable(id.c_str(), id == s.scene))
                    s.scene = id;
        } catch (const std::exception &e) {
            panel.error = e.what();
        }
        ImGui::EndCombo();
    }
    ImGui::InputText("Release runtime folder", &s.runtime);
    ImGui::TextWrapped("Prepare once: scripts/build-editor.ps1 -Configuration Release. Exports require the .NET 10 x64 "
                       "SDK on this PC; players do not need the SDK or editor.");
    ImGui::BeginDisabled(!canBuild);
    auto build = [&](bool run) {
        try {
            saveScene();
            service.start(run);
            panel.error.clear();
        } catch (const std::exception &e) {
            panel.error = e.what();
        }
    };
    if (ImGui::Button("Build"))
        build(false);
    ImGui::SameLine();
    if (ImGui::Button("Build and Run"))
        build(true);
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if (!canBuild)
        ImGui::TextWrapped("Stop Play mode and wait for the script compiler before exporting.");
#ifdef _WIN32
    if (ImGui::Button("Open output folder") && std::filesystem::is_directory(pathFromUtf8(s.output)))
        ShellExecuteW(nullptr, L"open", pathFromUtf8(s.output).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    ImGui::SameLine();
    if (ImGui::Button("Open logs"))
        ShellExecuteW(nullptr, L"open", service.logDirectory().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    ImGui::TextWrapped("Export is available on Windows only.");
#endif
    ImGui::Separator();
    ImGui::TextWrapped("%s", (panel.error.empty() ? service.state().status : panel.error).c_str());
    ImGui::BeginChild("Export log", ImVec2(0, 0), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted(service.state().details.c_str());
    ImGui::EndChild();
    ImGui::End();
}
} // namespace twodog
