# C4702: control may reach end of non-void function, goto label with no code on both sides
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/unit_test.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4702
)
