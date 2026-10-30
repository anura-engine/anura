# In an optimal state, this file does not exist
# XXX - The leading spaces in this file are meaningful for string concatenation!

# XXX - Some of the stringpop overflows only pop up during LTO
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-stringpop-overflow")
