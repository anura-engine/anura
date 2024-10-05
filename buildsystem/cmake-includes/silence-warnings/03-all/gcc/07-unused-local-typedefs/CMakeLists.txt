# In an optimal state, this file does not exist
# XXX - The leading spaces in this file are meaningful for string concatenation!

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/blur.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-local-typedefs"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/character_editor_dialog.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-local-typedefs"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/editor.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-local-typedefs"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/external_text_editor.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-local-typedefs"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/frame.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-local-typedefs"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/sound.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-local-typedefs"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/utility_object_compiler.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-local-typedefs"
)
