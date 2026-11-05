# C4371: class layout may change due to improved packing
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/custom_object_dialog.cpp"
    "${ANURA_SOURCE_DIR}/src/file_chooser_dialog.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/RenderTarget.cpp"
    "${ANURA_SOURCE_DIR}/src/particle_system.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_matchmaking_server.cpp"
    "${ANURA_SOURCE_DIR}/src/water_particle_system.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4371
)
