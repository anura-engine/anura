# BEGIN CMake setup

# 3.12 added add_compile_definitions
# 3.14 added FetchContent
# 3.19 added presets v1: we use configurePresets
cmake_minimum_required(VERSION 3.19)

# Default to debug builds
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Prefer pkgconfig as that's what Conan will provide
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/../conan/conan_toolchain.cmake")
    message("Conan toolchain file not found. ")
    message("")
    message(
        FATAL_ERROR
        "Please run 'conan install buildsystem/conan -s build_type=Debug --build=missing' first."
    )
endif()

# Use Conan as the package manager
include(${CMAKE_SOURCE_DIR}/../conan/conan_toolchain.cmake)

# END CMake setup

# BEGIN Project config

# Build target for CXX project "anura"
project(anura LANGUAGES CXX OBJCXX)

# For running tests through CTest
enable_testing()

# Build time dependencies
find_package(Boost COMPONENTS filesystem REQUIRED)
find_package(freetype REQUIRED)
find_package(SDL2 REQUIRED CONFIG)
find_package(Cairo REQUIRED CONFIG)
find_package(GLEW REQUIRED CONFIG)
find_package(SDL2_ttf REQUIRED CONFIG)
find_package(SDL2_image REQUIRED CONFIG)
find_package(Vorbis REQUIRED CONFIG)
find_package(Git REQUIRED)
find_library(COCOA_FRAMEWORK Cocoa REQUIRED)

# Fetch vendored dependencies
include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG d4ddc46e7773e9a9b68f965d007968f35ca4e09a # https://github.com/ocornut/imgui/releases/tag/v1.89.7
    GIT_SHALLOW TRUE
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/build/src/imgui"
    SOURCE_SUBDIR purposefully-empty-to-skip-build
)

FetchContent_MakeAvailable(imgui)

# XXX - 1.0.0 is the latest one which builds with C++17 on LLVM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 33b0eb9fa336ffd8551024b1d2690e418014553b # https://github.com/g-truc/glm/releases/tag/1.0.0
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
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_draw.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_tables.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_widgets.cpp"
    "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/hex/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/imgui_additions/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/kre/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/svg/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/tiled/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/treetree/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/xhtml/*.cpp"
)

set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/../../src/main.cpp"
    PROPERTIES LANGUAGE OBJCXX
)

# Add the headers
include_directories(
    "${CMAKE_SOURCE_DIR}/build/src/glm"
    "${CMAKE_SOURCE_DIR}/build/src/imgui"
    "${CMAKE_SOURCE_DIR}/../../src"
    "${CMAKE_SOURCE_DIR}/../../src/hex"
    "${CMAKE_SOURCE_DIR}/../../src/imgui_additions"
    "${CMAKE_SOURCE_DIR}/../../src/kre"
    "${CMAKE_SOURCE_DIR}/../../src/svg"
    "${CMAKE_SOURCE_DIR}/../../src/tiled"
    "${CMAKE_SOURCE_DIR}/../../src/treetree"
    "${CMAKE_SOURCE_DIR}/../../src/xhtml"
)

# END Project config

# BEGIN Compiler config

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Turn off (GNU) extensions (-std=c++17 vs. -std=gnu++17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_EXTENSIONS OFF)

# Keep configuration-specific flags compatible with Xcode's multi-config generator.
string(APPEND CMAKE_CXX_FLAGS_DEBUG " -g3 -Og")
string(APPEND CMAKE_CXX_FLAGS_RELEASE " -O3")
string(APPEND CMAKE_OBJCXX_FLAGS_DEBUG " -g3 -Og")
string(APPEND CMAKE_OBJCXX_FLAGS_RELEASE " -O3")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

# Inject our own imgui config for our own vector operations
add_compile_definitions(IMGUI_USER_CONFIG="${CMAKE_SOURCE_DIR}/../../src/imgui_additions/imconfig_anura.h")

# Use imgui provided vector math
# XXX - imgui_custom.cpp relies on these
add_compile_definitions(IMGUI_DEFINE_MATH_OPERATORS)

# Quaternions have become experimental in GLM???
# XXX - Newer versions of GLM need this for some reason
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

# END Compiler config

# BEGIN Linker config

# Output an executable from the anura sources for anura
add_executable(anura "${anura_SRC}")

# Link libraries
target_link_libraries(
    anura
    PRIVATE
    boost::boost
    Freetype::Freetype
    SDL2::SDL2
    Boost::filesystem
    cairo::cairo
    GLEW::GLEW
    sdl_ttf::sdl_ttf
    SDL2_image::SDL2_image
    vorbis::vorbis
    ${COCOA_FRAMEWORK}
)

# END Linker config

# BEGIN Test config

add_test(
    NAME engine_unit_tests
    COMMAND $<TARGET_FILE:anura> --tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../../
)
set_tests_properties(engine_unit_tests PROPERTIES LABELS "unittest")

# END Test config
