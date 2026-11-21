# BEGIN CMake setup

cmake_minimum_required(VERSION 4.3.1)

if(CMAKE_GENERATOR MATCHES "^Visual Studio")
    find_program(CCACHE "ccache")
endif()
if(CCACHE)
    cmake_policy(SET CMP0141 NEW)
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# END CMake setup

# BEGIN Project config

# Build target for CXX project "anura"
project(anura LANGUAGES CXX)

enable_testing()

get_filename_component(ANURA_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${ANURA_SOURCE_DIR}/buildsystem/conan/conanfile.py"
)

# Build time dependencies
find_package(Boost REQUIRED)
find_package(cairo CONFIG REQUIRED)
find_package(glew CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(WebP CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(freetype CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(Vorbis CONFIG REQUIRED)

# Fetch vendored dependencies
include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG d4ddc46e7773e9a9b68f965d007968f35ca4e09a
    GIT_SHALLOW TRUE
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/build/src/imgui"
    SOURCE_SUBDIR purposefully-empty-to-skip-build
)

FetchContent_MakeAvailable(imgui)

# XXX - 1.0.0 is the latest one which builds with C++17 on LLVM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 33b0eb9fa336ffd8551024b1d2690e418014553b
    GIT_SHALLOW TRUE
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/build/src/glm"
    SOURCE_SUBDIR purposefully-empty-to-skip-build
)

FetchContent_MakeAvailable(glm)

if(CCACHE AND MSVC AND CMAKE_GENERATOR MATCHES "^Visual Studio")
    configure_file("${CCACHE}" "${CMAKE_BINARY_DIR}/cl.exe" COPYONLY)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
    list(APPEND CMAKE_VS_GLOBALS
        "CLToolPath=${CMAKE_BINARY_DIR}"
        "CLToolExe=cl.exe"
        "UseMultiToolTask=true"
    )
endif()

# Add the source code
file(
    GLOB
    anura_SRC
    "${ANURA_SOURCE_DIR}/src/*.cpp"
    "${ANURA_SOURCE_DIR}/src/hex/*.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_draw.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_tables.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_widgets.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui.cpp"
    "${ANURA_SOURCE_DIR}/src/imgui_additions/*.cpp"
    "${ANURA_SOURCE_DIR}/src/kre/*.cpp"
    "${ANURA_SOURCE_DIR}/src/svg/*.cpp"
    "${ANURA_SOURCE_DIR}/src/tiled/*.cpp"
    "${ANURA_SOURCE_DIR}/src/treetree/*.cpp"
    "${ANURA_SOURCE_DIR}/src/xhtml/*.cpp"
)

set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_draw.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_tables.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_widgets.cpp"
    "${ANURA_SOURCE_DIR}/src/StackWalker.cpp"
    "${ANURA_SOURCE_DIR}/src/VoronoiDiagramGenerator.cpp"
    PROPERTIES COMPILE_OPTIONS /W4
)

# Add the headers
include_directories(SYSTEM
    "${CMAKE_SOURCE_DIR}/build/src/glm"
    "${CMAKE_SOURCE_DIR}/build/src/imgui"
)

include_directories(
    "${ANURA_SOURCE_DIR}/src"
    "${ANURA_SOURCE_DIR}/src/hex"
    "${ANURA_SOURCE_DIR}/src/imgui"
    "${ANURA_SOURCE_DIR}/src/imgui_additions"
    "${ANURA_SOURCE_DIR}/src/kre"
    "${ANURA_SOURCE_DIR}/src/svg"
    "${ANURA_SOURCE_DIR}/src/tiled"
    "${ANURA_SOURCE_DIR}/src/treetree"
    "${ANURA_SOURCE_DIR}/src/xhtml"
)

# END Project config

# BEGIN Compiler config

# Good things, to keep

# Inject our own imgui config for our own vector operations
add_compile_definitions(
    GLM_ENABLE_EXPERIMENTAL
    BOOST_ALL_NO_LIB
    IMGUI_USER_CONFIG="${ANURA_SOURCE_DIR}/src/imgui_additions/imconfig_anura.h"
    NOMINMAX
)

# Bad things, to get rid of

# Use imgui provided vector math
# XXX - imgui_custom.cpp relies on these
add_compile_definitions(IMGUI_DEFINE_MATH_OPERATORS)

# END Compiler config

# BEGIN Linker config

# Output an executable from the anura sources for anura
if(MSVC)
    include("${ANURA_SOURCE_DIR}/buildsystem/cmake-includes/silence-warnings/msvc/CMakeLists.txt")
endif()

add_executable(anura "${anura_SRC}")
set_target_properties(anura PROPERTIES
    DEBUGGER_WORKING_DIRECTORY "${ANURA_SOURCE_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${ANURA_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(anura PRIVATE /Wall /WX)
endif()

target_link_libraries(
    anura
    PRIVATE
    Boost::boost
    Boost::date_time
    Boost::filesystem
    Boost::locale
    Boost::regex
    cairo::cairo
    GLEW::GLEW
    SDL2_image::SDL2_image
    SDL2_mixer::SDL2_mixer
    libwebp::libwebp
    sdl_ttf::sdl_ttf
    Freetype::Freetype
    SDL2::SDL2main
    vorbis::vorbis
)

add_test(
    NAME engine_unit_tests
    COMMAND $<TARGET_FILE:anura> --tests --log-file=unit-tests.log
    WORKING_DIRECTORY "${ANURA_SOURCE_DIR}"
)
set_tests_properties(engine_unit_tests PROPERTIES LABELS "unittest")

# END Linker config
