# C4201: nonstandard extension used: nameless struct/union
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/kre/geometry.hpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4201
)
