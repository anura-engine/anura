# C4100: unreferenced formal parameter
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/LayerBlitInfo.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4100
)