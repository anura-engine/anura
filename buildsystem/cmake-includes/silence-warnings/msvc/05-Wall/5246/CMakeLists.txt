# C5246: nested aggregate initialization should use braces
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/editor.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd5246
)
