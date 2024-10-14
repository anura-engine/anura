# In an optimal state, this file does not exist
# XXX - The leading spaces in this file are meaningful for string concatenation!

# XXX - base class should be explicitly initialized in the copy constructor
set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/animation_preview_widget.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-extra"
)

# XXX - base class should be explicitly initialized in the copy constructor
set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/anura_shader.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-extra"
)

# XXX - base class should be explicitly initialized in the copy constructor
set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/custom_object_functions.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-extra"
)

# XXX - base class should be explicitly initialized in the copy constructor
set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/formula_function.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-extra"
)

# XXX - base class should be explicitly initialized in the copy constructor
set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/kre/AttributeSet.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-extra"
)

# XXX - base class should be explicitly initialized in the copy constructor
set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/kre/Texture.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-extra"
)

# XXX - base class should be explicitly initialized in the copy constructor
set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/variant.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-extra"
)
