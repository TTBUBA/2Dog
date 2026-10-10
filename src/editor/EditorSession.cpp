#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <twodog/core/Utf8Path.hpp>
#include <twodog/editor/EditorSession.hpp>
#ifdef TWODOG_NULL_PLATFORM
#include <imgui_impl_opengl3_loader.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
namespace twodog {
ImGuiSession::ImGuiSession(Window &window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr; // Layout is resettable and does not write to the launch directory.
    ImGui::StyleColorsDark();
    auto &style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(9, 6);
    style.ItemSpacing = ImVec2(9, 8);
    style.ItemInnerSpacing = ImVec2(6, 5);
    style.CellPadding = ImVec2(8, 6);
    style.IndentSpacing = 18;
    style.ScrollbarSize = 12;
    style.WindowRounding = 0;
    style.ChildRounding = 5;
    style.FrameRounding = 5;
    style.PopupRounding = 7;
    style.ScrollbarRounding = 8;
    style.GrabRounding = 4;
    style.TabRounding = 5;
    style.WindowBorderSize = 1;
    style.ChildBorderSize = 1;
    style.FrameBorderSize = 0;
    style.PopupBorderSize = 1;
    style.DockingSeparatorSize = 3;
    auto &c = style.Colors;
    const ImVec4 base(.078F, .094F, .122F, 1), panel(.098F, .118F, .153F, 1), field(.125F, .153F, .196F, 1),
        line(.188F, .227F, .286F, 1), accent(.369F, .655F, 1, 1), hover(.17F, .26F, .37F, 1),
        active(.145F, .224F, .31F, 1);
    c[ImGuiCol_Text] = ImVec4(.902F, .925F, .961F, 1);
    c[ImGuiCol_TextDisabled] = ImVec4(.60F, .66F, .73F, 1);
    c[ImGuiCol_WindowBg] = panel;
    c[ImGuiCol_ChildBg] = panel;
    c[ImGuiCol_PopupBg] = field;
    c[ImGuiCol_Border] = line;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = field;
    c[ImGuiCol_FrameBgHovered] = hover;
    c[ImGuiCol_FrameBgActive] = active;
    c[ImGuiCol_TitleBg] = base;
    c[ImGuiCol_TitleBgActive] = panel;
    c[ImGuiCol_TitleBgCollapsed] = base;
    c[ImGuiCol_MenuBarBg] = base;
    c[ImGuiCol_ScrollbarBg] = panel;
    c[ImGuiCol_ScrollbarGrab] = line;
    c[ImGuiCol_ScrollbarGrabHovered] = hover;
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = ImVec4(.58F, .77F, 1, 1);
    c[ImGuiCol_Button] = field;
    c[ImGuiCol_ButtonHovered] = hover;
    c[ImGuiCol_ButtonActive] = active;
    c[ImGuiCol_Header] = active;
    c[ImGuiCol_HeaderHovered] = hover;
    c[ImGuiCol_HeaderActive] = active;
    c[ImGuiCol_Separator] = line;
    c[ImGuiCol_SeparatorHovered] = accent;
    c[ImGuiCol_SeparatorActive] = accent;
    c[ImGuiCol_Tab] = base;
    c[ImGuiCol_TabHovered] = hover;
    c[ImGuiCol_TabSelected] = field;
    c[ImGuiCol_TabSelectedOverline] = accent;
    c[ImGuiCol_TabDimmed] = base;
    c[ImGuiCol_TabDimmedSelected] = panel;
    c[ImGuiCol_TabDimmedSelectedOverline] = line;
    c[ImGuiCol_DockingPreview] = ImVec4(.369F, .655F, 1, .35F);
    c[ImGuiCol_DockingEmptyBg] = base;
    c[ImGuiCol_TableHeaderBg] = field;
    c[ImGuiCol_TableBorderStrong] = line;
    c[ImGuiCol_TableBorderLight] = panel;
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, .025F);
    c[ImGuiCol_TextSelectedBg] = ImVec4(.369F, .655F, 1, .3F);
    c[ImGuiCol_NavHighlight] = accent;
    ImFontConfig fontConfig;
    fontConfig.SizePixels = 16;
#ifdef _WIN32
    wchar_t windowsDirectory[MAX_PATH]{};
    if (GetWindowsDirectoryW(windowsDirectory, MAX_PATH) > 0) {
        const auto font = std::filesystem::path(windowsDirectory) / "Fonts/segoeui.ttf";
        if (std::filesystem::is_regular_file(font))
            io.Fonts->AddFontFromFileTTF(pathToUtf8(font).c_str(), 16, nullptr, io.Fonts->GetGlyphRangesDefault());
    }
#endif
    if (io.Fonts->Fonts.empty())
        io.Fonts->AddFontDefault(&fontConfig);
    if (!ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow *>(window.nativeHandle()), true)) {
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui GLFW initialization failed");
    }
#ifdef TWODOG_NULL_PLATFORM
    // OSMesa has no GLX/EGL loader. Use the current GLFW context's resolver.
    if (imgl3wInit2(glfwGetProcAddress) != 0) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui offscreen OpenGL initialization failed");
    }
#endif
    if (!ImGui_ImplOpenGL3_Init("#version 410 core")) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui OpenGL initialization failed");
    }
}
ImGuiSession::~ImGuiSession() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
} // namespace twodog
