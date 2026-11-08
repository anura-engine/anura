# C4189: local variable is initialized but not referenced
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/bar_widget.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4189
)