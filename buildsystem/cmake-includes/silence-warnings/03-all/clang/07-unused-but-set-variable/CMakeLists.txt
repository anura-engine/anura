# In an optimal state, this file does not exist
# XXX - The leading spaces in this file are meaningful for string concatenation!

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/auto_update_window.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/custom_object.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/formula.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/formula_garbage_collector.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/kre/FontFreetype.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/kre/TextureOGL.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/kre/WindowManager.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/level.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/level_object.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/main.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/particle_system.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/tbs_matchmaking_server.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/tile_map.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/utility_object_compiler.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/xhtml/xhtml_node.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)
