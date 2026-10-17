# FindGLM
# -------
#
# Locate a copy of OpenGL Mathematics (glm) which is already present on the
# machine, so that a build does not have to download it through FetchContent.
# This is meant for build hosts without network access.
#
# glm is a header only library, so all we need is an include directory which
# provides glm/glm.hpp. If a CMake package (glm::glm) or pkg-config module is
# available it is preferred, as that also gives us the proper include and
# compile definitions.
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
#   GLM_FOUND       - TRUE if glm was found
#   GLM_VERSION     - glm version, if it could be determined
#   GLM_INCLUDE_DIR - include directory which contains glm/glm.hpp
#   GLM_LIBRARIES   - libraries or imported target to link against, may be empty
#   GLM_TARGET      - imported target providing include directories and linkage
#
# Hints
# ^^^^^
#
#   GLM_ROOT - directory of an unpacked glm source tree or an install prefix

include(FindPackageHandleStandardArgs)

# Already in cache, be silent
if(GLM_INCLUDE_DIR)
    set(GLM_FIND_QUIETLY TRUE)
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_GLM QUIET glm)
endif()

# Prefer a "proper" CMake package over a plain include directory
if(NOT TARGET glm::glm)
    find_package(glm CONFIG QUIET)
endif()

find_path(
    GLM_INCLUDE_DIR
    NAMES glm/glm.hpp
    HINTS
        ${PC_GLM_INCLUDEDIR}
        ${PC_GLM_INCLUDE_DIRS}
        ${GLM_ROOT}
)

# glm is header only, but some packagers ship a (usually empty) library
find_library(
    GLM_LIBRARY
    NAMES glm libglm
    HINTS
        ${PC_GLM_LIBDIR}
        ${PC_GLM_LIBRARY_DIRS}
        ${GLM_ROOT}
)

if(EXISTS "${GLM_INCLUDE_DIR}/glm/detail/setup.hpp")
    set(_GLM_VERSION_PARTS "")
    foreach(
        _GLM_PART
        "MAJOR"
        "MINOR"
        "PATCH"
    )
        file(
            STRINGS "${GLM_INCLUDE_DIR}/glm/detail/setup.hpp"
            _GLM_VERSION_LINE
            REGEX "^#define[ \t]+GLM_VERSION_${_GLM_PART}[ \t]+[0-9]+"
        )
        string(REGEX REPLACE "^#define[ \t]+GLM_VERSION_[A-Z]+[ \t]+([0-9]+).*$" "\\1" _GLM_VERSION_LINE "${_GLM_VERSION_LINE}")
        list(APPEND _GLM_VERSION_PARTS "${_GLM_VERSION_LINE}")
    endforeach()
    string(REPLACE ";" "." GLM_VERSION "${_GLM_VERSION_PARTS}")
endif()

if(TARGET glm::glm)
    set(GLM_IMPLEMENTATION "package")
elseif(GLM_INCLUDE_DIR)
    set(GLM_IMPLEMENTATION "headers")
endif()

string(
    CONCAT
    _GLM_REASON
    "no glm headers, CMake package or pkg-config module could be found. Install the glm package "
    "of your distribution, install a 'glm::glm' style CMake package, or point "
    "'-D GLM_ROOT=<dir>' at an unpacked glm source tree containing glm/glm.hpp."
)

find_package_handle_standard_args(
    GLM
    REQUIRED_VARS
        GLM_IMPLEMENTATION
    VERSION_VAR GLM_VERSION
    REASON_FAILURE_MESSAGE "${_GLM_REASON}"
)

if(GLM_FOUND)
    if(TARGET glm::glm)
        set(GLM_TARGET glm::glm)
        set(GLM_LIBRARIES glm::glm)
    elseif(GLM_LIBRARY)
        if(NOT TARGET GLM::GLM)
            add_library(GLM::GLM UNKNOWN IMPORTED)
            set_target_properties(
                GLM::GLM
                PROPERTIES
                    IMPORTED_LOCATION "${GLM_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${GLM_INCLUDE_DIR}"
            )
        endif()
        set(GLM_TARGET GLM::GLM)
        set(GLM_LIBRARIES GLM::GLM)
    else()
        if(NOT TARGET GLM::GLM)
            add_library(GLM::GLM INTERFACE IMPORTED)
            set_target_properties(
                GLM::GLM
                PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${GLM_INCLUDE_DIR}"
            )
        endif()
        set(GLM_TARGET GLM::GLM)
        set(GLM_LIBRARIES GLM::GLM)
    endif()

    set(GLM_INCLUDE_DIRS "${GLM_INCLUDE_DIR}")
endif()

mark_as_advanced(GLM_INCLUDE_DIR GLM_LIBRARY)
