# FindImGui
# ---------
#
# Locate a copy of Dear ImGui which is already present on the machine, so that
# a build does not have to download it through FetchContent. This is meant for
# build hosts without network access.
#
# The following providers are searched, in order of preference:
#
#   1. An imgui source tree which was requested explicitly through
#      ANURA_IMGUI_DIR or ImGui_ROOT. Such sources have to be compiled together
#      with the headers they ship with, which makes this the most reliable
#      provider.
#   2. A CMake package, i.e. an `imgui::imgui` target
#   3. A prebuilt `imgui` library plus the matching headers
#   4. Upstream sources which happen to sit next to the installed headers
#
# NOTE: Anura injects its own configuration into imgui through the
# NOTE: `IMGUI_USER_CONFIG` compile definition, which an already built imgui
# NOTE: library knows nothing about. Providers 3 and 4 therefore only work when
# NOTE: that library was built with an ABI compatible configuration, otherwise
# NOTE: the result is undefined behaviour at runtime.
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
#   ImGui_FOUND       - TRUE if imgui was found
#   ImGui_VERSION     - imgui version from imgui.h, if it could be determined
#   IMGUI_FOUND       - same as ImGui_FOUND, for consumers which want it upper case
#   IMGUI_VERSION     - imgui version string, if it could be determined
#   IMGUI_VERSION_NUM - numeric imgui version (IMGUI_VERSION_NUM), if it could be determined
#   IMGUI_INCLUDE_DIR - include directory containing imgui.h
#   IMGUI_LIBRARIES   - libraries or imported target to link against, may be empty
#   IMGUI_SOURCES     - sources which have to be compiled by the consumer, may be empty
#   IMGUI_TARGET      - imported target providing include directories and linkage
#
# Hints
# ^^^^^
#
#   ANURA_IMGUI_DIR - directory of an unpacked imgui source tree
#   ImGui_ROOT      - same, for `find_package(ImGui)` style usage

include(FindPackageHandleStandardArgs)

# The upstream core sources, imgui has no build system of its own
set(
    ImGui_CORE_SOURCES
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
)

# Helper: look for the complete set of upstream sources in a directory. On
# success it sets ImGui_SOURCE_DIR and ImGui_SOURCES in the caller's scope.
function(ImGui_search_sources HINT)
    if(NOT IS_DIRECTORY "${HINT}")
        return()
    endif()
    set(_SOURCES "")
    foreach(_SOURCE IN LISTS ImGui_CORE_SOURCES)
        if(NOT EXISTS "${HINT}/${_SOURCE}")
            return()
        endif()
        list(APPEND _SOURCES "${HINT}/${_SOURCE}")
    endforeach()
    set(ImGui_SOURCE_DIR "${HINT}" PARENT_SCOPE)
    set(ImGui_SOURCES "${_SOURCES}" PARENT_SCOPE)
endfunction()

# Provider 1: an explicitly requested source tree, which also means that we do
# not have to look for anything else
set(ImGui_SOURCE_DIR "")
set(ImGui_SOURCES "")
foreach(_ImGui_HINT IN ITEMS "${ANURA_IMGUI_DIR}" "${ImGui_ROOT}")
    if(_ImGui_HINT)
        ImGui_search_sources("${_ImGui_HINT}")
        if(ImGui_SOURCES)
            break()
        endif()
    endif()
endforeach()

if(ANURA_IMGUI_DIR AND NOT ImGui_SOURCES AND EXISTS "${ANURA_IMGUI_DIR}/imgui.h")
    message(
        WARNING
        "ANURA_IMGUI_DIR ('${ANURA_IMGUI_DIR}') provides imgui.h but not the "
        "complete upstream source set (${ImGui_CORE_SOURCES}), imgui will be "
        "searched for somewhere else instead."
    )
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_ImGui QUIET imgui)
endif()

# Provider 2: a "proper" CMake package, like the one vcpkg ships
if(NOT ImGui_SOURCES AND NOT TARGET imgui::imgui)
    find_package(imgui CONFIG QUIET)
endif()

# Provider 3: a prebuilt library, plus its headers
if(NOT ImGui_SOURCES)
    find_library(
        ImGui_LIBRARY
        NAMES imgui libimgui imgui_static libimgui_static
        HINTS
            ${PC_ImGui_LIBDIR}
            ${PC_ImGui_LIBRARY_DIRS}
            ${ANURA_IMGUI_DIR}
            ${ImGui_ROOT}
    )
endif()

if(ImGui_SOURCE_DIR)
    # The sources have to be compiled with the headers they ship with
    set(ImGui_INCLUDE_DIR "${ImGui_SOURCE_DIR}")
else()
    find_path(
        ImGui_INCLUDE_DIR
        NAMES imgui.h
        HINTS
            ${PC_ImGui_INCLUDEDIR}
            ${PC_ImGui_INCLUDE_DIRS}
            ${ANURA_IMGUI_DIR}
            ${ImGui_ROOT}
        PATH_SUFFIXES imgui
    )
endif()

# Provider 4: upstream sources which happen to sit next to the installed headers
if(NOT ImGui_SOURCES AND NOT TARGET imgui::imgui AND NOT ImGui_LIBRARY)
    foreach(
        _ImGui_HINT
        IN ITEMS
            "${ImGui_INCLUDE_DIR}"
            "${ImGui_INCLUDE_DIR}/.."
            "${PC_ImGui_INCLUDEDIR}/imgui"
            "${PC_ImGui_INCLUDE_DIRS}/imgui"
    )
        if(_ImGui_HINT)
            ImGui_search_sources("${_ImGui_HINT}")
            if(ImGui_SOURCES)
                break()
            endif()
        endif()
    endforeach()
endif()

if(EXISTS "${ImGui_INCLUDE_DIR}/imgui.h")
    file(
        STRINGS "${ImGui_INCLUDE_DIR}/imgui.h"
        _ImGui_VERSION_LINE
        REGEX "^#define[ \t]+IMGUI_VERSION[ \t]+\""
    )
    string(
        REGEX REPLACE
        "^#define[ \t]+IMGUI_VERSION[ \t]+\"([^\"]+)\".*$"
        "\\1"
        ImGui_VERSION
        "${_ImGui_VERSION_LINE}"
    )

    file(
        STRINGS "${ImGui_INCLUDE_DIR}/imgui.h"
        _ImGui_VERSION_NUM_LINE
        REGEX "^#define[ \t]+IMGUI_VERSION_NUM[ \t]+[0-9]+"
    )
    string(
        REGEX REPLACE
        "^#define[ \t]+IMGUI_VERSION_NUM[ \t]+([0-9]+).*$"
        "\\1"
        ImGui_VERSION_NUM
        "${_ImGui_VERSION_NUM_LINE}"
    )
endif()

if(ImGui_SOURCES)
    set(ImGui_IMPLEMENTATION "sources")
elseif(TARGET imgui::imgui)
    set(ImGui_IMPLEMENTATION "package")
elseif(ImGui_LIBRARY)
    set(ImGui_IMPLEMENTATION "library")
endif()

string(
    CONCAT
    _ImGui_REASON
    "no imgui library, CMake package, pkg-config module or upstream sources could be found. "
    "Install the imgui package of your distribution, install an 'imgui::imgui' style CMake package, "
    "or point '-D ANURA_IMGUI_DIR=<dir>' at an unpacked imgui source tree containing imgui.h and "
    "the upstream imgui*.cpp files. Note that some distributions only ship the imgui headers, "
    "in which case the upstream sources are required as well."
)

find_package_handle_standard_args(
    ImGui
    REQUIRED_VARS
        ImGui_IMPLEMENTATION
    VERSION_VAR ImGui_VERSION
    REASON_FAILURE_MESSAGE "${_ImGui_REASON}"
)

if(ImGui_FOUND)
    if(ImGui_IMPLEMENTATION STREQUAL "sources")
        if(NOT TARGET ImGui::ImGui)
            add_library(ImGui::ImGui INTERFACE IMPORTED)
            set_target_properties(
                ImGui::ImGui
                PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${ImGui_INCLUDE_DIR}"
            )
        endif()
        set(IMGUI_TARGET ImGui::ImGui)
        set(IMGUI_LIBRARIES ImGui::ImGui)
        set(IMGUI_SOURCES "${ImGui_SOURCES}")
    elseif(ImGui_IMPLEMENTATION STREQUAL "package")
        set(IMGUI_TARGET imgui::imgui)
        set(IMGUI_LIBRARIES imgui::imgui)
        set(IMGUI_SOURCES "")
    else()
        if(NOT TARGET ImGui::ImGui)
            add_library(ImGui::ImGui UNKNOWN IMPORTED)
            set_target_properties(
                ImGui::ImGui
                PROPERTIES
                    IMPORTED_LOCATION "${ImGui_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${ImGui_INCLUDE_DIR}"
            )
        endif()
        set(IMGUI_TARGET ImGui::ImGui)
        set(IMGUI_LIBRARIES ImGui::ImGui)
        set(IMGUI_SOURCES "")
    endif()

    set(IMGUI_INCLUDE_DIR "${ImGui_INCLUDE_DIR}")
    set(IMGUI_VERSION "${ImGui_VERSION}")
    set(IMGUI_VERSION_NUM "${ImGui_VERSION_NUM}")
    set(IMGUI_IMPLEMENTATION "${ImGui_IMPLEMENTATION}")
    set(IMGUI_FOUND TRUE)
endif()

mark_as_advanced(ImGui_INCLUDE_DIR ImGui_LIBRARY)
