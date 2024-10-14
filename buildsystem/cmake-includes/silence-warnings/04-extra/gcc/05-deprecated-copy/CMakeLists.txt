# In an optimal state, this file does not exist
# XXX - The leading spaces in this file are meaningful for string concatenation!

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/ParticleSystemWidget.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-deprecated-copy"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/level.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-deprecated-copy"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/text_editor_widget.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-deprecated-copy"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/tile_map.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-deprecated-copy"
)
