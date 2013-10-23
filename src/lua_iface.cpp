#if defined(USE_LUA)

#include <cmath>
#include <iostream>
#include <functional>
#include <boost/bind.hpp>
#ifdef _MSC_VER
#include <boost/math/special_functions/round.hpp>
#endif

#include "custom_object.hpp"
#include "custom_object_functions.hpp"
#include "filesystem.hpp"
#include "formula_function_registry.hpp"
#include "lua_iface.hpp"
#include "level.hpp"
#include "module.hpp"
#include "playable_custom_object.hpp"
#include "unit_test.hpp"

namespace lua
{
	lua_context& get_global_lua_instance()
	{
		static lua_context res;
		return res;
	}

	lua_context& lua_context::get_instance()
	{
		return get_global_lua_instance();
	}

	bool lua_context::dostring(const char* str, game_logic::formula_callable* callable)
	{
		if(callable) {
			using namespace game_logic;
			lua_getglobal(context_ptr(), "Anura");			// (-0,+1,e)

			formula_callable** a = static_cast<formula_callable**>(lua_newuserdata(context_ptr(), sizeof(formula_callable*))); //(-0,+1,e)
			*a = callable;
			intrusive_ptr_add_ref(*a);

			luaL_getmetatable(context_ptr(), "Callable");	// (-0,+1,e)
			lua_setmetatable(context_ptr(), -2);			// (-1,+0,e)

			lua_setfield(context_ptr(), -2, "me");			// (-1,+0,e)
			lua_pop(context_ptr(),1);						// (-n(1),+0,-)
		}

		if (luaL_loadbuffer(context_ptr(), str, std::strlen(str), str) || lua_pcall(context_ptr(), 0, 0, 0))
		{
			const char* a = lua_tostring(context_ptr(), -1);
			std::cout << a << "\n";
			lua_pop(context_ptr(), 1);
			return true;
		}
		return false;
	}

	bool lua_context::dofile(const char* str, game_logic::formula_callable* callable)
	{
		std::string file_contents = sys::read_file(module::map_file(str));
		return dostring(file_contents.c_str(), callable);
	}

	namespace
	{
		static int variant_to_lua_value(lua_State* L, const variant& value)
		{
			switch(value.type()) {
				case variant::VARIANT_TYPE_NULL:
					lua_pushnil(L);
					break;
				case variant::VARIANT_TYPE_BOOL:
					lua_pushboolean(L, value.as_bool());
					break;
				case variant::VARIANT_TYPE_INT:
					lua_pushinteger(L, value.as_int());
					break;
				case variant::VARIANT_TYPE_DECIMAL:
					lua_pushnumber(L, value.as_decimal().as_float());
					break;
				case variant::VARIANT_TYPE_LIST: {
					lua_newtable(L);								// (-0,+1,-)
					for(int n = 0; n != value.num_elements(); ++n) {
						lua_pushnumber(L, n);						// (-0,+1,-)
						variant_to_lua_value(L, value[n]);			// (-0,+1,e)
						lua_settable(L,-3);							// (-2,+0,e)
					}
					break;
				}
				case variant::VARIANT_TYPE_STRING: {
					const std::string& s = value.as_string();
					lua_pushlstring(L, s.c_str(), s.size());
					break;
				}
				case variant::VARIANT_TYPE_MAP: {
					lua_newtable(L);								// (-0,+1,-)
					auto m = value.as_map();
					for(auto it : m) {
						variant_to_lua_value(L, it.first);			// (-0,+1,e)
						variant_to_lua_value(L, it.second);			// (-0,+1,e)
						lua_settable(L,-3);							// (-2,+0,e)
					}
					break;
				}
				case variant::VARIANT_TYPE_CALLABLE: {
					using namespace game_logic;
					formula_callable** a = static_cast<formula_callable**>(lua_newuserdata(L, sizeof(formula_callable*))); //(-0,+1,e)
					*a = const_cast<formula_callable*>(value.as_callable());
					intrusive_ptr_add_ref(*a);

					luaL_getmetatable(L, "Callable");	// (-0,+1,e)
					lua_setmetatable(L, -2);			// (-1,+0,e)
					return 1;
				}
				case variant::VARIANT_TYPE_CALLABLE_LOADING:
				case variant::VARIANT_TYPE_FUNCTION:
				case variant::VARIANT_TYPE_MULTI_FUNCTION:
				case variant::VARIANT_TYPE_DELAYED:
				case variant::VARIANT_TYPE_INVALID:
				default:
					luaL_error(L, "%s:%d", "Unrecognised variant type", int(value.type()));
			}
			return 1;
		}

		struct ffl_function_userdata
		{
			char* name;
		};

		static variant lua_value_to_variant(lua_State* L, int ndx)
		{
#ifdef _MSC_VER
			using namespace boost::math;
#endif
			int t = lua_type(L, ndx);
			switch(t) {
				case LUA_TSTRING:
					return variant(lua_tostring(L, ndx));
				case LUA_TBOOLEAN:
					return variant::from_bool(lua_toboolean(L, ndx) != 0);
				case LUA_TNUMBER: {
					double d = lua_tonumber(L, ndx);
					if(abs(d - round(d)) < 1e-14 && d < double(std::numeric_limits<int>::max())) {
						return variant(int(d));
					} else {
						return variant(d);
					}
					break;
				}
				case LUA_TTABLE:
				case LUA_TFUNCTION:
				case LUA_TUSERDATA:
				case LUA_TTHREAD:
				case LUA_TLIGHTUSERDATA:
				default:
					luaL_error(L, "Unsupported type to convert on stack: %d", t);
			}
			return variant();
		}

		static int call_function(lua_State* L)
		{
			using namespace game_logic;
			ffl_function_userdata* fn = static_cast<ffl_function_userdata*>(luaL_checkudata(L,1,"FFL_Function")); // (-0,+0,-)
			auto& symbols = get_formula_functions_symbol_table();
			std::vector<expression_ptr> args;
			int nargs = lua_gettop(L);
			for(int n = 2; n <= nargs; ++n) {
				args.push_back(expression_ptr(new variant_expression(lua_value_to_variant(L, n))));
			}
			auto& player = level::current().player()->get_entity();
			auto value = symbols.create_function(fn->name, args, player.get_definition());
			if(value == NULL) {
				luaL_error(L, "Function not found: %s", fn->name);
			}
			auto ret = value->evaluate(player);
			if(ret.is_callable()) {
				player.execute_command(ret);
			} else {
				return variant_to_lua_value(L, ret);
			}
			return 0;
		}

		static int gc_function(lua_State* L)
		{
			ffl_function_userdata* fn = static_cast<ffl_function_userdata*>(luaL_checkudata(L,1,"FFL_Function")); // (-0,+0,-)
			delete[] fn->name;
			fn->name = NULL;
			return 0;
		}

		static int remove_callable_reference(lua_State* L)
		{
			using namespace game_logic;
			formula_callable* callable = *static_cast<formula_callable**>(lua_touserdata(L, 1));	// (-0,+0,-)
			luaL_argcheck(L, callable != NULL, 1, "'callable' was null");
			intrusive_ptr_release(callable);
			return 0;
		}

		static int get_callable_index(lua_State* L)
		{
			using namespace game_logic;
			formula_callable* callable = *static_cast<formula_callable**>(luaL_checkudata(L, 1, "Callable"));	// (-0,+0,-)
			const char *name = lua_tostring(L, 2);						// (-0,+0,e)
			variant value = callable->query_value(name);
			if(!value.is_null()) {
				return variant_to_lua_value(L, value);
			}
			ffl_function_userdata* fn = static_cast<ffl_function_userdata*>(lua_newuserdata(L, sizeof(ffl_function_userdata))); //(-0,+1,e)
			fn->name = new char[strlen(name)+1];			
			strcpy(fn->name, name);
			luaL_getmetatable(L, "FFL_Callable_Function");		// (-0,+1,e)
			lua_setmetatable(L, -2);					// (-1,+0,e)
			return 1;
		}

		static int set_callable_index(lua_State* L)
		{
			// stack -- table, key, value
			using namespace game_logic;
			formula_callable* callable = *static_cast<formula_callable**>(luaL_checkudata(L, 1, "Callable"));	// (-0,+0,-)
			const char *name = lua_tostring(L, 2);						// (-0,+0,e)
			variant value = lua_value_to_variant(L, 3);
			callable->mutate_value(name, value);
			return 0;
		}

		static int serialize_callable(lua_State* L) 
		{
			using namespace game_logic;
			std::string res;
			formula_callable* callable = *static_cast<formula_callable**>(luaL_checkudata(L, 1, "Callable"));	// (-0,+0,-)
			std::vector<formula_input> inputs = callable->inputs();
			for(auto inp : inputs) {
				std::stringstream ss;
				static const char* const access[] = {
					"ro",
					"wo",
					"rw",
				};
				if(inp.access != FORMULA_READ_ONLY 
					&& inp.access != FORMULA_WRITE_ONLY
					&& inp.access != FORMULA_READ_WRITE) {
					luaL_error(L, "Unrecognised access mode: ", inp.access);
				}
				ss << inp.name << "(" << access[inp.access] << ") : " << callable->query_value(inp.name) << std::endl;
				res += ss.str();
			}
			lua_pushstring(L, res.c_str());			// (-0,+1,-)
			return 1;
		}

		static int call_callable_function(lua_State* L)
		{
			using namespace game_logic;

			if(luaL_testudata(L, 1, "FFL_Function")) {
				return call_function(L);
			}

			ffl_function_userdata* fn = static_cast<ffl_function_userdata*>(luaL_checkudata(L,1,"FFL_Callable_Function")); // (-0,+0,-)
			std::vector<expression_ptr> args;
			int nargs = lua_gettop(L);
			for(int n = 3; n <= nargs; ++n) {
				args.push_back(expression_ptr(new variant_expression(lua_value_to_variant(L, n))));
			}
			formula_callable* callable = *static_cast<formula_callable**>(luaL_checkudata(L, 2, "Callable"));	// (-0,+0,-)

			auto& symbols = get_formula_functions_symbol_table();
			auto value = symbols.create_function(fn->name, args, NULL);
			if(value != NULL) {
				auto ret = value->evaluate(*callable);
				if(ret.is_callable()) {
					callable->execute_command(ret);
				} else {
					return variant_to_lua_value(L, ret);
				}
				return 0;
			}

			value = get_custom_object_functions_symbol_table().create_function(fn->name, args, NULL);
			if(value != NULL) {
				auto ret = value->evaluate(*callable);
				if(ret.is_callable()) {
					callable->execute_command(ret);
				} else {
					return variant_to_lua_value(L, ret);
				}
				return 0;
			}
			luaL_error(L, "Function not found: %s", fn->name);
			return 0;
		}

		static int gc_callable_function(lua_State* L)
		{
			ffl_function_userdata* fn = static_cast<ffl_function_userdata*>(luaL_checkudata(L,1,"FFL_Callable_Function")); // (-0,+0,-)
			delete[] fn->name;
			fn->name = NULL;
			return 0;
		}

		const luaL_Reg gFFLFunctions[] = {
			{"__call", call_function},
			{"__gc", gc_function},
		};

		const luaL_Reg gFFLCallableFunctions[] = {
			{"__call", call_callable_function},
			{"__gc", gc_callable_function},
		};

		const luaL_Reg gCallableFunctions[] = {
			{"__index", get_callable_index},
			{"__newindex", set_callable_index},
			{"__tostring", serialize_callable},
			{"__gc", remove_callable_reference},
			{NULL, NULL},
		};

		static int get_level(lua_State* L) 
		{
			level& lvl = level::current();

			level** a = static_cast<level**>(lua_newuserdata(L, sizeof(level*))); //(-0,+1,e)
			*a = &lvl;
			intrusive_ptr_add_ref(&lvl);

			luaL_getmetatable(L, "Callable");		// (-0,+1,e)
			lua_setmetatable(L, -2);			// (-1,+0,e)
			return 1;
		}

		int anura_table_index(lua_State* L) 
		{
			const char *name = lua_tostring(L, 2);						// (-0,+0,e)

			ffl_function_userdata* fn = static_cast<ffl_function_userdata*>(lua_newuserdata(L, sizeof(ffl_function_userdata))); //(-0,+1,e)
			fn->name = new char[strlen(name)+1];			
			strcpy(fn->name, name);
			luaL_getmetatable(L, "FFL_Function");		// (-0,+1,e)
			lua_setmetatable(L, -2);					// (-1,+0,e)
			return 1;
		}
		
		static const struct luaL_Reg anura_functions [] = {
			{"level", get_level},
			{NULL, NULL},
		};

		static void push_anura_table(lua_State* L) 
		{
			lua_getglobal(L, "Anura");					// (-0,+1,e)
			if (lua_isnil(L, -1)) {						// (-0,+0,e)
				lua_pop(L, 1);							// (-n(1),+0,e)
				lua_newtable(L);						// (-0,+1,e)

				luaL_newmetatable(L, "AnuraMeta");		// (-0,+1,e)
				lua_pushcfunction(L, anura_table_index);// (-0,+1,e)
				lua_setfield(L, -2, "__index");			// (-1,+0,e)
				lua_setmetatable(L, -2);				// (-1,+0,e)
			}
			luaL_setfuncs(L, anura_functions, 0);		// (-nup(0),+0,e)
			lua_setglobal(L, "Anura");					// (-1,+0,e)
		}
	}

	lua_context::lua_context()
	{
		// initialize Lua
		state_.reset(luaL_newstate(), [](lua_State* L) { lua_close(L); });

		// load various Lua libraries
		luaopen_base(context_ptr());
		luaopen_string(context_ptr());
		luaopen_table(context_ptr());
		luaopen_math(context_ptr());
		luaopen_io(context_ptr());
		luaopen_debug(context_ptr());
		luaL_openlibs(context_ptr());

		luaL_newmetatable(context_ptr(), "FFL_Function");
		luaL_setfuncs(context_ptr(), gFFLFunctions, 0);	

		luaL_newmetatable(context_ptr(), "Callable");
		luaL_setfuncs(context_ptr(), gCallableFunctions, 0);

		luaL_newmetatable(context_ptr(), "FFL_Callable_Function");
		luaL_setfuncs(context_ptr(), gFFLCallableFunctions, 0);

		push_anura_table(context_ptr());

		/*dostring(
			"local lvl = Anura.level()\n"
			"print(lvl.id, lvl.music_volume, lvl.in_editor)\n"
			"Anura.debug('abcd')\n"
			"Anura.eval('debug(map(range(10),value))')\n"
			"local camera = lvl.camera\n"
			"print('Camera speed: ' .. camera.speed)\n"
			"for n = 1, 10 do\n"
			"  camera.speed = n\n"
			"  print('Camera speed: ' .. camera.speed)\n"
			"end\n"
			"lvl.player:debug_rect(lvl.player.mid_x-50, lvl.player.mid_y-50, 100, 100)\n"
			//"for i,v in ipairs(lvl.active_chars) do\n"
			//"for i,v in ipairs(Anura.eval('range(10)')) do\n"
			"local mm = Anura.eval('{q(a):1,q(b):2}')\n"
			//"print(mm.a)\n"
			"for i,v in pairs(mm) do\n"
			"  print(i, v)\n"
			"end\n"
		);*/

		/*dostring("local me = Anura.me\n"
			"print(me.speed)",
			level::current().camera().get()
		);*/
	}

	lua_context::~lua_context()
	{
	}
}


#endif
