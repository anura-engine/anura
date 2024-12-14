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

# Use ccache to accelerate iterating on CXX files, if available
find_program(CCACHE "ccache")
if(CCACHE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE}")
endif(CCACHE)

# Add the source code
file(
    GLOB
    anura_SRC
    "${CMAKE_BINARY_DIR}/src/*.cpp"
    "${CMAKE_BINARY_DIR}/src/hex/*.cpp"
    "${CMAKE_BINARY_DIR}/src/imgui/imgui_draw.cpp"
    "${CMAKE_BINARY_DIR}/src/imgui/imgui_tables.cpp"
    "${CMAKE_BINARY_DIR}/src/imgui/imgui_widgets.cpp"
    "${CMAKE_BINARY_DIR}/src/imgui/imgui.cpp"
    "${CMAKE_BINARY_DIR}/src/imgui_additions/*.cpp"
    "${CMAKE_BINARY_DIR}/src/kre/*.cpp"
    "${CMAKE_BINARY_DIR}/src/svg/*.cpp"
    "${CMAKE_BINARY_DIR}/src/tiled/*.cpp"
    "${CMAKE_BINARY_DIR}/src/treetree/*.cpp"
    "${CMAKE_BINARY_DIR}/src/xhtml/*.cpp"
)

# Add the headers
include_directories(
    "${CMAKE_BINARY_DIR}/src"
    "${CMAKE_BINARY_DIR}/src/hex"
    "${CMAKE_BINARY_DIR}/src/imgui"
    "${CMAKE_BINARY_DIR}/src/imgui_additions"
    "${CMAKE_BINARY_DIR}/src/kre"
    "${CMAKE_BINARY_DIR}/src/svg"
    "${CMAKE_BINARY_DIR}/src/tiled"
    "${CMAKE_BINARY_DIR}/src/treetree"
    "${CMAKE_BINARY_DIR}/src/xhtml"
    "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/include"
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
add_compile_definitions(IMGUI_USER_CONFIG="${CMAKE_BINARY_DIR}/src/imgui_additions/imconfig_anura.h")

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

# END Linker config
