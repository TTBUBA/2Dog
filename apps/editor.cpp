#include <twodog/platform/WindowsArguments.hpp>
#ifdef TWODOG_EDITOR
#include <twodog/editor/Editor.hpp>
static int runEditor(int argc, char **argv) { return twodog::editorMain(argc, argv); }
#else
#include <twodog/platform/Desktop.hpp>
static int runEditor(int argc, char **argv) {
    return twodog::desktopMain(argc, argv, twodog::ApplicationKind::EditorBootstrap);
}
#endif
#ifdef _WIN32
#include <shellapi.h>
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return 1;
    const int result = twodog::withUtf8Arguments(argc, argv, runEditor);
    LocalFree(argv);
    return result;
}
#else
int main(int argc, char **argv) { return runEditor(argc, argv); }
#endif
