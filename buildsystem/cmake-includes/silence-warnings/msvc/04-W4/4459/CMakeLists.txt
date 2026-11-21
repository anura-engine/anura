# C4459: declaration hides global declaration
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/formula.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_function.cpp"
    "${ANURA_SOURCE_DIR}/src/i18n.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4459
)
