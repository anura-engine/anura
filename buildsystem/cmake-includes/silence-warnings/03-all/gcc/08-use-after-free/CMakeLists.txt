# In an optimal state, this file does not exist
# XXX - The leading spaces in this file are meaningful for string concatenation!

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/formula_function.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-use-after-free"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/hex/hex_map.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-use-after-free"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/sound.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-use-after-free"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/text_editor_widget.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-use-after-free"
)
