#include <stdio.h>
#include <stdlib.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

static int LUAF_checkludata(lua_State *L)
{
	lua_pushboolean(L, lua_touserdata(L, -1) == (void*)321);
	return 1;
}

static int LUAF_unboxinteger(lua_State *L)
{
	lua_pushnumber(L, *((int*)lua_touserdata(L, -1)));
	return 1;
}

static int LUAF_unboxboolean(lua_State *L)
{
					/* udata */
	lua_pushboolean(L, *(char*)lua_touserdata(L, 1));
					/* udata bool */
	return 1;
}

static int LUAF_boxboolean(lua_State *L)
{
					/* bool */
	char* ptr = static_cast<char*>(lua_newuserdata(L, sizeof(char)));
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
int lua_test_unpersist_main(int argc, char** argv)
{
	if (argc < 2) {
		printf("Usage: unpersist <script> <filename>\n");
		return 1;
	}
	lua_State* L = luaL_newstate();

	luaL_openlibs(L);
	lua_settop(L, 0);

	lua_register(L, "checkludata", LUAF_checkludata);
	lua_register(L, "unboxinteger", LUAF_unboxinteger);
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
