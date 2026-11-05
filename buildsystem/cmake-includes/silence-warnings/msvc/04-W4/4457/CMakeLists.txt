# C4457: declaration of local variable hides function parameter
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/animation_preview_widget.cpp"
    "${ANURA_SOURCE_DIR}/src/cairo.cpp"
    "${ANURA_SOURCE_DIR}/src/character_editor_dialog.cpp"
    "${ANURA_SOURCE_DIR}/src/code_editor_dialog.cpp"
    "${ANURA_SOURCE_DIR}/src/color_picker.cpp"
    "${ANURA_SOURCE_DIR}/src/custom_object.cpp"
    "${ANURA_SOURCE_DIR}/src/custom_object_type.cpp"
    "${ANURA_SOURCE_DIR}/src/draw_primitive.cpp"
    "${ANURA_SOURCE_DIR}/src/formula.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_object.cpp"
    "${ANURA_SOURCE_DIR}/src/graphical_font.cpp"
    "${ANURA_SOURCE_DIR}/src/http_client.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/BlendOGL.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/Texture.cpp"
    "${ANURA_SOURCE_DIR}/src/level_object.cpp"
    "${ANURA_SOURCE_DIR}/src/preprocessor.cpp"
    "${ANURA_SOURCE_DIR}/src/rich_text_label.cpp"
    "${ANURA_SOURCE_DIR}/src/scrollbar_widget.cpp"
    "${ANURA_SOURCE_DIR}/src/svg/svg_shapes.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_game.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_matchmaking_server.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_server.cpp"
    "${ANURA_SOURCE_DIR}/src/utility_object_compiler.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4457
)
