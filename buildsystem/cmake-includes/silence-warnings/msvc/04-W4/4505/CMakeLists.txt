# C4505: unreferenced local function has been removed
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/kre/VGraph.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4505
)
