# In an optimal state, this file does not exist
# XXX - The leading spaces in this file are meaningful for string concatenation!

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/module.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-missing-field-initializers"
)
