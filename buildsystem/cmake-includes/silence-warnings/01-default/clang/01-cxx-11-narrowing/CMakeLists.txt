# In an optimal state, this file does not exist
# XXX - The leading spaces in this file are meaningful for string concatenation!

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/cairo.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-c++11-narrowing"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/custom_object.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-c++11-narrowing"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/kre/FontSDL.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-c++11-narrowing"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/kre/SurfaceSDL.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-c++11-narrowing"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/md5.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-c++11-narrowing"
)
