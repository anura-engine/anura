# C5039: potentially throwing function passed to an extern "C" function
set_property(
    SOURCE
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_draw.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_tables.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_widgets.cpp"
    "${ANURA_SOURCE_DIR}/src/SceneObjectCallable.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/FontSTB.cpp"
    "${ANURA_SOURCE_DIR}/src/main.cpp"
    "${ANURA_SOURCE_DIR}/src/thread.cpp"
    APPEND PROPERTY
    COMPILE_OPTIONS /wd5039
)
