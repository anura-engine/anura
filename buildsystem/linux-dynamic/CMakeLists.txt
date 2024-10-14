# BEGIN CMake setup

# 3.12 added add_compile_definitions
cmake_minimum_required(VERSION 3.12)

if(DEFINED ENV{CXX})
  message("")
  message("We do not support injecting a compiler in via ENV.")
  message("")
  message(
    FATAL_ERROR
    "CXX is set on the ENV. Please only use the the flag '-D CMAKE_CXX_COMPILER='."
  )
endif()

# Default to debug builds
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Default to using LTO for Release builds
if (CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Include repo local modules
set(
    CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    "${CMAKE_BINARY_DIR}/buildsystem/cmake-includes/modules"
)

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

# Configure compiling against system provided libraries
# XXX - At the moment (2023-08) we have no idea of the upper and lower bounds
# XXX - At the moment (2023-08) we have no idea of the full minimal set

# https://cmake.org/cmake/help/v3.12/module/FindThreads.html
find_package(Threads REQUIRED)
# https://cmake.org/cmake/help/v3.12/module/FindBoost.html
find_package(Boost REQUIRED COMPONENTS filesystem locale regex system)
# https://cmake.org/cmake/help/v3.12/module/FindZLIB.html
find_package(ZLIB REQUIRED)
# https://cmake.org/cmake/help/v3.12/module/FindOpenGL.html
find_package(OpenGL REQUIRED)
# https://cmake.org/cmake/help/v3.12/module/FindGLEW.html
find_package(GLEW REQUIRED)
# https://cmake.org/cmake/help/v3.12/module/FindFreetype.html
find_package(Freetype REQUIRED)

# https://github.com/assimp/assimp/blob/60989a598e2eec923612597b1516604b816d404a/cmake-modules/FindRT.cmake
find_package(RT REQUIRED)
# https://chromium.googlesource.com/external/github.com/g-truc/glm/+/refs/heads/0.9.6/util/FindGLM.cmake
find_package(GLM REQUIRED)
# https://github.com/tcbrindle/sdl2-cmake-scripts/blob/e037fb54f32973343fada6a051d3a3f8adf3a4a0/FindSDL2.cmake
find_package(SDL2 REQUIRED)
# https://github.com/tcbrindle/sdl2-cmake-scripts/blob/e037fb54f32973343fada6a051d3a3f8adf3a4a0/FindSDL2_image.cmake
find_package(SDL2_image REQUIRED)
# https://github.com/tcbrindle/sdl2-cmake-scripts/blob/e037fb54f32973343fada6a051d3a3f8adf3a4a0/FindSDL2_mixer.cmake
find_package(SDL2_mixer REQUIRED)
# https://github.com/tcbrindle/sdl2-cmake-scripts/blob/e037fb54f32973343fada6a051d3a3f8adf3a4a0/FindSDL2_ttf.cmake
find_package(SDL2_ttf REQUIRED)
# https://github.com/libsndfile/libsndfile/blob/ec104e3631a06c4a16f5cbae93b4af2c554fab20/cmake/FindOgg.cmake
find_package(Ogg REQUIRED)
# https://github.com/libsndfile/libsndfile/blob/ec104e3631a06c4a16f5cbae93b4af2c554fab20/cmake/FindVorbis.cmake
find_package(Vorbis REQUIRED)
# https://github.com/WebKit/WebKit/blob/acece69bd261c37f0a66d82d4abc80bed8b09bd7/Source/cmake/FindCairo.cmake
find_package(Cairo REQUIRED)

# Add the headers
# XXX - Unknown as of 2023-08 why only Freetype ft2build and SLD2 are needed
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
    "${FREETYPE_INCLUDE_DIR_ft2build}"
    "${SDL2_INCLUDE_DIR}"
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
    # Debug builds need to be debuggable
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Og")
endif()

if (CMAKE_BUILD_TYPE MATCHES Release)
    # Optimize release builds
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

# Warnings as errors
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")

# Enable all compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# Enable extra compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")

# Enable pedantic compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic")

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

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Older compilers should not trip up when we selectively silence warnings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-warning-option")
endif()

# We need to hide a ton of warnings
include("${CMAKE_BINARY_DIR}/buildsystem/cmake-includes/silence-warnings/CMakeLists.txt")

# END Compiler config

# BEGIN Linker config

# Output an executable from the anura sources for anura
add_executable(anura "${anura_SRC}")

# Link libraries
target_link_libraries(
    anura
    PRIVATE
    "${RT_LIBRARIES}"
    "${CMAKE_THREAD_LIBS_INIT}"
    "${Boost_LIBRARIES}"
    "${ZLIB_LIBRARIES}"
    "${OPENGL_LIBRARIES}"
    "${GLEW_LIBRARIES}"
    "${FREETYPE_LIBRARIES}"
    "${SDL2_LIBRARY}"
    "${SDL2_IMAGE_LIBRARIES}"
    "${SDL2_MIXER_LIBRARIES}"
    "${SDL2_TTF_LIBRARIES}"
    "${Vorbis_File_LIBRARY}"
    "${Cairo_LIBRARIES}"
)

# END Linker config
