# In an optimal state, this file does not exist
# XXX - The leading spaces in this file are meaningful for string concatenation!

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/cairo.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-dangling-reference"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/code_editor_dialog.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-dangling-reference"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/code_editor_widget.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-dangling-reference"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/custom_object_functions.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-dangling-reference"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/sound.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-dangling-reference"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/stats.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-dangling-reference"
)
