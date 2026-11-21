# C4245: 'argument': conversion from 'int' to 'unsigned int', signed/unsigned mismatch.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/kre/TextureUtils.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4245
)