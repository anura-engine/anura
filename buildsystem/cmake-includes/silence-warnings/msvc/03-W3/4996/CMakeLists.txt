# C4996: deprecated or unsafe CRT functions.
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/auto_update_window.cpp"
    "${ANURA_SOURCE_DIR}/src/decimal.cpp"
    "${ANURA_SOURCE_DIR}/src/ft_iface.cpp"
    "${ANURA_SOURCE_DIR}/src/i18n.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/FontFreetype.cpp"
    "${ANURA_SOURCE_DIR}/src/level_object.cpp"
    "${ANURA_SOURCE_DIR}/src/main.cpp"
    "${ANURA_SOURCE_DIR}/src/module_web_server.cpp"
    "${ANURA_SOURCE_DIR}/src/preferences.cpp"
    "${ANURA_SOURCE_DIR}/src/string_utils.cpp"
    "${ANURA_SOURCE_DIR}/src/uuid.cpp"
    "${ANURA_SOURCE_DIR}/src/xhtml/css_lexer.cpp"
    "${ANURA_SOURCE_DIR}/src/xhtml/xhtml_listitem_box.cpp"
    "${ANURA_SOURCE_DIR}/src/xhtml/xhtml_text_node.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4996
)
