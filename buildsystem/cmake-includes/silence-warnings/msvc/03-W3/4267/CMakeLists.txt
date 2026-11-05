# C4267: conversion from size_t to a smaller type may lose data.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/ParticleSystemWidget.cpp"
    "${ANURA_SOURCE_DIR}/src/anura_shader.cpp"
    "${ANURA_SOURCE_DIR}/src/debug_console.cpp"
    "${ANURA_SOURCE_DIR}/src/filesystem.cpp"
    "${ANURA_SOURCE_DIR}/src/formula.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_callable_definition.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_function.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_garbage_collector.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_vm.cpp"
    "${ANURA_SOURCE_DIR}/src/hex/tile_rules.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/Color.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/FboOGL.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/FontSTB.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/Gradients.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/ParticleSystem.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/ParticleSystemAffectors.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/ParticleSystemEmitters.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/Surface.cpp"
    "${ANURA_SOURCE_DIR}/src/multiplayer.cpp"
    "${ANURA_SOURCE_DIR}/src/particle_system_proxy.cpp"
    "${ANURA_SOURCE_DIR}/src/sound.cpp"
    "${ANURA_SOURCE_DIR}/src/svg/easy_svg.cpp"
    "${ANURA_SOURCE_DIR}/src/svg/svg_shapes.cpp"
    "${ANURA_SOURCE_DIR}/src/svg/svg_utils.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_matchmaking_server.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_server_base.cpp"
    "${ANURA_SOURCE_DIR}/src/xhtml/xhtml_text_box.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4267
)
