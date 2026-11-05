# C4701: potentially uninitialized local variable used
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/VoronoiDiagramGenerator.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/Surface.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4701
)
