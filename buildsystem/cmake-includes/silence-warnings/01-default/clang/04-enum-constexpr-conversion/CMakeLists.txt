# In an optimal state, this file does not exist
# XXX - The leading spaces in this file are meaningful for string concatenation!

set_property(
    SOURCE "${CMAKE_SOURCE_DIR}/../../src/module_web_server.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-enum-constexpr-conversion"
)

set_property(
    SOURCE "${CMAKE_SOURCE_DIR}/../../src/stats_web_server.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-enum-constexpr-conversion"
)
