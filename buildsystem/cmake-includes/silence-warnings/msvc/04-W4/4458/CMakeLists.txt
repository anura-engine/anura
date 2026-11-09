# C4458: declaration of local variable hides previous local declaration, declaration of label nested in label group
set_property(
    SOURCE
    "${ANURA_SOURCE_DIR}/src/formula_object.cpp"
    "${ANURA_SOURCE_DIR}/src/json_parser.cpp"
    "${ANURA_SOURCE_DIR}/src/tile_map.cpp"
    "${ANURA_SOURCE_DIR}/src/treetree/tree.hpp"
    "${ANURA_SOURCE_DIR}/src/VoronoiDiagramGenerator.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd4458
)