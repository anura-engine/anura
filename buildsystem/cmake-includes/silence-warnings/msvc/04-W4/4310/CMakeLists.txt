# C4310: cast truncates constant value
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/variant.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4310
)
