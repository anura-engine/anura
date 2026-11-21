# C4866: call evaluation order may not be enforced
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/cairo.cpp"
    "${ANURA_SOURCE_DIR}/src/formula.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_function.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_object.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_test.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_tokenizer.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_vm.cpp"
    "${ANURA_SOURCE_DIR}/src/i18n.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/DisplayDeviceOGL.cpp"
    "${ANURA_SOURCE_DIR}/src/multiplayer.cpp"
    "${ANURA_SOURCE_DIR}/src/pathfinding.cpp"
    "${ANURA_SOURCE_DIR}/src/svg/svg_path_parse.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_matchmaking_server.cpp"
    "${ANURA_SOURCE_DIR}/src/variant_type.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4866
)
