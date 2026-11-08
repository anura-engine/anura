# C4456: declaration of local variable hides previous local declaration
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/animation_preview_widget.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4456
)