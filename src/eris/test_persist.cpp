#include <stdio.h>
#include <stdlib.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

static int LUAF_createludata(lua_State *L)
{
	lua_pushlightuserdata(L, (void*)321);
	return 1;
}

/* A userdata that may be literally persisted */
static int LUAF_boxinteger(lua_State *L)
{
					/* num */
	int* ptr = static_cast<int*>(lua_newuserdata(L, sizeof(int)));
					/* num udata */
	*ptr = luaL_checkint(L, 1);
	lua_newtable(L);
					/* num udata mt */
	lua_pushstring(L, "__persist");
					/* num udata mt "__persist" */
	lua_pushboolean(L, 1);
					/* num udata mt "__persist" true */
	lua_rawset(L, 3);
					/* num udata mt */
	lua_setmetatable(L, 2);
					/* num udata */
	return 1;
}

static int LUAF_boxboolean(lua_State *L)
{
					/* bool */
	char* ptr = static_cast<char *>(lua_newuserdata(L, sizeof(char)));
					/* bool udata */
	*ptr = (char)lua_toboolean(L, 1);
	lua_newtable(L);
					/* num udata mt */
	lua_pushstring(L, "__persist");
					/* num udata mt "__persist" */
	lua_getglobal(L, "booleanpersist");
					/* num udata mt "__persist" booleanpersist */
	lua_rawset(L, 3);
					/* num udata mt */
	lua_setmetatable(L, 2);
					/* num udata */
	return 1;
}

static int LUAF_unboxboolean(lua_State *L)
{
					/* udata */
	lua_pushboolean(L, *(char*)lua_touserdata(L, 1));
					/* udata bool */
	return 1;
}

static int LUAF_onerror(lua_State *L)
{
	
	const char* str = 0;
	if(lua_gettop(L) != 0)
	{
		str = lua_tostring(L, -1);
		printf("%s\n",str);
	}
	return 0;
}

#ifdef __cplusplus
extern "C" {
#endif
int lua_test_persist_main(int argc, char** argv)
{
	if (argc < 2) {
		printf("Usage: persist <script> <filename>\n");
		return 1;
	}

	lua_State* L = luaL_newstate();

	luaL_openlibs(L);
	lua_settop(L, 0);

	lua_register(L, "createludata", LUAF_createludata);
	lua_register(L, "boxinteger", LUAF_boxinteger);
	lua_register(L, "boxboolean", LUAF_boxboolean);
	lua_register(L, "unboxboolean", LUAF_unboxboolean);
	lua_register(L, "onerror", LUAF_onerror);

	lua_pushcfunction(L, LUAF_onerror);
	luaL_loadfile(L, argv[1]);
	lua_pushstring(L, argv[2]);
	lua_pcall(L,1,0,1);

	lua_close(L);

	return 0;
}
#ifdef __cplusplus
}
#endif
