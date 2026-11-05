# C4838: implicit narrowing conversion in aggregate or list initialization.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/cairo.cpp"
    "${ANURA_SOURCE_DIR}/src/custom_object.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/FontSDL.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/SurfaceSDL.cpp"
    "${ANURA_SOURCE_DIR}/src/md5.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4838
)
