#pragma once
namespace twodog {
enum class ApplicationKind { Runtime, EditorBootstrap };
int desktopMain(int argc, char** argv, ApplicationKind kind);
}
