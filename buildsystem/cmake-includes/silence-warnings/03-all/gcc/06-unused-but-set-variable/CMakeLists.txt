# In an optimal state, this file does not exist
# XXX - The leading spaces in this file are meaningful for string concatenation!

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/custom_object.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/formula_tokenizer.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/graphical_font.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/joystick.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/kre/FontFreetype.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/kre/ParticleSystemAffectors.cpp"
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
    SOURCE "${CMAKE_BINARY_DIR}/src/main.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/tbs_matchmaking_server.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/xhtml/xhtml_node.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)

set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/xhtml/xhtml_text_node.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-unused-but-set-variable"
)
