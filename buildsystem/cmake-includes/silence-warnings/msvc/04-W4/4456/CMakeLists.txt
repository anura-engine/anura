# C4456: declaration of local variable hides previous local declaration
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/animation_preview_widget.cpp"
    "${ANURA_SOURCE_DIR}/src/imgui_additions/imgui_custom.cpp"
    "${ANURA_SOURCE_DIR}/src/multiplayer_server.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4456
)

# C4456 from CI evidence: imgui_additions/imgui_custom.cpp, multiplayer_server.cpp
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/imgui_additions/imgui_custom.cpp"
    "${ANURA_SOURCE_DIR}/src/multiplayer_server.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4456
)