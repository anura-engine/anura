# C4946: reinterpret_cast used between related classes
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/cairo.cpp"
    "${ANURA_SOURCE_DIR}/src/character_editor_dialog.cpp"
    "${ANURA_SOURCE_DIR}/src/custom_object_functions.cpp"
    "${ANURA_SOURCE_DIR}/src/debug_console.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_function.cpp"
    "${ANURA_SOURCE_DIR}/src/preferences.cpp"
    "${ANURA_SOURCE_DIR}/src/sound.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_matchmaking_server.cpp"
    "${ANURA_SOURCE_DIR}/src/variant.cpp"
    "${ANURA_SOURCE_DIR}/src/variant_type.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4946
)
