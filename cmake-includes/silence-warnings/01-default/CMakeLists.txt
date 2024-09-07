if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    include("${CMAKE_CURRENT_LIST_DIR}/gcc/CMakeLists.txt")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    include("${CMAKE_CURRENT_LIST_DIR}/clang/CMakeLists.txt")
endif()
