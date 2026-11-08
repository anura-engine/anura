# C4100: unreferenced formal parameter
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/LayerBlitInfo.cpp"
    "${ANURA_SOURCE_DIR}/src/base64.cpp"
    "${ANURA_SOURCE_DIR}/src/clipboard.cpp"
    "${ANURA_SOURCE_DIR}/src/VoronoiDiagramGenerator.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4100
)