# XXX - The leading spaces in this file are meaningful for string concatenation!

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Some of the maybe unitialized warnings only pop up during LTO with -Wextra
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized")
    include("${CMAKE_CURRENT_LIST_DIR}/gcc/CMakeLists.txt")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    include("${CMAKE_CURRENT_LIST_DIR}/clang/CMakeLists.txt")
endif()
