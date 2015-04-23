/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#if defined(USE_LUA)

#include <cmath>
#include <iostream>
#include <functional>
#ifdef _MSC_VER
#include <boost/math/special_functions/round.hpp>
#endif

#include "eris/lauxlib.h"
#include "eris/lualib.h"

#include "custom_object.hpp"
#include "custom_object_functions.hpp"
#include "filesystem.hpp"
#include "formula_function_registry.hpp"
#include "formula_object.hpp"
#include "lua_iface.hpp"
#include "level.hpp"
#include "module.hpp"
#include "playable_custom_object.hpp"
#include "unit_test.hpp"

namespace lua
{
	namespace 
	{
		const char* const anura_str = "Anura";
		const char* const anura_meta_str = "anura.meta";
		const char* const callable_function_str = "anura.callable.function";
		const char* const function_str = "anura.function";
		const char* const callable_str = "anura.callable";
		const char* const lib_functions_str = "anura.lib";

		const char* const error_handler_fcn_reg_key = "error_handler";
	}

	void LuaContext::setSelfCallable(game_logic::FormulaCallable& callable)
	{
		using namespace game_logic;
		// Gets the global "Anura" table
		lua_getglobal(getContextPtr(), anura_str);			// (-0,+1,e)

		// Create a new holder for a fomula callable for the given callable
		FormulaCallable** a = static_cast<FormulaCallable**>(lua_newuserdata(getContextPtr(), sizeof(FormulaCallable*))); //(-0,+1,e)
		*a = &callable;
		intrusive_ptr_add_ref(*a);

		// Set metatable for the callable
		luaL_getmetatable(getContextPtr(), callable_str);	// (-0,+1,e)
		lua_setmetatable(getContextPtr(), -2);			// (-1,+0,e)

		// Set the me Anura["me"] = callable
		lua_setfield(getContextPtr(), -2, "me");			// (-1,+0,e)
		lua_pop(getContextPtr(),1);						// (-n(1),+0,-)
	}

	bool LuaContext::dostring(const std::string&name, const std::string&str, game_logic::FormulaCallable* callable)
	{
		if(callable) {
			setSelfCallable(*callable);
		}

		lua_State * L = getContextPtr();

		lua_pushstring(L, error_handler_fcn_reg_key);
		lua_rawget(L, LUA_REGISTRYINDEX);

		if (luaL_loadbuffer(L, str.c_str(), str.size(), name.c_str()) || lua_pcall(L, 0, 0, -2))
		{
			const char* a = lua_tostring(L, -1);
			//LOG_DEBUG(a);
			ASSERT_LOG(false, "Lua: " << a);
			lua_pop(L, 1);
			return true;
		}
		return false;
	}

	bool LuaContext::dofile(const std::string&name, const std::string&str, game_logic::FormulaCallable* callable)
	{
		std::string file_contents = sys::read_file(module::map_file(str));
		return dostring(name, file_contents, callable);
	}

	namespace
	{
		struct ffl_variant_lib_userdata
		{
			variant value;
		};

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
						lua_pushnumber(L, n+1);						// (-0,+1,-)
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
					FormulaCallable** a = static_cast<FormulaCallable**>(lua_newuserdata(L, sizeof(FormulaCallable*))); //(-0,+1,e)
					*a = const_cast<FormulaCallable*>(value.as_callable());
					intrusive_ptr_add_ref(*a);

					luaL_getmetatable(L, callable_str);	// (-0,+1,e)
					lua_setmetatable(L, -2);			// (-1,+0,e)
					return 1;
				}
				case variant::VARIANT_TYPE_FUNCTION: {
					using namespace game_logic;
					//(-0,+1,e)
					ffl_variant_lib_userdata* ud = static_cast<ffl_variant_lib_userdata*>(lua_newuserdata(L, sizeof(ffl_variant_lib_userdata)));
					ud->value = value;

					luaL_getmetatable(L, lib_functions_str);	// (-0,+1,e)
					lua_setmetatable(L, -2);					// (-1,+0,e)
					return 1;
				}
				case variant::VARIANT_TYPE_CALLABLE_LOADING:
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
					if(std::abs(d - round(d)) < 1e-14 && d < double(std::numeric_limits<int>::max())) {
						return variant(int(d));
					} else {
						return variant(d);
					}
					break;
				}
				case LUA_TFUNCTION: {
					 int ref = luaL_ref(L, LUA_REGISTRYINDEX);
					 return variant(new LuaFunctionReference(L, ref));
					break;
				}
				case LUA_TTABLE:
					break;
				case LUA_TUSERDATA:
				case LUA_TTHREAD:
				case LUA_TLIGHTUSERDATA:
				default:
					luaL_error(L, "Unsupported type to convert on stack: %s", lua_typename(L,t));
			}
			return variant();
		}

		static int call_function(lua_State* L)
		{
			using namespace game_logic;
			ffl_function_userdata* fn = static_cast<ffl_function_userdata*>(luaL_checkudata(L,1,function_str)); // (-0,+0,-)
			auto& symbols = get_formula_functions_symbol_table();
			std::vector<ExpressionPtr> args;
			int nargs = lua_gettop(L);
			for(int n = 2; n <= nargs; ++n) {
				args.push_back(ExpressionPtr(new VariantExpression(lua_value_to_variant(L, n))));
			}
			auto& player = Level::current().player()->getEntity();
			auto value = symbols.createFunction(fn->name, args, player.getDefinition());
			if(value == nullptr) {
				luaL_error(L, "Function not found: %s", fn->name);
			}
			auto ret = value->evaluate(player);
			if(ret.is_callable()) {
				player.executeCommand(ret);
			} else {
				return variant_to_lua_value(L, ret);
			}
			return 0;
		}

		static int gc_function(lua_State* L)
		{
			ffl_function_userdata* fn = static_cast<ffl_function_userdata*>(luaL_checkudata(L,1,function_str)); // (-0,+0,-)
			delete[] fn->name;
			fn->name = nullptr;
			return 0;
		}

		static int remove_callable_reference(lua_State* L)
		{
			using namespace game_logic;
			FormulaCallable* callable = *static_cast<FormulaCallable**>(lua_touserdata(L, 1));	// (-0,+0,-)
			luaL_argcheck(L, callable != nullptr, 1, "'callable' was null");
			intrusive_ptr_release(callable);
			return 0;
		}

		static int get_callable_index(lua_State* L)
		{
			using namespace game_logic;
			FormulaCallable* callable = *static_cast<FormulaCallable**>(luaL_checkudata(L, 1, callable_str));	// (-0,+0,-)
			const char *name = lua_tostring(L, 2);						// (-0,+0,e)
			variant value = callable->queryValue(name);
			if(!value.is_null()) {
				return variant_to_lua_value(L, value);
			}
			ffl_function_userdata* fn = static_cast<ffl_function_userdata*>(lua_newuserdata(L, sizeof(ffl_function_userdata))); //(-0,+1,e)
			fn->name = new char[strlen(name)+1];			
			strcpy(fn->name, name);
			luaL_getmetatable(L, callable_function_str);		// (-0,+1,e)
			lua_setmetatable(L, -2);					// (-1,+0,e)
			return 1;
		}

		static int set_callable_index(lua_State* L)
		{
			// stack -- table, key, value
			using namespace game_logic;
			FormulaCallable* callable = *static_cast<FormulaCallable**>(luaL_checkudata(L, 1, callable_str));	// (-0,+0,-)
			const char *name = lua_tostring(L, 2);						// (-0,+0,e)
			variant value = lua_value_to_variant(L, 3);
			callable->mutateValue(name, value);
			return 0;
		}

		static int serialize_callable(lua_State* L) 
		{
			using namespace game_logic;
			std::string res;
			FormulaCallable* callable = *static_cast<FormulaCallable**>(luaL_checkudata(L, 1, callable_str));	// (-0,+0,-)
			std::vector<FormulaInput> inputs = callable->inputs();
			for(auto inp : inputs) {
				std::stringstream ss;
				static const char* const access[] = {
					"ro",
					"wo",
					"rw",
				};
				if(inp.access != FORMULA_ACCESS_TYPE::READ_ONLY 
					&& inp.access != FORMULA_ACCESS_TYPE::WRITE_ONLY
					&& inp.access != FORMULA_ACCESS_TYPE::READ_WRITE) {
					luaL_error(L, "Unrecognised access mode: ", inp.access);
				}
				ss << inp.name << "(" << access[static_cast<int>(inp.access)] << ") : " << callable->queryValue(inp.name) << std::endl;
				res += ss.str();
			}
			lua_pushstring(L, res.c_str());			// (-0,+1,-)
			return 1;
		}

		static int call_callable_function(lua_State* L)
		{
			using namespace game_logic;

			if(luaL_testudata(L, 1, function_str)) {
				return call_function(L);
			}

			ffl_function_userdata* fn = static_cast<ffl_function_userdata*>(luaL_checkudata(L,1,callable_function_str)); // (-0,+0,-)
			std::vector<ExpressionPtr> args;
			int nargs = lua_gettop(L);
			for(int n = 3; n <= nargs; ++n) {
				args.push_back(ExpressionPtr(new VariantExpression(lua_value_to_variant(L, n))));
			}
			FormulaCallable* callable = *static_cast<FormulaCallable**>(luaL_checkudata(L, 2, callable_str));	// (-0,+0,-)

			auto& symbols = get_formula_functions_symbol_table();
			auto value = symbols.createFunction(fn->name, args, nullptr);
			if(value != nullptr) {
				auto ret = value->evaluate(*callable);
				if(ret.is_callable()) {
					callable->executeCommand(ret);
				} else {
					return variant_to_lua_value(L, ret);
				}
				return 0;
			}

			value = get_custom_object_functions_symbol_table().createFunction(fn->name, args, nullptr);
			if(value != nullptr) {
				auto ret = value->evaluate(*callable);
				if(ret.is_callable()) {
					callable->executeCommand(ret);
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
			ffl_function_userdata* fn = static_cast<ffl_function_userdata*>(luaL_checkudata(L,1,callable_function_str)); // (-0,+0,-)
			delete[] fn->name;
			fn->name = nullptr;
			return 0;
		}

		static int call_variant_function(lua_State* L)
		{
			using namespace game_logic;
			ffl_variant_lib_userdata* ud = static_cast<ffl_variant_lib_userdata*>(luaL_checkudata(L,1,lib_functions_str)); // (-0,+0,-)
			std::vector<variant> args;
			int nargs = lua_gettop(L);
			for(int n = 2; n <= nargs; ++n) {
				args.push_back(lua_value_to_variant(L, n));
			}
			return variant_to_lua_value(L, (ud->value)(args));
		}

		static int gc_variant_function(lua_State* L)
		{
			ffl_variant_lib_userdata* ud = static_cast<ffl_variant_lib_userdata*>(luaL_checkudata(L,1,lib_functions_str)); // (-0,+0,-)
			ud->value = variant();
			return 0;
		}

		static int index_variant_function(lua_State* L)
		{
			// stack -- table, key
			// (-0,+0,-)
			ffl_variant_lib_userdata* ud = static_cast<ffl_variant_lib_userdata*>(luaL_checkudata(L,1,lib_functions_str));
			// (-0,+0,e)
			const char *name = lua_tostring(L, 2);
			// (-1,+0,-)
			lua_pop(L,1);
			
			if(ud->value.is_callable()) {
				ud->value = ud->value.as_callable()->queryValue(name);
			} else {
				return variant_to_lua_value(L, ud->value);
			}
			return 1;
		}

		const luaL_Reg gFFLFunctions[] = {
			{"__call", call_function},
			{"__gc", gc_function},
			{nullptr, nullptr},
		};

		const luaL_Reg gLibMetaFunctions[] = {
			{"__call", call_variant_function},
			{"__gc", gc_variant_function},
			{"__index", index_variant_function},
			{nullptr, nullptr},
		};

		const luaL_Reg gFFLCallableFunctions[] = {
			{"__call", call_callable_function},
			{"__gc", gc_callable_function},
			{nullptr, nullptr},
		};

		const luaL_Reg gCallableFunctions[] = {
			{"__index", get_callable_index},
			{"__newindex", set_callable_index},
			{"__tostring", serialize_callable},
			{"__gc", remove_callable_reference},
			{nullptr, nullptr},
		};

		static int get_level(lua_State* L) 
		{
			Level& lvl = Level::current();

			Level** a = static_cast<Level**>(lua_newuserdata(L, sizeof(Level*))); //(-0,+1,e)
			*a = &lvl;
			intrusive_ptr_add_ref(&lvl);

			luaL_getmetatable(L, callable_str);		// (-0,+1,e)
			lua_setmetatable(L, -2);			// (-1,+0,e)
			return 1;
		}
		
		static int get_lib(lua_State* L)
		{
			//(-0,+1,e)
			ffl_variant_lib_userdata* ud = static_cast<ffl_variant_lib_userdata*>(lua_newuserdata(L, sizeof(ffl_variant_lib_userdata)));
			ud->value = variant(game_logic::get_library_object().get());
			
			luaL_getmetatable(L, lib_functions_str);	// (-0,+1,e)
			lua_setmetatable(L, -2);					// (-1,+0,e)
			return 1;
		}

		static int anura_table_index(lua_State* L) 
		{
			const char *name = lua_tostring(L, 2);						// (-0,+0,e)

			ffl_function_userdata* fn = static_cast<ffl_function_userdata*>(lua_newuserdata(L, sizeof(ffl_function_userdata))); //(-0,+1,e)
			fn->name = new char[strlen(name)+1];			
			strcpy(fn->name, name);
			luaL_getmetatable(L, function_str);		// (-0,+1,e)
			lua_setmetatable(L, -2);					// (-1,+0,e)
			return 1;
		}
		
		static const struct luaL_Reg anura_functions [] = {
			{"level", get_level},
			{"lib", get_lib},
			{nullptr, nullptr},
		};

		static void push_anura_table(lua_State* L) 
		{
			lua_getglobal(L, anura_str);					// (-0,+1,e)
			if (lua_isnil(L, -1)) {						// (-0,+0,e)
				lua_pop(L, 1);							// (-n(1),+0,e)
				lua_newtable(L);						// (-0,+1,e)

				luaL_newmetatable(L, anura_meta_str);		// (-0,+1,e)
				lua_pushcfunction(L, anura_table_index);// (-0,+1,e)
				lua_setfield(L, -2, "__index");			// (-1,+0,e)
				lua_setmetatable(L, -2);				// (-1,+0,e)
			}
			luaL_setfuncs(L, anura_functions, 0);		// (-nup(0),+0,e)
			lua_setglobal(L, anura_str);					// (-1,+0,e)
		}
	}

	bool LuaContext::execute(const variant& value, game_logic::FormulaCallable* callable)
	{
		bool res = false;
		if(callable) {
			setSelfCallable(*callable);
		}
		if(value.is_string()) {
			res = dostring("", value.as_string());
		} else {
			LuaCompiledPtr compiled = value.try_convert<LuaCompiled>();
			ASSERT_LOG(compiled != nullptr, "FATAL: object given couldn't be converted to type 'lua_compiled'");
			res = compiled->run(getContextPtr());
		}
		return res;
	}

	LuaContext::LuaContext()
	{
		init();
	}

	LuaContext::LuaContext(game_logic::FormulaCallable& callable)
	{
		init();
		setSelfCallable(callable);
	}

	LuaContext::~LuaContext()
	{
	}

	void LuaContext::init()
	{
		// initialize Lua
		state_.reset(luaL_newstate(), [](lua_State* L) { lua_close(L); });

		lua_State * L = getContextPtr();

		// load various Lua libraries
		luaopen_base(L);
		luaopen_string(L);
		luaopen_table(L);
		luaopen_math(L);
		luaopen_io(L);
		luaopen_debug(L);
		luaL_openlibs(L);

		// load the debug traceback function to registry, to be used as error handler
		lua_pushstring(L, error_handler_fcn_reg_key);
		lua_getglobal(L, "debug");
		lua_getfield(L, -1, "traceback");
		lua_remove(L, -2);
		lua_rawset(L, LUA_REGISTRYINDEX);
		lua_pop(L, 1);

		luaL_newmetatable(L, function_str);
		luaL_setfuncs(L, gFFLFunctions, 0);	

		luaL_newmetatable(L, callable_str);
		luaL_setfuncs(L, gCallableFunctions, 0);

		luaL_newmetatable(L, callable_function_str);
		luaL_setfuncs(L, gFFLCallableFunctions, 0);

		luaL_newmetatable(L, lib_functions_str);
		luaL_setfuncs(L, gLibMetaFunctions, 0);

		push_anura_table(L);

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

	namespace 
	{
		int chunk_writer(lua_State *L, const void* p, size_t sz, void* ud)
		{
			CompiledChunk* chunks = reinterpret_cast<CompiledChunk*>(ud);
			chunks->addChunk(p, sz);
			return 0;
		}

		const char* chunk_reader(lua_State* L, void* ud, size_t* sz)
		{
			CompiledChunk* chunks = reinterpret_cast<CompiledChunk*>(ud);
			auto& it = chunks->current(); chunks->next();
			if(sz) {
				*sz = it.size();
			}
			return &it[0];
		}
	}

	LuaCompiledPtr LuaContext::compile(const std::string& name, const std::string& str)
	{
		LuaCompiled* chunk = new LuaCompiled();
		luaL_loadbuffer(getContextPtr(), str.c_str(), str.size(), name.c_str());		// (-0,+1,-)
#if LUA_VERSION_NUM == 502
		lua_dump(getContextPtr(), chunk_writer, reinterpret_cast<void*>(chunk));		// (-0,+0,-)
#elif LUA_VERSION_NUM == 503
		lua_dump(getContextPtr(), chunk_writer, reinterpret_cast<void*>(chunk), 0);		// (-0,+0,-)
#endif
		lua_pop(getContextPtr(), 1);													// (-n(1),+0,-)
		chunk->addChunk(0, 0);
		return LuaCompiledPtr(chunk);
	}

	CompiledChunk* LuaContext::compileChunk(const std::string& name, const std::string& str)
	{
		CompiledChunk* chunk = new CompiledChunk();
		luaL_loadbuffer(getContextPtr(), str.c_str(), str.size(), name.c_str());		// (-0,+1,-)
#if LUA_VERSION_NUM == 502
		lua_dump(getContextPtr(), chunk_writer, reinterpret_cast<void*>(chunk));		// (-0,+0,-)
#elif LUA_VERSION_NUM == 503
		lua_dump(getContextPtr(), chunk_writer, reinterpret_cast<void*>(chunk), 0);		// (-0,+0,-)
#endif
		lua_pop(getContextPtr(), 1);													// (-n(1),+0,-)
		chunk->addChunk(0, 0);
		return chunk;
	}


	LuaCompiled::LuaCompiled()
	{
	}

	LuaCompiled::~LuaCompiled()
	{
	}

	bool CompiledChunk::run(lua_State* L) const 
	{
		// Load the error handler for pcall
		lua_pushstring(L, error_handler_fcn_reg_key);
		lua_rawget(L, LUA_REGISTRYINDEX);

		chunks_it_ = chunks_.begin();
		if(lua_load(L, chunk_reader, reinterpret_cast<void*>(const_cast<CompiledChunk*>(this)), nullptr, nullptr) || lua_pcall(L, 0, 0, -2)) {
			const char* a = lua_tostring(L, -1);
			//LOG_DEBUG(a);
			ASSERT_LOG(false, "Lua: " << a);
			lua_pop(L, 1);
			return true;
		}
		return false;
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(LuaCompiled)
	BEGIN_DEFINE_FN(execute, "(object) ->any")
		lua::LuaContext ctx;
		game_logic::FormulaCallable* callable = const_cast<game_logic::FormulaCallable*>(FN_ARG(0).as_callable());
		ctx.setSelfCallable(*callable);
		obj.run(ctx.getContextPtr());
		//return lua_value_to_variant(ctx.getContextPtr());
		return variant();
	END_DEFINE_FN
	DEFINE_FIELD(dummy, "int")
		return variant(0);
	END_DEFINE_CALLABLE(LuaCompiled)


	LuaFunctionReference::LuaFunctionReference(lua_State* L, int ref)
		: ref_(ref), L_(L)
	{
	}

	LuaFunctionReference::~LuaFunctionReference()
	{
		luaL_unref(L_, LUA_REGISTRYINDEX, ref_);
	}

	variant LuaFunctionReference::getValue(const std::string& key) const
	{
		return variant();
	}

	variant LuaFunctionReference::call()
	{
		// load error handler for pcall
		lua_pushstring(L_, error_handler_fcn_reg_key);
		lua_rawget(L_, LUA_REGISTRYINDEX);
		int error_handler_index = lua_gettop(L_);

		lua_rawgeti(L_, LUA_REGISTRYINDEX, ref_);
		// XXX: decide how arguments will get passed in and push them onto the lua stack here.
		int nargs = 0;
		if(lua_pcall(L_, nargs, LUA_MULTRET, error_handler_index) != LUA_OK) {				// (-(nargs + 1), +(nresults|1),-)
			const char* a = lua_tostring(L_, -1);
			//LOG_DEBUG(a);
			ASSERT_LOG(false, "Lua: " << a);
			lua_settop(L_, 0);
			return variant();
		}
		lua_remove(L_, error_handler_index);
		int nresults = lua_gettop(L_);
		// Return multiple results as a list.
		if(nresults > 1) {
			std::vector<variant> v;
			for(int n = 1; n < nresults; ++n) {
				v.push_back(lua_value_to_variant(L_, n));
			}
			return variant(&v);
		} else if(nresults == 1) {
			return variant(lua_value_to_variant(L_, 1));
		}
		return variant();
	}
}

UNIT_TEST(lua_test)
{
	lua::LuaContext ctx;
}

#endif
