# C4774: format string expected to be a string literal
set_property(
    SOURCE
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_widgets.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4774
)
