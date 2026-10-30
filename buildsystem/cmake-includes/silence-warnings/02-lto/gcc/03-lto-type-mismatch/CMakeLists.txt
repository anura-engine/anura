# In an optimal state, this file does not exist
# XXX - We have to set this globally as otherwise it does not reach the linker!

# XXX - Some of the type mismatches only pop up during LTO
# XXX - Needs to be globally exluded from errors due to how exactly it only bubbles up during LTO
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=stringop-overflow")
