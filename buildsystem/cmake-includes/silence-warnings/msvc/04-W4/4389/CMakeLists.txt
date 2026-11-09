# C4389: '==': signed/unsigned mismatch
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/dialog.cpp"
    "${ANURA_SOURCE_DIR}/src/dropdown_widget.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_tokenizer.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_tokenizer.hpp"
    "${ANURA_SOURCE_DIR}/src/grid_widget.cpp"
    "${ANURA_SOURCE_DIR}/src/http_client.hpp"
    "${ANURA_SOURCE_DIR}/src/kre/geometry.hpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4389
)
