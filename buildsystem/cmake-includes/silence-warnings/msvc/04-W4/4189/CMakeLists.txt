# C4189: local variable initialized but not referenced.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/simplex_noise_tests.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4189
)