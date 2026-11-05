# C4018: signed/unsigned comparison mismatch.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/controls.cpp"
    "${ANURA_SOURCE_DIR}/src/debug_console.cpp"
    "${ANURA_SOURCE_DIR}/src/formula.cpp"
    "${ANURA_SOURCE_DIR}/src/http_client.cpp"
    "${ANURA_SOURCE_DIR}/src/module.cpp"
    "${ANURA_SOURCE_DIR}/src/variant.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4018
)
