# C4457: declaration of local variable hides function parameter
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/custom_object_type.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_object.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/BlendOGL.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/Texture.cpp"
    "${ANURA_SOURCE_DIR}/src/preprocessor.cpp"
    "${ANURA_SOURCE_DIR}/src/character_editor_dialog.cpp"
    "${ANURA_SOURCE_DIR}/src/code_editor_dialog.cpp"
    "${ANURA_SOURCE_DIR}/src/color_picker.cpp"
    "${ANURA_SOURCE_DIR}/src/draw_primitive.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4457
)
