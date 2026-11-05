# C4355: 'this' used in base member initializer list
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/ParticleSystemWidget.cpp"
    "${ANURA_SOURCE_DIR}/src/character_editor_dialog.cpp"
    "${ANURA_SOURCE_DIR}/src/checkbox.cpp"
    "${ANURA_SOURCE_DIR}/src/custom_object.cpp"
    "${ANURA_SOURCE_DIR}/src/custom_object_callable.cpp"
    "${ANURA_SOURCE_DIR}/src/ffl_dom.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_garbage_collector.cpp"
    "${ANURA_SOURCE_DIR}/src/hex/hex_mask.cpp"
    "${ANURA_SOURCE_DIR}/src/hex/hex_renderable.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/CameraObject.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/ParticleSystem.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/SceneGraph.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/SceneNode.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/SceneObject.cpp"
    "${ANURA_SOURCE_DIR}/src/level.cpp"
    "${ANURA_SOURCE_DIR}/src/main.cpp"
    "${ANURA_SOURCE_DIR}/src/particle_system_proxy.cpp"
    "${ANURA_SOURCE_DIR}/src/playable_custom_object.cpp"
    "${ANURA_SOURCE_DIR}/src/svg/svg_element.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_game.cpp"
    "${ANURA_SOURCE_DIR}/src/tiled/tiled.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4355
)
