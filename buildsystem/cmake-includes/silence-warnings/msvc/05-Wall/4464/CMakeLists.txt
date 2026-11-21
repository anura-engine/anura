# C4464: relative include path contains '..'
set_property(
    SOURCE
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_draw.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_tables.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_widgets.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/LightObject.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/ParticleSystem.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4464
)
