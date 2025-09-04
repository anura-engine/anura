# In an optimal state, this file does not exist
# XXX - The leading spaces in this file are meaningful for string concatenation!

set_property(
    SOURCE "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_widgets.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-uninitialized-const-pointer"
)
