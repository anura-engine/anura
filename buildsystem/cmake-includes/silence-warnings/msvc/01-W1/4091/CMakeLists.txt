# C4091: typedef or attribute is ignored when no variable is declared.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/StackWalker.cpp"
    "${ANURA_SOURCE_DIR}/src/asserts.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4091
)
