# In an optimal state, this file does not exist
# XXX - The leading spaces in this file are meaningful for string concatenation!

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/formula_function.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-format-pedantic"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/formula_garbage_collector.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-format-pedantic"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/particle_system_proxy.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-format-pedantic"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/utility_object_compiler.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-format-pedantic"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/variant.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-format-pedantic"
)
