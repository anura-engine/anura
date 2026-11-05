# C4858: discarding an async future makes its destructor wait for the task.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/cairo.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4858
)
