# In an optimal state, this file does not exist
# XXX - The leading spaces in this file are meaningful for string concatenation!

set_property(
    SOURCE "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-nontrivial-memcall"
)

set_property(
    SOURCE "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_draw.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-nontrivial-memcall"
)

set_property(
    SOURCE "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_tables.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-nontrivial-memcall"
)

set_property(
    SOURCE "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_widgets.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-nontrivial-memcall"
)

set_property(
    SOURCE "${CMAKE_SOURCE_DIR}/../../src/imgui_additions/imgui_custom.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-nontrivial-memcall"
)

set_property(
    SOURCE "${CMAKE_SOURCE_DIR}/../../src/input.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-nontrivial-memcall"
)

set_property(
    SOURCE "${CMAKE_SOURCE_DIR}/../../src/kre/ParticleSystemUI.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-nontrivial-memcall"
)

set_property(
    SOURCE "${CMAKE_SOURCE_DIR}/../../src/kre/WindowManager.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-nontrivial-memcall"
)

set_property(
    SOURCE "${CMAKE_SOURCE_DIR}/../../src/kre/imgui_impl_sdl_gl3.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-nontrivial-memcall"
)

set_property(
    SOURCE "${CMAKE_SOURCE_DIR}/../../src/theme_imgui.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-nontrivial-memcall"
)
