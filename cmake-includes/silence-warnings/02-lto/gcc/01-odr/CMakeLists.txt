# In an optimal state, this file does not exist
# XXX - We have to set this globally as otherwise it does not reach the linker!

# XXX - C++ 2003 One Definition Rule is broken at link time and trips up on LTO
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-odr")
