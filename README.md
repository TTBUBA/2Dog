# 2Dog Game Engine

A modern, lightweight 2D game engine written in C++20 with a visual editor, gameplay systems, and C# scripting support.

Version: 0.7.3
License: MIT
Status: Active development

<p align="center">
  <img src="assets/demo_atlas.tga" alt="2Dog demo atlas" width="720" />
</p>

---

## Why 2Dog?

2Dog is built for creators who want a compact but capable 2D engine with:

- a real gameplay loop and physics at the core
- an editor for scene authoring and runtime preview
- typed C# scripting with API support
- modular architecture for engine/editor work
- a build flow based on CMake and pinned dependency versions

This project aims to combine engine logic, authoring tools, and scripting in one package.

---

## Main features

### Core engine
- Fixed-step game loop and timing control
- Window and OpenGL desktop host
- Sprite rendering and layered scene composition
- Physics with Box2D
- Input handling and simulation updates
- JSON/project persistence and editor sessions

### Editor
- ImGui-based editor layout
- Hierarchy, Inspector, Assets, Scene, Game, Console panels
- Undo/redo and document-based scene editing
- Runtime preview and play mode
- Project workspace and export tooling

### Scripting
- C# runtime integration
- Typed component APIs
- Scripted game logic and scene behavior
- .NET hosting integrated into the engine pipeline

### Tooling and distribution
- CMake presets and build targets
- Asset copying and project packaging
- Export support for finished builds
- Native dependency management with version pinning and SHA checks

---

## Repository structure

```text
2Dog/
├── apps/                  runtime/editor entry points
├── assets/                demos, atlases, fonts, sounds
├── cmake/                 build script fragments and dependency setup
├── docs/                  architecture and product docs
├── include/               public engine headers
├── managed/               C# host and scripts
├── samples/               demo projects
├── scripts/               automation and support scripts
├── src/                   engine sources
├── tests/                 automated unit tests
├── CMakeLists.txt         main build definition
├── CMakePresets.json      project presets
├── README.md              this file
├── THIRD_PARTY.md         library attributions
├── LICENSE                MIT license
└── .gitignore
```

---

## Quick start

### Prerequisites
- CMake 3.24+
- C++20 compiler
- Visual Studio 2022 on Windows or GCC/Clang on Linux/macOS
- .NET 10 SDK for C# scripting integration

### Configure

Windows (recommended):
```bash
cmake --preset windows-csharp-debug
```

Headless/core-only validation:
```bash
cmake --preset headless
```

### Build
```bash
cmake --build build --config Debug
```

### Run
```bash
# Editor
./build/artifacts/bin/Debug/TwoDogEditor

# Runtime playground
./build/artifacts/bin/Debug/TwoDogRuntime
```

---

## Screenshot gallery

The project already includes demo resources and can be extended with real editor screenshots as the engine grows.

If you want to add polished visuals, place files in a dedicated folder like this:

```text
assets/screenshots/
├── editor-overview.png
├── scene-view.png
├── game-preview.png
├── inspector-panel.png
└── project-hub.png
```

Then reference them in the README as:

```markdown
![Editor overview](assets/screenshots/editor-overview.png)
```

Example preview using repository assets:

<p align="center">
  <img src="assets/demo_atlas.tga" alt="2Dog atlas preview" width="640" />
</p>

---

## Documentation

The project already contains substantial documentation in the `docs/` folder:

- `docs/ARCHITECTURE.md`
- `docs/EDITOR.md`
- `docs/PROJECT_FORMAT.md`
- `docs/ROADMAP.md`
- `docs/SCRIPTING_CSHARP.md`
- `docs/BUILD_LAYOUT.md`

This documentation is the best place to understand the engine architecture and editor workflows in detail.

---

## Supported stacks

- C++20 core runtime
- OpenGL desktop backend
- GLFW for windowing and input
- GLM for math
- Box2D for physics
- Dear ImGui for editor UI
- miniaudio for sound
- C# host for gameplay scripting

---

## Build and test notes

The project includes multiple build presets and testing flows:

```bash
ctest --preset windows-debug
ctest --preset headless
```

The build configuration is designed to support debugging, gameplay, editor authoring, and optional C# integration.

---

## Roadmap

The current direction is focused on:

- editor tool maturity
- typed component scripting
- project and asset authoring workflows
- distribution/export pipeline
- improved runtime and editor polish

See `docs/ROADMAP.md` for the detailed roadmap.

---

## License

This project is distributed under the MIT license. See `LICENSE` for details.

Third-party attribution is documented in `THIRD_PARTY.md`.

---

## Contributing

Contributions are welcome. The project is well-structured around engine modules, editor features, and script/managed support.

Typical contribution workflows:

1. Open a feature branch
2. Implement and test changes
3. Add or update relevant docs
4. Submit a pull request with a clear summary

---

## Contact and project links

- GitHub: https://github.com/TTBUBA/2Dog
- Issues: https://github.com/TTBUBA/2Dog/issues
- Docs: https://github.com/TTBUBA/2Dog/tree/main/docs

<p align="center">
  <img src="assets/demo_atlas.tga" alt="2Dog final preview" width="480" />
</p>
