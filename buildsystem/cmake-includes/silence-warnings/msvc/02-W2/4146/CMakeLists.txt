# C4146: unary minus applied to an unsigned value remains unsigned.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/variant.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4146
)
