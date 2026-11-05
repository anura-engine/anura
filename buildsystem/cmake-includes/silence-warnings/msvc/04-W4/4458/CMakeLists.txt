# C4458: declaration of local variable hides previous local declaration, declaration of label nested in label group
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/ParticleSystemWidget.cpp"
    "${ANURA_SOURCE_DIR}/src/VoronoiDiagramGenerator.cpp"
    "${ANURA_SOURCE_DIR}/src/custom_object.cpp"
    "${ANURA_SOURCE_DIR}/src/ffl_dom.cpp"
    "${ANURA_SOURCE_DIR}/src/formula_object.cpp"
    "${ANURA_SOURCE_DIR}/src/hex/hex_mask.cpp"
    "${ANURA_SOURCE_DIR}/src/hex/hex_renderable.cpp"
    "${ANURA_SOURCE_DIR}/src/json_parser.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/CameraObject.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/ParticleSystem.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/SceneGraph.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/SceneNode.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/SceneObject.cpp"
    "${ANURA_SOURCE_DIR}/src/level.cpp"
    "${ANURA_SOURCE_DIR}/src/main.cpp"
    "${ANURA_SOURCE_DIR}/src/particle_system_proxy.cpp"
    "${ANURA_SOURCE_DIR}/src/sound.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_matchmaking_server.cpp"
    "${ANURA_SOURCE_DIR}/src/tile_map.cpp"
    "${ANURA_SOURCE_DIR}/src/tiled/tiled.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4458
)