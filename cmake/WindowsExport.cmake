# Export requires a Release C# runtime produced by this version of the engine.
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/export/$<CONFIG>/runtime-template.json"
    CONTENT "{\"format\":1,\"engine\":\"0.7\",\"configuration\":\"$<CONFIG>\",\"architecture\":\"x64\",\"csharp\":true,\"staticCRT\":true}\n")
set(export_licenses "${CMAKE_CURRENT_BINARY_DIR}/export/licenses")
file(MAKE_DIRECTORY "${export_licenses}")
configure_file("${glfw_SOURCE_DIR}/LICENSE.md" "${export_licenses}/GLFW.txt" COPYONLY)
configure_file("${box2d_SOURCE_DIR}/LICENSE" "${export_licenses}/Box2D.txt" COPYONLY)
configure_file("${glm_SOURCE_DIR}/copying.txt" "${export_licenses}/GLM.txt" COPYONLY)
configure_file("${stb_SOURCE_DIR}/LICENSE" "${export_licenses}/stb.txt" COPYONLY)
# miniaudio is fetched as one header; it includes both offered licenses.
configure_file("${miniaudio_SOURCE_DIR}/miniaudio.h" "${export_licenses}/miniaudio-notices.h" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY.md" "${export_licenses}/THIRD_PARTY.md" COPYONLY)
add_custom_command(TARGET TwoDogRuntime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/export/$<CONFIG>/runtime-template.json"
        "$<TARGET_FILE_DIR:TwoDogRuntime>/runtime-template.json"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${export_licenses}" "$<TARGET_FILE_DIR:TwoDogRuntime>/ExportLicenses"
    VERBATIM)


# Dedicated export input: no editor, Hub, API sources, CMake intermediates or game cache.
set(runtime_template_dir "${TWODOG_OUTPUT_ROOT}/export-templates/windows-x64/$<CONFIG>")
add_custom_target(TwoDogRuntimeTemplate ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${runtime_template_dir}/managed/native" "${runtime_template_dir}/Builtin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:TwoDogRuntime>" "${runtime_template_dir}/TwoDogRuntime.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${TWODOG_OUTPUT_ROOT}/bin/$<CONFIG>/managed/TwoDog.ManagedHost.dll"
        "${TWODOG_OUTPUT_ROOT}/bin/$<CONFIG>/managed/TwoDog.ManagedHost.deps.json"
        "${TWODOG_OUTPUT_ROOT}/bin/$<CONFIG>/managed/TwoDog.ManagedHost.runtimeconfig.json"
        "${TWODOG_OUTPUT_ROOT}/bin/$<CONFIG>/managed/TwoDog.API.dll"
        "${runtime_template_dir}/managed"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TWODOG_NETHOST_DIR}/nethost.dll" "${runtime_template_dir}/managed/native/nethost.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TWODOG_BUILTIN_SCRIPTS} "${runtime_template_dir}/Builtin"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${export_licenses}" "${runtime_template_dir}/ExportLicenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/export/engine-root.txt" "${runtime_template_dir}/engine-root.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/export/$<CONFIG>/runtime-template.json" "${runtime_template_dir}/runtime-template.json"
    DEPENDS TwoDogRuntime TwoDogManaged VERBATIM)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/export/engine-root.txt" "${CMAKE_CURRENT_SOURCE_DIR}")
