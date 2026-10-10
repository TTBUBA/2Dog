include(FetchContent)
if(TWODOG_BUILD_GAMEPLAY)
    FetchContent_Declare(glm URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz
        URL_HASH SHA256=9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(box2d URL https://github.com/erincatto/box2d/archive/refs/tags/v3.1.1.tar.gz
        URL_HASH SHA256=fb6ef914b50f4312d7d921a600eabc12318bb3c55a0b8c0b90608fa4488ef2e4 DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(BOX2D_SAMPLES OFF CACHE BOOL "" FORCE)
    set(BOX2D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(BOX2D_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(BOX2D_VALIDATE OFF CACHE BOOL "" FORCE)
    set(BOX2D_SIMD OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glm box2d)
    if(TWODOG_BUILD_DESKTOP)
        FetchContent_Declare(stb URL https://github.com/nothings/stb/archive/f0569113c93ad095470c54bf34a17b36646bbbb5.tar.gz
            URL_HASH SHA256=4d05c96640ae3a8cbdafdad8d344b50ce610802f78aee80154acdb8e266282e0 DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(stb)
    endif()
endif()
if(NOT TWODOG_BUILD_DESKTOP)
    return()
endif()
# Exact release archive + digest. No moving branch and no new engine dependencies.
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)
if(TWODOG_NULL_PLATFORM)
    set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(glfw
    URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
    URL_HASH SHA256=c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(glfw)

if(TWODOG_BUILD_GAMEPLAY)
    FetchContent_Declare(miniaudio
        URL https://raw.githubusercontent.com/mackron/miniaudio/0.11.23/miniaudio.h
        URL_HASH SHA256=7e4f3f13c8fe66df2080ac3dd12a89193e3c2463cb7f067c798abd7331cd8ee6
        DOWNLOAD_NO_EXTRACT TRUE)
    FetchContent_MakeAvailable(miniaudio)
    add_library(TwoDogMiniaudio STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/Miniaudio.c)
    target_include_directories(TwoDogMiniaudio SYSTEM PUBLIC ${miniaudio_SOURCE_DIR})
    if(NOT WIN32)
        target_link_libraries(TwoDogMiniaudio PUBLIC ${CMAKE_DL_LIBS} m)
    endif()
endif()

if(TWODOG_BUILD_GAMEPLAY)
    # Dear ImGui is used only by TwoDogEditor; runtime UI uses the native renderer.
    FetchContent_Declare(imgui
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b-docking.tar.gz
        URL_HASH SHA256=466fdef9b18de15f0bb6e288e3d00ffa3d82200ec458ce5e4f724a161d9528a5
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(imgui)
    add_library(TwoDogImGui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
    target_include_directories(TwoDogImGui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${imgui_SOURCE_DIR}/misc/cpp)
    target_link_libraries(TwoDogImGui PRIVATE glfw ${CMAKE_DL_LIBS})
    target_compile_definitions(TwoDogImGui PRIVATE GLFW_INCLUDE_NONE)
    target_compile_features(TwoDogImGui PUBLIC cxx_std_20)
endif()
