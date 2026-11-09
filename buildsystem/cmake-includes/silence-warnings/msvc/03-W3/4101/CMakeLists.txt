# C4101: unreferenced local variable.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/formula.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_matchmaking_server.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4101
)
