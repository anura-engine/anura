# BEGIN CMake setup

# 3.12 added add_compile_definitions
# 3.14 added FetchContent
# 3.19 added presets v1: we use configurePresets
# XXX - We cannot take this further than 3.23 as that's what ships in the Steam Sniper SDK!
cmake_minimum_required(VERSION 3.19)

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

# Include repo local modules
set(
    CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    "${CMAKE_SOURCE_DIR}/../cmake-includes/modules"
)

# END CMake setup

# BEGIN Project config

# Build target for CXX project "anura"
project(anura LANGUAGES CXX)

# For running tests through CTest
enable_testing()

# Default to doing parallel LTO for Release builds, do thin LTO on Clang
if (CMAKE_BUILD_TYPE MATCHES Release)
    include(ProcessorCount)
    ProcessorCount(NPROC)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(LTO_COMPILE_FLAGS "-flto=${NPROC}")
        set(LTO_LINK_FLAGS    "-flto=${NPROC}")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|LLVM")
        # ThinLTO + parallel backend jobs for Clang
        set(LTO_COMPILE_FLAGS "-flto=thin")
        set(LTO_LINK_FLAGS    "-flto=thin" "-flto-jobs=${N}")
    endif()
endif()

# Fetch vendored dependencies
include(FetchContent)

# imgui is normally vendored by downloading it while configuring. Build hosts
# without network access can use a copy which is already present on the machine
# instead. See README.md ("Building without network access"). glm is never
# vendored here, it always comes from the machine.

# The imgui version which is vendored by default, used to warn about system
# provided imgui versions which the engine cannot be built against
set(anura_VENDORED_IMGUI_VERSION "1.89.7")

option(
    ANURA_USE_SYSTEM_IMGUI
    "Use an imgui which is present on the machine instead of downloading it"
    OFF
)
set(
    ANURA_IMGUI_DIR
    ""
    CACHE PATH
    "Directory of an unpacked imgui source tree, used when ANURA_USE_SYSTEM_IMGUI is enabled"
)

# Git is only needed to download the vendored dependencies
if(NOT ANURA_USE_SYSTEM_IMGUI)
    find_package(Git REQUIRED)
endif()

if(ANURA_USE_SYSTEM_IMGUI)
    find_package(ImGui REQUIRED)
    message(STATUS "Using system provided imgui ${IMGUI_VERSION} (${IMGUI_IMPLEMENTATION})")

    # imgui 1.92 removed the old ImGui::Combo()/ListBox() getter overloads and
    # reworked the font and texture handling, none of which anura handles yet
    if(IMGUI_VERSION_NUM AND IMGUI_VERSION_NUM GREATER_EQUAL 19200)
        message(
            WARNING
            "The system provided imgui is ${IMGUI_VERSION}, while anura is built "
            "against the ${anura_VENDORED_IMGUI_VERSION} which it normally vendors. "
            "imgui 1.92 removed API which the engine still uses, for example the old "
            "ImGui::Combo()/ListBox() getters, so expect compilation errors. Point "
            "'-D ANURA_IMGUI_DIR=<dir>' at an imgui ${anura_VENDORED_IMGUI_VERSION} "
            "source tree if the build fails."
        )
    endif()
else()
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG d4ddc46e7773e9a9b68f965d007968f35ca4e09a # https://github.com/ocornut/imgui/releases/tag/v1.89.7
        GIT_SHALLOW TRUE
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/build/src/imgui"
        SOURCE_SUBDIR purposefully-empty-to-skip-build
    )

    FetchContent_MakeAvailable(imgui)

    # imgui has no build system of its own, its sources are compiled into anura
    set(IMGUI_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/build/src/imgui")
    set(
        IMGUI_SOURCES
        "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui.cpp"
        "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_draw.cpp"
        "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_tables.cpp"
        "${CMAKE_SOURCE_DIR}/build/src/imgui/imgui_widgets.cpp"
    )
    set(IMGUI_LIBRARIES "")
endif()

# The vendored dependencies are compiled into the executable itself, system
# provided ones may have to be linked in explicitly
set(anura_VENDORED_LIBS "")
if(IMGUI_LIBRARIES)
    list(APPEND anura_VENDORED_LIBS "${IMGUI_LIBRARIES}")
endif()

# imgui is provided by an imported target when it was not vendored, such a
# target carries its own include directories
set(anura_VENDORED_INCLUDE_DIRS "")
if(IMGUI_INCLUDE_DIR)
    list(APPEND anura_VENDORED_INCLUDE_DIRS "${IMGUI_INCLUDE_DIR}")
endif()

# Use ccache to accelerate iterating on CXX files, if available
find_program(CCACHE "ccache")
if(CCACHE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE}")
endif(CCACHE)

# Add the source code
# XXX - IMGUI_SOURCES is empty when imgui is provided as a system library
file(
    GLOB
    anura_SRC
    ${IMGUI_SOURCES}
    "${CMAKE_SOURCE_DIR}/../../src/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/hex/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/imgui_additions/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/kre/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/svg/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/tiled/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/treetree/*.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/xhtml/*.cpp"
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
# https://github.com/anura-engine/anura/blob/trunk/buildsystem/cmake-includes/modules/FindGLM.cmake
find_package(GLM REQUIRED)

# https://github.com/shadow/shadow/blob/aa0445f42d4b5413b27b27d267d0a7f3ec29bed1/cmake/FindRT.cmake
find_package(RT REQUIRED)
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
    ${anura_VENDORED_INCLUDE_DIRS}
    "${CMAKE_SOURCE_DIR}/../../src"
    "${CMAKE_SOURCE_DIR}/../../src/hex"
    "${CMAKE_SOURCE_DIR}/../../src/imgui_additions"
    "${CMAKE_SOURCE_DIR}/../../src/kre"
    "${CMAKE_SOURCE_DIR}/../../src/svg"
    "${CMAKE_SOURCE_DIR}/../../src/tiled"
    "${CMAKE_SOURCE_DIR}/../../src/treetree"
    "${CMAKE_SOURCE_DIR}/../../src/xhtml"
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
add_compile_definitions(IMGUI_USER_CONFIG="${CMAKE_SOURCE_DIR}/../../src/imgui_additions/imconfig_anura.h")

if (CMAKE_BUILD_TYPE MATCHES Release)
    # Turn all things debug, like assertions, off for a release build
    add_compile_definitions(NDEBUG)
endif()

# Bad things, to get rid of

# Use imgui provided vector math
# XXX - imgui_custom.cpp relies on these
add_compile_definitions(IMGUI_DEFINE_MATH_OPERATORS)

# Quaternions have become experimental in GLM???
# XXX - Newer versions of GLM need this for some reason
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Older compilers should not trip up when we selectively silence warnings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-warning-option")
endif()

# We need to hide a ton of warnings
include("${CMAKE_SOURCE_DIR}/../cmake-includes/silence-warnings/CMakeLists.txt")

# END Compiler config

# BEGIN Linker config

# Output an executable from the anura sources for anura
add_executable(anura "${anura_SRC}")

# Set manual LTO flags as CMAKE_INTERPROCEDURAL_OPTIMIZATION does not parallelize
if (CMAKE_BUILD_TYPE MATCHES Release)
    target_compile_options(anura PRIVATE ${LTO_COMPILE_FLAGS})
    target_link_options(anura PRIVATE ${LTO_LINK_FLAGS})
endif()

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
    "${GLM_LIBRARIES}"
    "${FREETYPE_LIBRARIES}"
    "${SDL2_LIBRARY}"
    "${SDL2_IMAGE_LIBRARIES}"
    "${SDL2_MIXER_LIBRARIES}"
    "${SDL2_TTF_LIBRARIES}"
    "${Vorbis_File_LIBRARY}"
    "${Cairo_LIBRARIES}"
    ${anura_VENDORED_LIBS}
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
