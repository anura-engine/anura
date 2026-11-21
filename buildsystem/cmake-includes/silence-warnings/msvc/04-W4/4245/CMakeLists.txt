# C4245: 'argument': conversion from 'int' to 'unsigned int', signed/unsigned mismatch.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/custom_object_functions.cpp"
    "${ANURA_SOURCE_DIR}/src/http_client.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/AttributeSet.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/AttributeSetOGL.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/ShadersOGL.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/TextureOGL.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/TextureUtils.cpp"
    "${ANURA_SOURCE_DIR}/src/level.cpp"
    "${ANURA_SOURCE_DIR}/src/level_runner.cpp"
    "${ANURA_SOURCE_DIR}/src/main.cpp"
    "${ANURA_SOURCE_DIR}/src/md5.cpp"
    "${ANURA_SOURCE_DIR}/src/module.cpp"
    "${ANURA_SOURCE_DIR}/src/stats.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_ai_player.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_bot.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_client.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_game.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_internal_client.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_internal_server.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_matchmaking_server.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_relay_server.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_server.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_server_base.cpp"
    "${ANURA_SOURCE_DIR}/src/tbs_web_server.cpp"
    "${ANURA_SOURCE_DIR}/src/text_editor_widget.cpp"
    "${ANURA_SOURCE_DIR}/src/xhtml/css_lexer.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4245
)