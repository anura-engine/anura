# C4305: truncation from a wider floating-point type to a narrower one.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/editor.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4305
)
