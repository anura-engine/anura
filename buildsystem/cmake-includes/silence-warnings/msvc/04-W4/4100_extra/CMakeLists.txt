# C4100: unreferenced local parameter

set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/base64.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4100
)

set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/clipboard.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4100
)

set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/VoronoiDiagramGenerator.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4100
)
