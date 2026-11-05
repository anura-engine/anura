# C4668: undefined preprocessor macro replaced with 0
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/preferences.cpp"
    "${ANURA_SOURCE_DIR}/src/sound.cpp"
    "${ANURA_SOURCE_DIR}/src/stats.cpp"
    "${ANURA_SOURCE_DIR}/src/sys.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4668
)
