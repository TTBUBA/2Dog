#include <twodog/platform/WindowsArguments.hpp>
#include <twodog/platform/Desktop.hpp>
static int runRuntime(int argc, char **argv) {
    return twodog::desktopMain(argc, argv, twodog::ApplicationKind::Runtime);
}
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) { return twodog::withUtf8Arguments(argc, argv, runRuntime); }
#else
int main(int argc, char **argv) { return runRuntime(argc, argv); }
#endif
