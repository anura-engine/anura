if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    include("${CMAKE_CURRENT_LIST_DIR}/gcc/CMakeLists.txt")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Currently nothing pops up during LTO with Clang
endif()
