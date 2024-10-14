# In an optimal state, this file does not exist
# XXX - The leading spaces in this file are meaningful for string concatenation!

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/custom_object_functions.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-implicit-fallthrough"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/formula_tokenizer.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-implicit-fallthrough"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/grid_widget.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-implicit-fallthrough"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/json_parser.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-implicit-fallthrough"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/kre/TextureOGL.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-implicit-fallthrough"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/level_runner.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-implicit-fallthrough"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/svg/svg_element.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-implicit-fallthrough"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/svg/svg_gradient.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-implicit-fallthrough"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/svg/svg_length.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-implicit-fallthrough"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/svg/svg_shapes.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-implicit-fallthrough"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/svg/svg_style.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-implicit-fallthrough"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/text_editor_widget.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-implicit-fallthrough"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/xhtml/css_styles.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-implicit-fallthrough"
)
