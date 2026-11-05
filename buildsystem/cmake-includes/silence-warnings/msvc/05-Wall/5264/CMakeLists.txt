# C5264: const variable is not used
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/kre/Frustum.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/SceneNode.cpp"
    "${ANURA_SOURCE_DIR}/src/module.cpp"
    "${ANURA_SOURCE_DIR}/src/module_web_server.cpp"
    "${ANURA_SOURCE_DIR}/src/multiplayer.cpp"
    "${ANURA_SOURCE_DIR}/src/xhtml/xhtml_text_node.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd5264
)
