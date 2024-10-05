include("${CMAKE_CURRENT_LIST_DIR}/01-default/CMakeLists.txt")

# We only do LTO on Release builds
if (CMAKE_BUILD_TYPE MATCHES Release)
    include("${CMAKE_CURRENT_LIST_DIR}/02-lto/CMakeLists.txt")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/03-all/CMakeLists.txt")
include("${CMAKE_CURRENT_LIST_DIR}/04-extra/CMakeLists.txt")
include("${CMAKE_CURRENT_LIST_DIR}/05-pedantic/CMakeLists.txt")
