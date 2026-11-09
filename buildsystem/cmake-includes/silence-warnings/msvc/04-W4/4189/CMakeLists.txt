# C4189: local variable is initialized but not referenced
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/bar_widget.cpp"
    "${ANURA_SOURCE_DIR}/src/simplex_noise_tests.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4189
)