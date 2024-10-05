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

# Use vcpkg for dependencies
set(VCPKG_MANIFEST_DIR "${CMAKE_BINARY_DIR}")
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_BINARY_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")

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

# Configure compiling against vcpkg provided libraries
# XXX - At the moment (2023-08) we have no idea of the upper and lower bounds
# XXX - At the moment (2023-08) we have no idea of the full minimal set
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem locale regex system)
find_package(ZLIB REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(Freetype REQUIRED)
find_package(GLM REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_mixer REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(Ogg REQUIRED)
find_package(Vorbis REQUIRED)
find_package(harfbuzz CONFIG REQUIRED)
find_package(WebP CONFIG REQUIRED)
find_package(unofficial-lerc CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)

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
    "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include"
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
    set(VCPKG_TARGET_TRIPLET x64-linux-dbg)
    set(VCPKG_BUILD_TYPE debug)
    link_directories("${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/debug/lib")
endif()

if (CMAKE_BUILD_TYPE MATCHES Release)
    # Optimize release builds
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
    set(VCPKG_TARGET_TRIPLET x64-linux-rel)
    set(VCPKG_BUILD_TYPE release)
    link_directories("${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/release/lib")
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

# XXX - The md5 module of anura does not build without this.
# XXX - Other files also emit the warning, but somehow this gets elevated to an error - no clue, probably worse.
set_property(
    SOURCE "${CMAKE_BINARY_DIR}/src/md5.cpp"
    APPEND_STRING
    PROPERTY COMPILE_FLAGS " -Wno-narrowing"
)

# END Compiler config

# BEGIN Linker config

# Staticly link dependencies in
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

# Output an executable from the anura sources for anura
add_executable(anura "${anura_SRC}")

# Link libraries
# XXX - The order of the static linking matters!
# XXX - The grouping of the static linking matters!
# XXX - DO NOT BUMP VCPKG BASELINE - ANURA CODE CHANGES REQUIRED FIRST!
# XXX - Some stuff is simply not staticly linkable:
# - Audio stuff (samplerate, ALSA, Pulseaudio)
# - Graphics stuff (OpenGL, EGL, DRM, GBM)
# - X11
# - Wayland
# - libc (with: realtme, dlopen)
#
# We must YOLO and trust the platform has these things
target_link_libraries(
    anura
    PRIVATE
    -Wl,-Bstatic

    -Wl,--start-group
    SDL2
    -Wl,--end-group

    -Wl,--start-group
    Boost::filesystem
    -Wl,--end-group

    -Wl,--start-group
    cairo
    pixman-1
    freetype
    ZLIB::ZLIB
    fontconfig
    png
    expat
    brotlidec
    brotlicommon
    uuid
    -Wl,--end-group

    -Wl,--start-group
    GLEW::GLEW
    -Wl,--end-group

    -Wl,--start-group
    vorbisfile
    vorbis
    ogg
    -Wl,--end-group

    -Wl,--start-group
    SDL2_ttf
    harfbuzz::harfbuzz
    -Wl,--end-group

    -Wl,--start-group
    Boost::locale
    -Wl,--end-group

    -Wl,--start-group
    SDL2_image
    WebP::webp
    WebP::webpdemux
    WebP::webpdecoder
    tiff
    lzma
    deflate
    WebP::webp
    jpeg
    unofficial::Lerc::Lerc
    zstd::libzstd_static
    -Wl,--end-group

    -Wl,-Bdynamic
    -lrt
    -ldl
    -ldrm
    -lgbm
    -lX11
    -lXcursor
    -lXext
    -lXfixes
    -lXi
    -lXrandr
    -lXss
    -lxkbcommon
    -ldecor-0
    -lwayland-client
    -lwayland-cursor
    -lwayland-egl
    -lsamplerate
    -lasound
    -lpulse
)

# END Linker config
