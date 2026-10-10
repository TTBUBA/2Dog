# Framework-dependent .NET 10 host. Only enabled by the windows-csharp preset.
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "TWODOG_CSHARP requires Windows x64")
endif()
find_program(TWODOG_DOTNET_EXECUTABLE NAMES dotnet REQUIRED)
execute_process(COMMAND "${TWODOG_DOTNET_EXECUTABLE}" --list-sdks
    OUTPUT_VARIABLE sdk_list RESULT_VARIABLE sdk_result OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT sdk_result EQUAL 0 OR NOT sdk_list MATCHES "(^|\n)10\\.[0-9]+\\.[0-9]+[^\n]*")
    message(FATAL_ERROR "Install the .NET 10 SDK x64; dotnet --list-sdks must list version 10.x")
endif()
# Resolve the host pack from the installation that owns dotnet.exe.
get_filename_component(dotnet_real "${TWODOG_DOTNET_EXECUTABLE}" REALPATH)
get_filename_component(dotnet_root "${dotnet_real}" DIRECTORY)
file(GLOB host_pack_candidates LIST_DIRECTORIES TRUE
    "${dotnet_root}/packs/Microsoft.NETCore.App.Host.win-x64/10.*")
list(SORT host_pack_candidates COMPARE NATURAL ORDER DESCENDING)
if(NOT host_pack_candidates)
    message(FATAL_ERROR "The x64 .NET 10 host pack is missing under ${dotnet_root}/packs")
endif()
list(GET host_pack_candidates 0 host_pack)
set(TWODOG_NETHOST_DIR "${host_pack}/runtimes/win-x64/native")
foreach(file nethost.h hostfxr.h coreclr_delegates.h nethost.dll)
    if(NOT EXISTS "${TWODOG_NETHOST_DIR}/${file}")
        message(FATAL_ERROR "Missing .NET host file: ${TWODOG_NETHOST_DIR}/${file}")
    endif()
endforeach()
find_program(TWODOG_POWERSHELL NAMES pwsh powershell REQUIRED)
add_custom_target(TwoDogManaged ALL
    COMMAND "${TWODOG_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-csharp.ps1"
        -Configuration "$<CONFIG>" -Output "${TWODOG_OUTPUT_ROOT}/bin/$<CONFIG>/managed"
        -DotNet "${TWODOG_DOTNET_EXECUTABLE}" -RebuildHost
    COMMAND ${CMAKE_COMMAND} -E make_directory "${TWODOG_OUTPUT_ROOT}/bin/$<CONFIG>/managed/native"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TWODOG_NETHOST_DIR}/nethost.dll"
        "${TWODOG_OUTPUT_ROOT}/bin/$<CONFIG>/managed/native/nethost.dll"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" VERBATIM)
