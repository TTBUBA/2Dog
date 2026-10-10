cmake_minimum_required(VERSION 3.24)
project(2Dog VERSION 0.7.3 LANGUAGES C CXX)

# Static CRT keeps the native player independent of VC++ redistributable installation.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Human-facing binaries are separate from generated CMake projects and intermediates.
set(TWODOG_OUTPUT_ROOT "${CMAKE_BINARY_DIR}/artifacts" CACHE PATH "Root for bin, lib and test outputs")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${TWODOG_OUTPUT_ROOT}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${TWODOG_OUTPUT_ROOT}/lib/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${TWODOG_OUTPUT_ROOT}/lib/$<CONFIG>")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${TWODOG_OUTPUT_ROOT}/bin/$<CONFIG>")
include(CTest)
option(TWODOG_BUILD_DESKTOP "Build GLFW desktop applications" ON)
option(TWODOG_GRAPHICS_TESTS "Register tests requiring a real OpenGL 4.1 context" OFF)
option(TWODOG_NULL_PLATFORM "Use GLFW null + OSMesa for offscreen Linux graphics tests" OFF)
option(TWODOG_SANITIZERS "Address/undefined sanitizers on GCC or Clang" OFF)
option(TWODOG_CSHARP "Build Windows x64 .NET 10 scripting host" OFF)
option(TWODOG_BUILD_GAMEPLAY "Build sprite/input/physics modules" ON)

if(TWODOG_CSHARP AND (NOT TWODOG_BUILD_DESKTOP OR NOT TWODOG_BUILD_GAMEPLAY))
    message(FATAL_ERROR "C# requires desktop and gameplay")
endif()
if(TWODOG_CSHARP)
    include(cmake/DotNet.cmake)
endif()

if(TWODOG_GRAPHICS_TESTS AND NOT TWODOG_BUILD_DESKTOP)
    message(FATAL_ERROR "Graphics tests require the desktop target")
endif()
if(TWODOG_NULL_PLATFORM AND WIN32)
    message(FATAL_ERROR "Null platform is only for non-Windows build verification")
endif()

function(twodog_target target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    if(target MATCHES "Tests$")
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${TWODOG_OUTPUT_ROOT}/tests/$<CONFIG>"
            PDB_OUTPUT_DIRECTORY "${TWODOG_OUTPUT_ROOT}/tests/$<CONFIG>")
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
        if(TWODOG_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

add_library(TwoDogCore STATIC
    src/core/FixedStepClock.cpp
    src/core/Application.cpp
    src/core/LaunchOptions.cpp
    src/core/WindowConfig.cpp
    src/data/Json.cpp
    src/data/ProjectCreation.cpp
    src/data/RecentProjects.cpp)
add_library(2Dog::Core ALIAS TwoDogCore)
target_include_directories(TwoDogCore PUBLIC include)
twodog_target(TwoDogCore)

add_library(TwoDogDevelopmentTools STATIC src/tools/ExportService.cpp)
target_link_libraries(TwoDogDevelopmentTools PUBLIC TwoDogCore)
twodog_target(TwoDogDevelopmentTools)

include(cmake/Dependencies.cmake)
if(TWODOG_BUILD_GAMEPLAY)
    add_library(TwoDogGameplay STATIC src/gameplay/Scene.cpp src/gameplay/Input.cpp src/gameplay/Physics.cpp src/data/Project.cpp src/data/ComponentSchema.cpp src/gameplay/World2D.cpp)
    target_link_libraries(TwoDogGameplay PUBLIC 2Dog::Core glm::glm PRIVATE box2d)
    twodog_target(TwoDogGameplay)
    add_library(TwoDogEditorDocument STATIC src/editor/SceneDocument.cpp)
    target_link_libraries(TwoDogEditorDocument PUBLIC TwoDogGameplay)
    twodog_target(TwoDogEditorDocument)
endif()

if(TWODOG_BUILD_DESKTOP)
    find_package(Threads REQUIRED)
    add_library(TwoDogPlatform STATIC src/platform/Window.cpp src/platform/OpenGL.cpp src/platform/Desktop.cpp)
    target_link_libraries(TwoDogPlatform PUBLIC 2Dog::Core PRIVATE glfw Threads::Threads)
    target_compile_definitions(TwoDogPlatform PRIVATE GLFW_INCLUDE_NONE)
    if(TWODOG_NULL_PLATFORM)
        target_compile_definitions(TwoDogPlatform PRIVATE TWODOG_NULL_PLATFORM=1)
    endif()
    twodog_target(TwoDogPlatform)
    if(TWODOG_BUILD_GAMEPLAY)
        target_sources(TwoDogPlatform PRIVATE src/platform/SpriteRenderer.cpp src/platform/Image.cpp src/platform/Sandbox.cpp
            src/platform/Audio.cpp src/platform/Font.cpp src/platform/SceneView.cpp
            src/platform/ProjectPlayer.cpp src/platform/GameUI.cpp src/scripting/ManagedHost.cpp)
        target_link_libraries(TwoDogPlatform PUBLIC TwoDogGameplay PRIVATE TwoDogMiniaudio)
        if(TWODOG_CSHARP)
            target_include_directories(TwoDogPlatform SYSTEM PRIVATE "${TWODOG_NETHOST_DIR}")
            target_compile_definitions(TwoDogPlatform PRIVATE TWODOG_CSHARP=1)
        endif()
        target_include_directories(TwoDogPlatform SYSTEM PRIVATE ${stb_SOURCE_DIR})
        target_compile_definitions(TwoDogPlatform PRIVATE TWODOG_GAMEPLAY=1)
    endif()
    add_executable(TwoDogRuntime apps/runtime.cpp)
    add_executable(TwoDogEditor apps/editor.cpp)
    if(WIN32)
        set_target_properties(TwoDogEditor PROPERTIES WIN32_EXECUTABLE TRUE)
        target_link_libraries(TwoDogEditor PRIVATE shell32)
    endif()
    if(TWODOG_BUILD_GAMEPLAY)
        add_library(TwoDogEditorTools STATIC 
            src/editor/Editor.cpp
            src/editor/EditorActions.cpp
            src/editor/EditorAssets.cpp
            src/editor/EditorChrome.cpp
            src/editor/EditorContext.cpp
            src/editor/EditorDefaults.cpp
            src/editor/EditorSession.cpp
            src/editor/EditorWidgets.cpp
            src/editor/panels/AssetsPanel.cpp
            src/editor/panels/ConsolePanel.cpp
            src/editor/panels/GamePanel.cpp
            src/editor/panels/HierarchyPanel.cpp
            src/editor/panels/InspectorPanel.cpp
            src/editor/panels/ScenePanel.cpp
            src/editor/RenderTarget.cpp src/editor/ScriptWorkspace.cpp src/editor/WindowsExport.cpp)
        target_link_libraries(TwoDogEditorTools PUBLIC TwoDogPlatform PRIVATE TwoDogEditorDocument TwoDogDevelopmentTools TwoDogImGui glfw)
        target_compile_definitions(TwoDogEditorTools PRIVATE GLFW_INCLUDE_NONE)
        if(TWODOG_NULL_PLATFORM)
            target_compile_definitions(TwoDogEditorTools PRIVATE TWODOG_NULL_PLATFORM=1)
        endif()
        if(WIN32)
            target_link_libraries(TwoDogEditorTools PRIVATE comdlg32 shell32 ole32 uuid)
        endif()
        file(GLOB TWODOG_BUILTIN_SCRIPTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/managed/GameScripts/*.cs")
        file(GLOB TWODOG_API_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/managed/TwoDog.API/*.cs")
        # Refresh support scripts even when no C++ file needs relinking.
        add_custom_target(TwoDogEditorSupport
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:TwoDogEditor>/EditorSupport/Defaults"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TWODOG_BUILTIN_SCRIPTS} "$<TARGET_FILE_DIR:TwoDogEditor>/EditorSupport/Defaults"
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:TwoDogEditor>/EditorSupport/API"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TWODOG_API_SOURCES} "$<TARGET_FILE_DIR:TwoDogEditor>/EditorSupport/API"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/scripts/open-script.ps1" "$<TARGET_FILE_DIR:TwoDogEditor>/EditorSupport/open-script.ps1"
            VERBATIM)
        add_custom_command(TARGET TwoDogEditorSupport POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/scripts/export-windows.ps1"
                "$<TARGET_FILE_DIR:TwoDogEditor>/EditorSupport/export-windows.ps1" VERBATIM)
        add_dependencies(TwoDogEditor TwoDogEditorSupport)
        twodog_target(TwoDogEditorTools)
        target_link_libraries(TwoDogEditor PRIVATE TwoDogEditorTools)
        target_compile_definitions(TwoDogEditor PRIVATE TWODOG_EDITOR=1)
        if(WIN32)
            add_executable(TwoDogHub WIN32 src/hub/Hub.cpp)
            target_link_libraries(TwoDogHub PRIVATE TwoDogCore ole32 shell32 comdlg32 uuid dwmapi uxtheme)
            target_compile_definitions(TwoDogHub PRIVATE UNICODE _UNICODE)
            twodog_target(TwoDogHub)
            add_dependencies(TwoDogHub TwoDogEditor)
        endif()
    endif()
    if(WIN32 AND TWODOG_CSHARP)
        include(cmake/WindowsExport.cmake)
    endif()
    foreach(app TwoDogRuntime TwoDogEditor)
        target_link_libraries(${app} PRIVATE TwoDogPlatform)
        twodog_target(${app})
        if(TWODOG_CSHARP)
            add_dependencies(${app} TwoDogManaged)
        endif()
        if(TWODOG_BUILD_GAMEPLAY)
            add_custom_command(TARGET ${app} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:${app}>/assets"
                COMMAND ${CMAKE_COMMAND} "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/samples/CSharpDemo" "-DDESTINATION=$<TARGET_FILE_DIR:${app}>/projects/CSharpDemo" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyProject.cmake"
                COMMAND ${CMAKE_COMMAND} "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/samples/EditorDemo" "-DDESTINATION=$<TARGET_FILE_DIR:${app}>/projects/EditorDemo" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyProject.cmake" VERBATIM)
        endif()
    endforeach()
endif()

if(BUILD_TESTING AND TWODOG_BUILD_GAMEPLAY)
    add_executable(TwoDogComponentSchemaTests tests/component_schema_tests.cpp)
    target_link_libraries(TwoDogComponentSchemaTests PRIVATE TwoDogEditorDocument)
    twodog_target(TwoDogComponentSchemaTests)
    foreach(case defaults collider_geometry material_and_body visual_rules shared_transaction)
        add_test(NAME components.${case} COMMAND TwoDogComponentSchemaTests ${case} "${CMAKE_CURRENT_SOURCE_DIR}/samples/CSharpDemo")
        set_tests_properties(components.${case} PROPERTIES LABELS "components" TIMEOUT 20)
    endforeach()
    add_executable(TwoDogEditorDocumentTests tests/editor_document_tests.cpp)
    target_link_libraries(TwoDogEditorDocumentTests PRIVATE TwoDogEditorDocument)
    twodog_target(TwoDogEditorDocumentTests)
    foreach(case history_selection transaction branch_and_saved script play_lock replace_identity delete_references history_limit)
        add_test(NAME editor.${case} COMMAND TwoDogEditorDocumentTests ${case} "${CMAKE_CURRENT_SOURCE_DIR}/samples/CSharpDemo")
        set_tests_properties(editor.${case} PROPERTIES LABELS "editor" TIMEOUT 20)
    endforeach()
    add_executable(TwoDogDataTests tests/data_tests.cpp)
    target_link_libraries(TwoDogDataTests PRIVATE TwoDogGameplay)
    twodog_target(TwoDogDataTests)
    foreach(case json_roundtrip json_reject json_limits path_containment scene_roundtrip scene_duplicate prefab_instances scene_transaction physics_compound physics_more_joints raycast_ignore project_cli)
        add_test(NAME data.${case} COMMAND TwoDogDataTests ${case} "${CMAKE_CURRENT_SOURCE_DIR}/samples/CSharpDemo")
        set_tests_properties(data.${case} PROPERTIES LABELS "data;v03" TIMEOUT 20)
    endforeach()
    add_executable(TwoDogGameplayTests tests/gameplay_tests.cpp)
    target_link_libraries(TwoDogGameplayTests PRIVATE TwoDogGameplay)
    twodog_target(TwoDogGameplayTests)
    foreach(case entity_generation scene_destroy camera_roundtrip animation input_edges input_multibind input_focus input_validation physics_gravity physics_bounce physics_trigger physics_raycast physics_destroy physics_joint physics_validation physics_force physics_kinematic)
        add_test(NAME gameplay.${case} COMMAND TwoDogGameplayTests ${case})
        set_tests_properties(gameplay.${case} PROPERTIES LABELS "gameplay" TIMEOUT 20)
    endforeach()
    if(TWODOG_BUILD_DESKTOP)
        add_executable(TwoDogUiTests tests/ui_tests.cpp)
        target_link_libraries(TwoDogUiTests PRIVATE TwoDogPlatform)
        twodog_target(TwoDogUiTests)
        foreach(case anchors scaling layout cycle hidden validation capture canvas canvas_validation)
            add_test(NAME ui.${case} COMMAND TwoDogUiTests ${case})
            set_tests_properties(ui.${case} PROPERTIES LABELS "ui;v04" TIMEOUT 10)
        endforeach()
        add_executable(TwoDogSpriteTests tests/sprite_tests.cpp)
        target_link_libraries(TwoDogSpriteTests PRIVATE TwoDogPlatform)
        twodog_target(TwoDogSpriteTests)
        add_executable(TwoDogSandboxTests tests/sandbox_tests.cpp)
        target_link_libraries(TwoDogSandboxTests PRIVATE TwoDogPlatform)
        twodog_target(TwoDogSandboxTests)
        add_dependencies(TwoDogSandboxTests TwoDogRuntime)
        add_custom_command(TARGET TwoDogSandboxTests POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:TwoDogSandboxTests>/assets")
        if(TWODOG_GRAPHICS_TESTS)
            add_test(NAME graphics.sprites COMMAND TwoDogSpriteTests)
            add_test(NAME graphics.sandbox_controls COMMAND TwoDogSandboxTests)
            set_tests_properties(graphics.sprites PROPERTIES LABELS "graphics" TIMEOUT 30 RUN_SERIAL TRUE)
            set_tests_properties(graphics.sandbox_controls PROPERTIES LABELS "graphics" TIMEOUT 30 RUN_SERIAL TRUE)
        endif()
    endif()
endif()

if(BUILD_TESTING)
    add_executable(TwoDogExportServiceTests tests/export_service_tests.cpp)
    target_link_libraries(TwoDogExportServiceTests PRIVATE TwoDogDevelopmentTools)
    twodog_target(TwoDogExportServiceTests)
    foreach(case defaults settings_roundtrip legacy_settings)
        add_test(NAME export_service.${case} COMMAND TwoDogExportServiceTests ${case})
        set_tests_properties(export_service.${case} PROPERTIES LABELS "tools" TIMEOUT 10)
    endforeach()
    add_executable(TwoDogWorkspaceFixture tests/workspace_fixture.cpp src/editor/ScriptWorkspace.cpp)
    target_link_libraries(TwoDogWorkspaceFixture PRIVATE TwoDogCore)
    twodog_target(TwoDogWorkspaceFixture)
    set_target_properties(TwoDogWorkspaceFixture PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TWODOG_OUTPUT_ROOT}/tests/$<CONFIG>")
    add_executable(TwoDogWorkspaceTests tests/workspace_tests.cpp src/editor/ScriptWorkspace.cpp)
    target_link_libraries(TwoDogWorkspaceTests PRIVATE TwoDogCore)
    twodog_target(TwoDogWorkspaceTests)
    if(TWODOG_BUILD_GAMEPLAY)
        target_link_libraries(TwoDogWorkspaceTests PRIVATE TwoDogGameplay)
        target_compile_definitions(TwoDogWorkspaceTests PRIVATE TWODOG_WORKSPACE_GAMEPLAY=1)
    endif()
    foreach(case create_empty refuse_overwrite invalid_name workspace_tree workspace_empty recent_projects)
        add_test(NAME workspace.${case} COMMAND TwoDogWorkspaceTests ${case})
        set_tests_properties(workspace.${case} PROPERTIES LABELS "workspace;v06" TIMEOUT 20)
    endforeach()
    add_executable(TwoDogCoreTests tests/core_tests.cpp)
    target_link_libraries(TwoDogCoreTests PRIVATE 2Dog::Core)
    twodog_target(TwoDogCoreTests)
    set(core_cases
        clock_initial clock_step clock_fraction clock_multiple clock_partition
        clock_clamp clock_overload clock_invalid_config clock_invalid_delta clock_reset
        clock_long_run clock_conservation clock_extreme_delta
        options_default options_flags options_frames options_invalid options_help
        config_default config_invalid
        loop_frame_limit loop_initial_close loop_poll_close loop_fixed_before_render
        loop_suspend loop_resume loop_escape_exception loop_wait_close)
    foreach(case IN LISTS core_cases)
        add_test(NAME core.${case} COMMAND TwoDogCoreTests ${case})
        set_tests_properties(core.${case} PROPERTIES LABELS "core" TIMEOUT 10)
    endforeach()
    if(TWODOG_BUILD_DESKTOP)
        add_test(NAME cli.runtime_help COMMAND TwoDogRuntime --help)
        add_test(NAME cli.editor_help COMMAND TwoDogEditor --help)
        add_test(NAME cli.reject_unknown COMMAND TwoDogRuntime --unknown)
        set_tests_properties(cli.reject_unknown PROPERTIES WILL_FAIL TRUE)
        set_tests_properties(cli.runtime_help cli.editor_help cli.reject_unknown PROPERTIES LABELS "cli" TIMEOUT 10)
        add_executable(TwoDogWindowTests tests/window_tests.cpp)
        target_link_libraries(TwoDogWindowTests PRIVATE TwoDogPlatform Threads::Threads)
        twodog_target(TwoDogWindowTests)
        if(TWODOG_GRAPHICS_TESTS)
            add_test(NAME graphics.window_lifecycle COMMAND TwoDogWindowTests)
            add_test(NAME graphics.runtime_smoke COMMAND TwoDogRuntime --smoke-test)
            add_test(NAME graphics.editor_smoke COMMAND TwoDogEditor --smoke-test)
            set_tests_properties(graphics.window_lifecycle graphics.runtime_smoke graphics.editor_smoke
                PROPERTIES LABELS "graphics" TIMEOUT 30 RUN_SERIAL TRUE)
        endif()
    endif()
endif()
