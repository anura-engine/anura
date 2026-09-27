# BEGIN CMake setup

# 3.12 added add_compile_definitions
cmake_minimum_required(VERSION 3.12)

# Default to debug builds
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Default to using LTO for Release builds
if (CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# END CMake setup

# BEGIN Project config

# Build target for CXX project "anura"
project(anura LANGUAGES CXX)

get_filename_component(ANURA_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# Build time dependencies
find_package(Boost REQUIRED)
find_package(cairo CONFIG REQUIRED)
find_package(glew CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
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

# Use ccache to accelerate iterating on CXX files, if available
find_program(CCACHE "ccache")
if(CCACHE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE}")
endif(CCACHE)

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

# Add the headers
include_directories(
    "${CMAKE_SOURCE_DIR}/build/src/glm"
    "${CMAKE_SOURCE_DIR}/build/src/imgui"
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

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Turn off (GNU) extensions (-std=c++17 vs. -std=gnu++17)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_BUILD_TYPE MATCHES Debug)
endif()

if (CMAKE_BUILD_TYPE MATCHES Release)
    # Optimize release builds
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

# Inject our own imgui config for our own vector operations
add_compile_definitions(
    GLM_ENABLE_EXPERIMENTAL
    IMGUI_USER_CONFIG="${ANURA_SOURCE_DIR}/src/imgui_additions/imconfig_anura.h"
    NOMINMAX
)

if (CMAKE_BUILD_TYPE MATCHES Release)
    # Turn all things debug, like assertions, off for a release build
    add_compile_definitions(NDEBUG)
endif()

# Bad things, to get rid of

# Use imgui provided vector math
# XXX - imgui_custom.cpp relies on these
add_compile_definitions(IMGUI_DEFINE_MATH_OPERATORS)

# END Compiler config

# BEGIN Linker config

# Staticly link dependencies in
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

# Output an executable from the anura sources for anura
add_executable(anura "${anura_SRC}")

target_link_libraries(
    anura
    PRIVATE
    boost::boost
    cairo::cairo
    GLEW::GLEW
    SDL2_image::SDL2_image
    libwebp::libwebp
    sdl_ttf::sdl_ttf
    Freetype::Freetype
    SDL2::SDL2main
    vorbis::vorbis
)

# END Linker config
