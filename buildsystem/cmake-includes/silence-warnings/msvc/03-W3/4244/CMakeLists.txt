# C4244: conversion to a smaller type may lose data.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/custom_object_functions.cpp"
    "${ANURA_SOURCE_DIR}/src/joystick.cpp"
    "${ANURA_SOURCE_DIR}/src/particle_system.cpp"
    "${ANURA_SOURCE_DIR}/src/sys.cpp"
    "${ANURA_SOURCE_DIR}/src/theme_imgui.cpp"
    "${ANURA_SOURCE_DIR}/src/utility_object_compiler.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4244
)
