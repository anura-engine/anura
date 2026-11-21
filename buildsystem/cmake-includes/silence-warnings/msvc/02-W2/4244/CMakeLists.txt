# C4244: conversion to a smaller type may lose data.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/cairo.cpp"
    "${ANURA_SOURCE_DIR}/src/draw_scene.cpp"
    "${ANURA_SOURCE_DIR}/src/entity.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_profiler.cpp"
    "${ANURA_SOURCE_DIR}/src/frame.cpp"
    "${ANURA_SOURCE_DIR}/src/graphical_font.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/ParticleSystemUI.cpp"
    "${ANURA_SOURCE_DIR}/src/level.cpp"
    "${ANURA_SOURCE_DIR}/src/level_runner.cpp"
    "${ANURA_SOURCE_DIR}/src/rectangle_rotator.cpp"
    "${ANURA_SOURCE_DIR}/src/xhtml/css_transition.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4244
)
