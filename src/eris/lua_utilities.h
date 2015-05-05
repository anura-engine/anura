#pragma once

///////
// Header to allow anura to find the `main` functions
// of the lua standalone tools.
///////
#ifdef __cplusplus
extern "C" {
#endif

int lua_standalone_interpreter_main( int argc, char * argv[]);
int lua_standalone_compiler_main( int argc, char * argv[]);

// The Eris persistence unit tests
int lua_test_persist_main( int argc, char* argv[]);
int lua_test_unpersist_main( int argc, char* argv[]);
#ifdef __cplusplus
}
#endif
