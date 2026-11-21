# C4244: conversion to a smaller type may lose data.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/custom_object.cpp"
    "${ANURA_SOURCE_DIR}/src/custom_object_functions.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_function.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_vm.cpp"
    "${ANURA_SOURCE_DIR}/src/joystick.cpp"
    "${ANURA_SOURCE_DIR}/src/particle_system.cpp"
    "${ANURA_SOURCE_DIR}/src/sound.cpp"
    "${ANURA_SOURCE_DIR}/src/sys.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_matchmaking_server.cpp"
    "${ANURA_SOURCE_DIR}/src/theme_imgui.cpp"
    "${ANURA_SOURCE_DIR}/src/utility_object_compiler.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4244
)
