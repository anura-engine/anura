# C4457: declaration of local variable hides function parameter
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/animation_preview_widget.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4457
)