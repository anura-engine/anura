# C5262: implicit fall-through in switch statement
set_property(
    SOURCE
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_draw.cpp"
    "${ANURA_SOURCE_DIR}/src/editor.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_function.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/TextureOGL.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/UniformBuffer.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/UniformBufferOGL.cpp"
    "${ANURA_SOURCE_DIR}/src/level.cpp"
    "${ANURA_SOURCE_DIR}/src/level_object.cpp"
    "${ANURA_SOURCE_DIR}/src/level_runner.cpp"
    "${ANURA_SOURCE_DIR}/src/text_editor_widget.cpp"
    "${ANURA_SOURCE_DIR}/src/variant.cpp"
    "${ANURA_SOURCE_DIR}/src/xhtml/css_styles.cpp"
    "${ANURA_SOURCE_DIR}/src/xhtml/xhtml_text_box.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd5262
)
