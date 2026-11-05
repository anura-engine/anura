# C4702: control may reach end of non-void function, goto label with no code on both sides
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/VoronoiDiagramGenerator.cpp"
    "${ANURA_SOURCE_DIR}/src/auto_update_window.cpp"
    "${ANURA_SOURCE_DIR}/src/custom_object_functions.cpp"
    "${ANURA_SOURCE_DIR}/src/draw_scene.cpp"
    "${ANURA_SOURCE_DIR}/src/editor.cpp"
    "${ANURA_SOURCE_DIR}/src/filesystem.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_function.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/SurfaceSDL.cpp"
    "${ANURA_SOURCE_DIR}/src/level.cpp"
    "${ANURA_SOURCE_DIR}/src/multiplayer.cpp"
    "${ANURA_SOURCE_DIR}/src/playable_custom_object.cpp"
    "${ANURA_SOURCE_DIR}/src/string_utils.cpp"
    "${ANURA_SOURCE_DIR}/src/svg/svg_gradient.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_server.cpp"
    "${ANURA_SOURCE_DIR}/src/tiled/tiled.cpp"
    "${ANURA_SOURCE_DIR}/src/unit_test.cpp"
    "${ANURA_SOURCE_DIR}/src/xhtml/css_properties.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4702
)
