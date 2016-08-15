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

#include "eris/lua.h"
#include "eris/eris.h"
#include "eris/lauxlib.h"
#include "eris/lualib.h"
#include "eris/lua_utilities.h"

#include "custom_object.hpp"
#include "custom_object_functions.hpp"
#include "filesystem.hpp"
#include "formula_function_registry.hpp"
#include "formula_object.hpp"
#include "lua_iface.hpp"
#include "level.hpp"
#include "module.hpp"
#include "playable_custom_object.hpp"
#include "variant_type.hpp"
#include "unit_test.hpp"

namespace lua
{
	using game_logic::FormulaCallable;
	using game_logic::FormulaCallablePtr;
	using game_logic::FormulaObject;
	using game_logic::FormulaObjectPtr;

	namespace
	{
		class assert_stack_neutral {
		private:
			lua_State * L_;
			int stack_;
			std::string file_;
			int line_;
		public:
			assert_stack_neutral(lua_State *L, const char * file, int line)
				: L_(L)
				, stack_(lua_gettop(L))
				, file_(file)
				, line_(line)
			{}
			~assert_stack_neutral()
			{
				ASSERT_LOG(stack_ == lua_gettop(L_), "lua stack corruption detected: start " << stack_ << " end " << lua_gettop(L_) << "  [" << file_ << ":" << line_ << "]");
			}
		};
		#define TOKEN_PASTE(x, y) x ## y
		#define ASSERT_STACK_NEUTRAL(L) assert_stack_neutral TOKEN_PASTE(anonymous_variable_, __LINE__) (L, __FILE__, __LINE__)

		const char* const anura_str = "Anura";
		const char* const anura_meta_str = "anura.meta";
		const char* const callable_function_str = "anura.callable.function";
		const char* const function_str = "anura.function";
		const char* const callable_str = "anura.callable";
		const char* const lib_functions_str = "anura.lib";
		const char* const object_str = "anura.object";

		const char* const error_handler_fcn_reg_key = "error_handler";

		const char* const me_key = "me";
		const char* const persist_key = "persist";
		const char* const unpersist_key = "unpersist";

		void dump_stack (lua_State * L);
		void load_libs(lua_State * L) {
			ASSERT_STACK_NEUTRAL(L);
			static const luaL_Reg loadedlibs[] = {
				{"_G", luaopen_base},
//				{LUA_LOADLIBNAME, luaopen_package},
//				{LUA_COLIBNAME, luaopen_coroutine},
				{LUA_TABLIBNAME, luaopen_table},
//				{LUA_IOLIBNAME, luaopen_io},
//				{LUA_OSLIBNAME, luaopen_os},
//				{LUA_STRLIBNAME, luaopen_string},
//				{LUA_BITLIBNAME, luaopen_bit32},
//				{LUA_MATHLIBNAME, luaopen_math},
				{LUA_DBLIBNAME, luaopen_debug},
//				{LUA_ERISLIBNAME, luaopen_eris},
				{nullptr, nullptr}
			};

			lua_pushstring(L, persist_key);
			lua_rawget(L, LUA_REGISTRYINDEX);
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				lua_newtable(L);
				lua_pushstring(L, persist_key);
				lua_pushvalue(L, -2);
				lua_rawset(L, LUA_REGISTRYINDEX);
			}
			ASSERT_LOG(lua_gettop(L) == 1, "stack is bad");				// persist

			lua_pushstring(L, unpersist_key);
			lua_rawget(L, LUA_REGISTRYINDEX);
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				lua_newtable(L);
				lua_pushstring(L, unpersist_key);
				lua_pushvalue(L, -2);
				lua_rawset(L, LUA_REGISTRYINDEX);
			}
			ASSERT_LOG(lua_gettop(L) == 2, "stack is bad");				// persist unpersist

			const luaL_Reg *lib;
			/* call open functions from 'loadedlibs' and set results to global table */
			for (lib = loadedlibs; lib->func; lib++) {
				luaL_requiref(L, lib->name, lib->func, 1);			// persist unpersist lib
				lua_pushstring(L, lib->name);					// persist unpersist lib name
				lua_pushvalue(L, -1);						// persist unpersist lib name name
				lua_pushvalue(L, -3); 						// persist unpersist lib name name lib
				lua_settable(L, -5); //assign it to the unpersist table		// persist unpersist lib name
				lua_settable(L, -4); //assign it to the persistent table	// persist unpersist
			}
			lua_pop(L, 2);
		}

		// Two functions to interpret the error codes that lua can give when loading or executing lua, and report the errors
		void handle_load_error(lua_State * L, int err_code) {
			if (err_code != LUA_OK) {
				const char* a = lua_tostring(L, -1);

				std::string error_class = "an unknown type of error";
				switch(err_code) {
				case LUA_ERRSYNTAX:
					error_class = "a syntax error";
					break;
				case LUA_ERRMEM:
					error_class = "a memory allocation error";
					break;
				case LUA_ERRGCMM:
					error_class = "an error while running the garbage collector";
					break;
				}

				// LOG_DEBUG(a);
				ASSERT_LOG(false, "Lua error (" << error_class << "): " << (a ? a : "(null string)"));
				lua_pop(L, 1);
			}
		}

		void handle_pcall_error(lua_State * L, int err_code) {
			if(err_code != LUA_OK) {
				const char* a = lua_tostring(L, -1);

				std::string error_class = "an unknown type of error";
				switch(err_code) {
				case LUA_ERRRUN:
					error_class = "a runtime error";
					break;
				case LUA_ERRMEM:
					error_class = "a memory allocation error";
					break;
				case LUA_ERRERR:
					error_class = "an error in the error handler function";
					break;
				}

				//LOG_DEBUG(a);
				ASSERT_LOG(false, "Lua error (" << error_class << "): " << (a ? a : "(null string)"));
				lua_pop(L, 1);
			}
		}

		// Describe the type of the lua value. If it's a table / userdata, report string description of the metatable also.
		std::string describe_lua_value(lua_State *L, int ndx) {
			ASSERT_STACK_NEUTRAL(L);
			int t = lua_type(L, ndx);
			const char * n = lua_typename(L, t);
			assert(n);
			std::string ret = n;
			if (t == LUA_TUSERDATA || t == LUA_TTABLE) {
				if (lua_getmetatable(L, ndx)) { // returns 0 if there is no metatable
					if (!lua_isnil(L, -1)) {
						if (lua_isstring(L, -1)) {
							ret += " (";
							ret += lua_tostring(L, -1);
							ret += ")";
						} else {
							ret += " (unknown kind)";
						}
					}
					lua_pop(L, 1);
				}
			} else if (t == LUA_TSTRING) {
				ret += " \"";
				ret += lua_tostring(L, ndx);
				ret += "\"";
			}
			return ret;
		}

		void dump_stack(lua_State * L) {
			ASSERT_STACK_NEUTRAL(L);

			std::cerr << "Stack contents:\n***\n";
			for (int i = 1; i <= lua_gettop(L); ++i) {
				std::cerr << "[" << i << "]: " << describe_lua_value(L, i);
				if (const char * foo = lua_tostring(L, i)) {
					std::cerr << " \"" << foo << "\"\n";
				}
				std::cerr << std::endl;
			}
			std::cerr << "***\n";
		}

		void print_traceback(lua_State * L) {
			ASSERT_STACK_NEUTRAL(L);

			lua_getglobal(L,"debug");
			lua_getfield(L, -1, "traceback");
			lua_call(L, 0, 0);
			if (const char * bt = lua_tostring(L, -1)) {
				std::cerr << bt << std::endl;
			} else {
				std::cerr << "<no lua stack backtrace>\n";
			}
			lua_pop(L, 1);
		}

		std::string full_dump(lua_State * L, size_t indent = 0) {
			ASSERT_STACK_NEUTRAL(L);
//			std::cerr << "+ top: " << lua_gettop(L) << std::endl;
			if (lua_istable(L, -1)) {
				if (indent > 6) {
					return "table (too deep)\n";
				}
				std::string result = "table:\n";
				result += std::string(indent, '\t') + "{\n";

//				std::cerr << "top: " << lua_gettop(L) << std::endl;

				lua_pushnil(L);
				while(lua_next(L, -2)) {
					{
					ASSERT_STACK_NEUTRAL(L);

					result += std::string(indent + 1, '\t');

					lua_pushvalue(L, -2);
					result += full_dump(L, indent + 1);
					lua_pop(L, 1);

					if (lua_istable(L, -2)) {
						result += std::string(indent + 1, '\t');
					}
					result += " -> ";

					result += full_dump(L, indent + 1);

					if (!lua_istable(L, -1)) {
						result += "\n";
					}
					}
					lua_pop(L, 1);
				}
				//lua_pop(L, 1);
//				std::cerr << "- top: " << lua_gettop(L) << std::endl;
				result += std::string(indent, '\t') + "}\n";
				return result;
			} else {
//				std::cerr << "% top: " << lua_gettop(L) << std::endl;
				return describe_lua_value(L, -1);
			}
		}
	}

	void LuaContext::setSelfCallable(FormulaCallable& callable)
	{
		lua_State * L = getState();

		// Gets the global "Anura" table
		lua_getglobal(L, anura_str);			// (-0,+1,e)

		// Create a new holder for a fomula callable for the given callable
		auto a = static_cast<FormulaCallablePtr*>(lua_newuserdata(L, sizeof(FormulaCallablePtr))); //(-0,+1,e)
		luaL_setmetatable(L, callable_str);
		new (a) FormulaCallablePtr(& callable);

		// Set the me Anura["me"] = callable
		lua_setfield(L, -2, "me");			// (-1,+0,e)
		lua_pop(L, 1);						// (-n(1),+0,-)
	}

	void LuaContext::setSelfObject(FormulaObject & object)
	{
		lua_State * L = getState();

		// Gets the global "Anura" table
		lua_getglobal(L, anura_str);			// Anura
		ASSERT_LOG(lua_istable(L, -1), "Anura table is missing");

		// Create a new holder for a fomula callable for the given callable
		auto a = static_cast<FormulaObjectPtr*>(lua_newuserdata(L, sizeof(FormulaObjectPtr))); // Anura, me
		luaL_setmetatable(L, object_str);
		new (a) FormulaCallablePtr(& object); 		// Anura, me
		lua_pushstring(L, "me");			// Anura, me, "me"
		lua_pushvalue(L, -2);				// Anura, me, "me", me
		lua_rawset(L, LUA_REGISTRYINDEX);		// Anura, me

		// Set the me Anura["me"] = callable
		lua_setfield(L, -2, "me");			// Anura
		lua_pop(L, 1);					//
	}

	std::string LuaContext::persist()
	{
		// First assemble the persistent table.
		lua_State * L = getState();

		lua_newtable(L);					// persist
		lua_pushstring(L, me_key);				// persist "me"
		lua_rawget(L, LUA_REGISTRYINDEX);			// persist me/nil
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);					// persist
		} else {
			lua_pushstring(L, me_key);			// persist me "me"
			lua_rawset(L, -3);				// persist
		}

		lua_getglobal(L, "_G");					// persist _G
		eris_persist(L, -2, -1);				// string (if succeeded)

		const char * str = lua_tostring(L, -1);
		ASSERT_LOG(str, "eris_persist returned a null string");
		std::string ret = str;
		lua_pop(L, 3);
		return ret;
//		return "";
	}

	bool LuaContext::dostring(const std::string&name, const std::string&str, game_logic::FormulaCallable* callable)
	{
		if(callable) {
			setSelfCallable(*callable);
		}

		lua_State * L = getState();

		lua_pushstring(L, error_handler_fcn_reg_key);
		lua_rawget(L, LUA_REGISTRYINDEX);

		if (int err_code = luaL_loadbuffer(L, str.c_str(), str.size(), name.c_str())) {
			handle_load_error(L, err_code);
			return true;
		}
		if (int err_code = lua_pcall(L, 0, 0, -2)) {
			handle_pcall_error(L, err_code);
			return true;
		}
		lua_pop(L, 1); // pop the error handler
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
					FormulaCallable * callable = const_cast<FormulaCallable*>(value.as_callable());

					// If it's actually a formula object, make it one of those since its nicer (we get better type conversion)
					if (FormulaObject * obj = dynamic_cast<FormulaObject*>(callable)) {
						auto a = static_cast<FormulaObjectPtr*>(lua_newuserdata(L, sizeof(FormulaObjectPtr))); //(-0,+1,e)
						luaL_setmetatable(L, object_str);
						new (a) FormulaObjectPtr(obj);
					} else {
						auto a = static_cast<FormulaCallablePtr*>(lua_newuserdata(L, sizeof(FormulaCallablePtr))); //(-0,+1,e)
						luaL_setmetatable(L, callable_str);
						new (a) FormulaCallablePtr(const_cast<FormulaCallable*>(value.as_callable()));
					}

					return 1;
				}
				case variant::VARIANT_TYPE_FUNCTION: {
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

		static variant lua_value_to_variant(lua_State* L, int ndx, variant_type_ptr desired_type = nullptr)
		{
			ASSERT_STACK_NEUTRAL(L);

			static int recursion_preventer = 0;
			if (recursion_preventer > 20) {
				std::cerr << "Preventing recursion in lua_value_to_variant:\n";
				dump_stack(L);
				print_traceback(L);
				return variant();
			}

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
					lua_pushvalue(L, ndx);
					int ref = luaL_ref(L, LUA_REGISTRYINDEX);
					return variant(new LuaFunctionReference(L, ref));
					break;
				}
				case LUA_TNIL: {
					return variant();
				}
				case LUA_TTABLE: {
					// If we specifically desire a list, then convert only the array part of the lua table to a list, and pass on type info.
					if (desired_type && desired_type->is_list_of()) {
						std::vector<variant> temp;

						for (int i = 1; ; ++i) {
							lua_rawgeti(L, ndx, i);
							if (lua_isnil(L, -1)) {
								lua_pop(L, 1);
								break;
							}
							recursion_preventer++;
							temp.push_back(lua_value_to_variant(L, -1, desired_type->is_list_of()));
							recursion_preventer--;

							lua_pop(L, 1);
						}

						return variant(&temp);
					}

					// By default we will return a map. Check if the map has desired key / value types (might be tables also)
					variant_type_ptr key_type;
					variant_type_ptr value_type;

					if (desired_type) {
						auto p = desired_type->is_map_of();
						if (p.first && p.second) {
							key_type = p.first;
							value_type = p.second;
						}
					}

					std::map<variant, variant> temp;

					lua_pushvalue(L, ndx); //copy the item so we can iterate over it
					lua_pushnil(L);

					while (lua_next(L, -2)) {

						recursion_preventer++;
						variant key = lua_value_to_variant(L, -2, key_type);
						temp[key] = lua_value_to_variant(L, -1, value_type);
						recursion_preventer--;

						lua_pop(L, 1);
					}
					lua_pop(L, 1);
					return variant(&temp);
				}
				case LUA_TUSERDATA:
					if (auto callable_ptr_ptr = static_cast<FormulaCallablePtr*>(luaL_testudata(L, ndx, callable_str))) {
						return variant(callable_ptr_ptr->get());
					}
					if (auto object_ptr_ptr = static_cast<FormulaObjectPtr*>(luaL_testudata(L, ndx, object_str))) {
						return variant(object_ptr_ptr->get());
					}
				case LUA_TTHREAD:
				case LUA_TLIGHTUSERDATA:
				default:
					luaL_error(L, "Unsupported type to convert on stack: %s", describe_lua_value(L, ndx).c_str());
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

			if (auto * player_ptr = Level::current().player()) {
				auto& player = player_ptr->getEntity();
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
			} else {
				/*if(auto fcn = symbols.getFormulaFunction(fn->name)) {
					auto expr = fcn->generateFunctionExpression(args);

					MapFormulaCallable blank(nullptr);
					auto ret = expr->evaluate(blank);
					return variant_to_lua_value(L, ret);*/
				auto value = symbols.createFunction(fn->name, args, nullptr);
				if (value == nullptr) {
					luaL_error(L, "[%s, %d] Function not found: %s", __FILE__, __LINE__, fn->name);
				} else {
					MapFormulaCallable blank(nullptr);
					auto ret = value->evaluate(blank);
					return variant_to_lua_value(L, ret);
				}
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

		static int gc_callable(lua_State* L)
		{
			if (auto d = static_cast< FormulaCallablePtr *> (luaL_testudata(L, 1, callable_str))) {
				d->~FormulaCallablePtr();
			} else {
				LOG_DEBUG("gc_callable called on an item \"" << describe_lua_value(L, 1) << "\" which is not a callable.");
				lua_pushstring(L, "gc_callable did not find a callable to destroy, garbage collection failure");
				lua_error(L);
			}
			return 0;
		}

		static int get_callable_index(lua_State* L)
		{
			auto callable = *static_cast<FormulaCallablePtr*>(luaL_checkudata(L, 1, callable_str));	// (-0,+0,-)
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
			auto callable = *static_cast<FormulaCallablePtr*>(luaL_checkudata(L, 1, callable_str));	// (-0,+0,-)
			const char *name = lua_tostring(L, 2);						// (-0,+0,e)
			variant value = lua_value_to_variant(L, 3);
			callable->mutateValue(name, value);
			return 0;
		}

		static int serialize_callable(lua_State* L)
		{
			using namespace game_logic;
			std::string res;
			auto callable = *static_cast<FormulaCallablePtr*>(luaL_checkudata(L, 1, callable_str));	// (-0,+0,-)
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
			auto callable = *static_cast<FormulaCallablePtr*>(luaL_checkudata(L, 2, callable_str));	// (-0,+0,-)

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

		static int get_object_index(lua_State* L)
		{
			// takes table, key on stack, returns result
			auto object = *static_cast<FormulaObjectPtr*>(luaL_checkudata(L, 1, object_str));
			const char * name = lua_tostring(L, 2);
			variant value = object->queryValue(name);
			return variant_to_lua_value(L, value);
		}

		static int set_object_index(lua_State* L)
		{
			// stack -- table, key, value
			auto object = *static_cast<FormulaObjectPtr*>(luaL_checkudata(L, 1, object_str));
			const char *name = lua_tostring(L, 2);
			if (!name) {
				luaL_argerror(L, 2, "expected: string");
			}
			std::string idx = name;
			variant value = lua_value_to_variant(L, 3, object->getPropertySetType(idx));
			object->mutateValue(name, value);
			return 0;
		}

		static int serialize_object(lua_State* L)
		{
			using namespace game_logic;
			std::string res;
			auto object = *static_cast<FormulaObjectPtr*>(luaL_checkudata(L, 1, object_str));	// (-0,+0,-)
			std::vector<FormulaInput> inputs = object->inputs();
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
				ss << inp.name << "(" << access[static_cast<int>(inp.access)] << ") : " << object->queryValue(inp.name) << std::endl;
				res += ss.str();
			}
			lua_pushstring(L, res.c_str());			// (-0,+1,-)
			return 1;
		}

		static int gc_object(lua_State* L)
		{
			if (auto d = static_cast< FormulaObjectPtr *> (luaL_testudata(L, 1, object_str))) {
				d->~FormulaObjectPtr();
			} else {
				LOG_DEBUG("gc_object called on an item \"" << describe_lua_value(L, 1) << "\" which is not an object.");
				lua_pushstring(L, "gc_object did not find an object to destroy, garbage collection failure");
				lua_error(L);
			}
			return 0;
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
			{"__gc", gc_callable},
			{nullptr, nullptr},
		};

		const luaL_Reg gObjectFunctions[] = {
			{"__index", get_object_index},
			{"__newindex", set_object_index},
			{"__tostring", serialize_object},
			{"__gc", gc_object},
			{nullptr, nullptr},
		};

		static int get_level(lua_State* L)
		{
			Level& lvl = Level::current();

			auto a = static_cast<FormulaCallablePtr*>(lua_newuserdata(L, sizeof(FormulaCallablePtr))); //(-0,+1,e)
			luaL_setmetatable(L, callable_str);		// (-0,+1,e)
			new (a) FormulaCallablePtr(&lvl);
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

		// Getter which takes a lua_State and recovers the pointer to its C++ owner from the lua EXTRA_SPACE region.
		// (The owner object stores its 'this' pointer there at construction time).
		LuaContext*& get_lua_context(lua_State* L) { return *reinterpret_cast<LuaContext**>(reinterpret_cast<char*>(L) - ANURA_OFFSET); }
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
			res = compiled->run(*this);
		}
		return res;
	}

	LuaContext::LuaContext()
		: state_(luaL_newstate())
	{
		init();
	}

	LuaContext::LuaContext(game_logic::FormulaObject& object)
		: state_(luaL_newstate())
	{
		init();
		setSelfObject(object);
	}

	LuaContext::~LuaContext()
	{
		lua_close(state_);
	}

	void LuaContext::init()
	{
		lua_State * L = getState();
		get_lua_context(L) = this;

		ASSERT_STACK_NEUTRAL(L);

		// load various Lua libraries
		load_libs(L);
		//luaL_openlibs(L);

		// load the debug traceback function to registry, to be used as error handler
		lua_pushstring(L, error_handler_fcn_reg_key);
		lua_getglobal(L, "debug");
		ASSERT_LOG(!lua_isnil(L, -1), "unexpected nil");
		lua_getfield(L, -1, "traceback");
		ASSERT_LOG(!lua_isnil(L, -1), "unexpected nil");
		lua_remove(L, -2);
		lua_rawset(L, LUA_REGISTRYINDEX);
		lua_pop(L, 1);

		luaL_newmetatable(L, function_str);
		luaL_setfuncs(L, gFFLFunctions, 0);
		lua_pushliteral(L, "__metatable");
		lua_pushliteral(L, "FFL function");
		lua_rawset(L, -3);
		lua_pop(L, 1);

		luaL_newmetatable(L, callable_str);
		luaL_setfuncs(L, gCallableFunctions, 0);
		lua_pushliteral(L, "__metatable");
		lua_pushliteral(L, "FFL callable");
		lua_rawset(L, -3);
		lua_pop(L, 1);

		luaL_newmetatable(L, object_str);
		luaL_setfuncs(L, gObjectFunctions, 0);
		lua_pushliteral(L, "__metatable");
		lua_pushliteral(L, "FFL object");
		lua_rawset(L, -3);
		lua_pop(L, 1);

		luaL_newmetatable(L, callable_function_str);
		luaL_setfuncs(L, gFFLCallableFunctions, 0);
		lua_pushliteral(L, "__metatable");
		lua_pushliteral(L, "FFL callable function");
		lua_rawset(L, -3);
		lua_pop(L, 1);

		luaL_newmetatable(L, lib_functions_str);
		luaL_setfuncs(L, gLibMetaFunctions, 0);
		lua_pushliteral(L, "__metatable");
		lua_pushliteral(L, "FFL library function");
		lua_rawset(L, -3);
		lua_pop(L, 1);

//		std::cerr << "pushing anura table\n";
		push_anura_table(L);

		lua_settop(L, 0);
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

	LuaCompiledPtr LuaContext::compile(const std::string& nam, const std::string& str)
	{
		lua_State * L = getState();

		ASSERT_STACK_NEUTRAL(L);

		std::string name = "@" + nam; // if the name does not begin "@" then lua will represent it as [string "name"] in the debugging output

		int err_code = luaL_loadbuffer(L, str.c_str(), str.size(), name.c_str());		// (-0,+1,-)
		if (err_code != LUA_OK) {
			handle_load_error(L, err_code);
			return nullptr;
		}

		LuaCompiled* chunk = new LuaCompiled();
#if LUA_VERSION_NUM == 502
		lua_dump(L, chunk_writer, reinterpret_cast<void*>(chunk));		// (-0,+0,-)
#elif LUA_VERSION_NUM == 503
		lua_dump(L, chunk_writer, reinterpret_cast<void*>(chunk), 0);		// (-0,+0,-)
#endif
		lua_pop(L, 1);													// (-n(1),+0,-)
		chunk->addChunk(0, 0);
		chunk->setName(name);
		return LuaCompiledPtr(chunk);
	}

	CompiledChunk* LuaContext::compileChunk(const std::string& nam, const std::string& str)
	{
		lua_State * L = getState();

		ASSERT_STACK_NEUTRAL(L);

		std::string name = "@" + nam; // if the name does not begin "@" then lua will represent it as [string "name"] in the debugging output

		int err_code = luaL_loadbuffer(L, str.c_str(), str.size(), name.c_str());		// (-0,+1,-)
		if (err_code != LUA_OK) {
			handle_load_error(L, err_code);
			return nullptr;
		}

		CompiledChunk* chunk = new CompiledChunk();
#if LUA_VERSION_NUM == 502
		lua_dump(L, chunk_writer, reinterpret_cast<void*>(chunk));		// (-0,+0,-)
#elif LUA_VERSION_NUM == 503
		lua_dump(L, chunk_writer, reinterpret_cast<void*>(chunk), 0);		// (-0,+0,-)
#endif
		lua_pop(L, 1);													// (-n(1),+0,-)
		chunk->addChunk(0, 0);
		chunk->setName(name);
		return chunk;
	}


	LuaCompiled::LuaCompiled()
	{
	}

	LuaCompiled::~LuaCompiled()
	{
	}

	void CompiledChunk::setName(const std::string & name) {
		chunk_name_ = name;
	}

	bool CompiledChunk::run(const LuaContext & L_) const
	{
		lua_State * L = L_.getState();

		ASSERT_STACK_NEUTRAL(L);

		// Load the error handler for pcall
		lua_pushstring(L, error_handler_fcn_reg_key);
		lua_rawget(L, LUA_REGISTRYINDEX);

		chunks_it_ = chunks_.begin();
		if(int err_code = lua_load(L, chunk_reader, reinterpret_cast<void*>(const_cast<CompiledChunk*>(this)), chunk_name_.c_str(), nullptr)) {
			handle_load_error(L, err_code);
			lua_pop(L, 1); // remove the error handler
			return true;
		}

		if (int err_code = lua_pcall(L, 0, 0, -2)) {
			handle_pcall_error(L, err_code);
			lua_pop(L, 1); // remove the error handler
			return true;
		}

		lua_pop(L,1); // remove error handler
		return false;
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(LuaCompiled)
	BEGIN_DEFINE_FN(execute, "(object) ->any")
		game_logic::FormulaCallable* callable = const_cast<game_logic::FormulaCallable*>(FN_ARG(0).as_callable());
		ASSERT_LOG(callable != nullptr, "Argument to LuaCompiled::execute was not a formula callable");
		game_logic::FormulaObject * object = dynamic_cast<game_logic::FormulaObject*>(callable);
		ASSERT_LOG(object != nullptr, "Argument to LuaCompiled::execute was not a formula object");
		boost::intrusive_ptr<lua::LuaContext> ctx = object->get_lua_context();
		ASSERT_LOG(ctx, "Argument to LuaCompiled::execute was not a formula object with a lua context. (Check class definition?)");
		obj.run(*ctx);
		//return lua_value_to_variant(ctx.getState());
		return variant();
	END_DEFINE_FN
	DEFINE_FIELD(dummy, "int")
		return variant(0);
	END_DEFINE_CALLABLE(LuaCompiled)

	LuaFunctionReference::LuaFunctionReference(lua_State* L, int ref)
		: ref_(ref), L_(get_lua_context(L)), target_return_type_()
	{
	}

	LuaFunctionReference::~LuaFunctionReference()
	{
		luaL_unref(L_->getState(), LUA_REGISTRYINDEX, ref_);
	}

	variant LuaFunctionReference::call() const
	{
		std::vector<variant> foo; //make an empty list variant
		return call(variant(&foo));
	}

	variant LuaFunctionReference::call(const variant & args) const
	{
		lua_State* L = L_->getState();

		ASSERT_STACK_NEUTRAL(L);

		int top = lua_gettop(L);

		// load error handler for pcall
		lua_pushstring(L, error_handler_fcn_reg_key);
		lua_rawget(L, LUA_REGISTRYINDEX);
		int error_handler_index = lua_gettop(L);

		// load the function itself from the registry
		lua_rawgeti(L, LUA_REGISTRYINDEX, ref_);

		// load the arguments to the function call
		auto args_vec = args.as_list();
		for (size_t i = 0; i < args_vec.size(); ++i) {
			variant_to_lua_value(L, args_vec[i]);
		}
		int nargs = args_vec.size();

		// call the function in a protected context
		int err_code = lua_pcall(L, nargs, LUA_MULTRET, error_handler_index);
		if(err_code != LUA_OK) {
			handle_pcall_error(L, err_code);
			lua_remove(L, error_handler_index);
			lua_settop(L, top);
			return variant();
		}
		// remove the error handler
		lua_remove(L, error_handler_index);

		// Count results based on what the top was when we entered this function
		int nresults = lua_gettop(L) - top;

		variant return_value;

		if (nresults > 1) {
			// Return results as a list.
			std::vector<variant> v;
			for(int n = 1; n <= nresults; ++n) {
				v.push_back(lua_value_to_variant(L, n + top));
			}
			return_value = variant(&v);
		} else if (nresults == 1) {
			// If there was only one return value, try to convert it to our target return type.
			// (We don't try to sort through what it means if there are multiple return values.)
			return_value = lua_value_to_variant(L, 1 + top, target_return_type_);
		}

		// Restore the top (popping the lua results we just converted to anura).
		lua_settop(L, top);
		return return_value;
	}

	void LuaFunctionReference::set_return_type(const variant_type_ptr & t) const {
		target_return_type_ = t;
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(LuaFunctionReference)
	BEGIN_DEFINE_FN(execute, "( [ any ] ) -> [any]")
		return obj.call(FN_ARG(0));
	END_DEFINE_FN
	BEGIN_DEFINE_FN(set_return_type, "(string) -> bool")
		obj.set_return_type(parse_variant_type(FN_ARG(0)));
		return variant(true);
	END_DEFINE_FN
	END_DEFINE_CALLABLE(LuaFunctionReference)

}

UNIT_TEST(lua_test)
{
	using namespace lua;
	LuaContextPtr ctx = new LuaContext();
}

/*UNIT_TEST(lua_callable_ffl_types) {
	CHECK_EQ (variant_types_compatible(LuaFunctionReference))
}*/

UNIT_TEST(lua_to_ffl_conversions) {
	using namespace game_logic;
	using namespace lua;

	LuaContextPtr test_context = new LuaContext();
	lua_State * L = test_context->getState();

	lua_settop(L, 0);

	{
		ASSERT_STACK_NEUTRAL(L);

		lua_pushstring(L, "foo");
		variant result = lua_value_to_variant(L, 1);
		CHECK_EQ (result, Formula(variant("\"foo\"")).execute());
		lua_pop(L, 1);
	}

	{
		ASSERT_STACK_NEUTRAL(L);

		lua_newtable(L);
		lua_pushstring(L, "foo");
		lua_setfield(L, -2, "a");
		lua_pushstring(L, "bar");
		lua_setfield(L, -2, "b");
		CHECK_EQ (lua_gettop(L), 1);
		variant result = lua_value_to_variant(L, 1);
		CHECK_EQ (result, Formula(variant("{ 'a' : 'foo', 'b' : 'bar' }")).execute());
		lua_pop(L, 1);
	}

	{
		ASSERT_STACK_NEUTRAL(L);

		lua_newtable(L);
		lua_pushinteger(L, 1);
		lua_setfield(L, -2, "a");
		lua_pushnumber(L, 1.5);
		lua_setfield(L, -2, "b");
		lua_pushboolean(L, false);
		lua_setfield(L, -2, "c");
		CHECK_EQ (lua_gettop(L), 1);
		variant result = lua_value_to_variant(L, 1);
		CHECK_EQ (result, Formula(variant("{ 'a' : 1, 'b' : 1.5, 'c': false }")).execute());
		lua_pop(L, 1);
	}

	{
		ASSERT_STACK_NEUTRAL(L);

		lua_newtable(L);
		lua_pushstring(L, "foo");
		lua_rawseti(L, -2, 1);
		lua_pushstring(L, "bar");
		lua_rawseti(L, -2, 2);
		CHECK_EQ (lua_gettop(L), 1);
		variant result = lua_value_to_variant(L, 1, parse_variant_type(variant("[string]")));
		CHECK_EQ (result, Formula(variant("['foo', 'bar']")).execute());
		result = lua_value_to_variant(L, 1, parse_variant_type(variant("{int -> string}")));
		CHECK_EQ (result, Formula(variant("{ 1 : 'foo', 2 : 'bar'}")).execute());
		lua_pop(L, 1);
	}

	{
		ASSERT_STACK_NEUTRAL(L);

		int err_code = luaL_loadstring(L, "return function(x) return x + 1 end");
		CHECK_EQ(err_code, LUA_OK);
		err_code = lua_pcall(L, 0, 1, 0);
		CHECK_EQ(err_code, LUA_OK);
		CHECK_EQ(lua_typename(L, lua_type(L, 1)), std::string("function"));

		variant func = lua_value_to_variant(L, 1);
		FormulaCallable* callable = const_cast<FormulaCallable*>(func.as_callable());
		CHECK_EQ(callable != nullptr, true);

		variant fn = callable->queryValue("execute");

		std::vector<variant> input;
		input.push_back(Formula(variant("[5]")).execute());

		variant output = fn(input);
		CHECK_EQ(output, Formula(variant("6")).execute());

		lua_pop(L,1);
	}

	{
		ASSERT_STACK_NEUTRAL(L);

		int err_code = luaL_loadstring(L, "return function(x) return {{x, x + 1}, {x * x, x * x * x}} end");
		CHECK_EQ(err_code, LUA_OK);
		err_code = lua_pcall(L, 0, 1, 0);
		CHECK_EQ(err_code, LUA_OK);
		CHECK_EQ(lua_typename(L, lua_type(L, 1)), std::string("function"));

		variant func = lua_value_to_variant(L, 1);
		FormulaCallable* callable = const_cast<FormulaCallable*>(func.as_callable());
		CHECK_EQ(callable != nullptr, true);

		variant exec_fn = callable->queryValue("execute");
		std::vector<variant> input;

		input.push_back(Formula(variant("[5]")).execute());
		CHECK_EQ(exec_fn(input), Formula(variant("{ 1: {1 : 5, 2 : 6}, 2: {1: 25, 2: 125}}")).execute());



		variant set_fn = callable->queryValue("set_return_type");
		input.back() = variant("[[int,int]]");
		CHECK_EQ(set_fn(input), variant(true));

		input.back() = Formula(variant("[5]")).execute();
		CHECK_EQ(exec_fn(input), Formula(variant("[[5,6], [25, 125]]")).execute());



		input.back() = variant("{ int -> [int,int] }");
		CHECK_EQ(set_fn(input), variant(true));

		input.back() = Formula(variant("[5]")).execute();
		CHECK_EQ(exec_fn(input), Formula(variant("{ 1: [5,6], 2: [25, 125] }")).execute());




		input.back() = variant("[{ int -> int }]");
		CHECK_EQ(set_fn(input), variant(true));

		input.back() = Formula(variant("[5]")).execute();
		CHECK_EQ(exec_fn(input), Formula(variant("[{ 1: 5, 2: 6}, {1 : 25, 2: 125} ]")).execute());


		lua_pop(L,1);
	}
}

UNIT_TEST(lua_persist) {
	using namespace lua;
//	lua::LuaContextPtr ctxt = new lua::LuaContext();
//	std::string p = ctxt->persist();
//	std::cerr << "lua_persist\n";

	lua_State * L = luaL_newstate();

	std::string persist_string;

	{
		ASSERT_STACK_NEUTRAL(L);

		{
			ASSERT_STACK_NEUTRAL(L);

			lua_pushboolean(L, true);
			eris_set_setting(L, "path", -1);
			lua_pop(L, 1);
		}

		load_libs(L);
	//	luaL_openlibs(L);

		//std::cerr << lua_gettop(L) << std::endl;
		lua_settop(L, 0);

		lua_pushstring(L, persist_key); 		// persist_key
		lua_rawget(L, LUA_REGISTRYINDEX);  		// persist

		//std::cerr << " *** PERSIST TABLE:\n";
		//std::cerr << full_dump(L, 0) << std::endl;
		//std::cerr << " ***\n";

		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			lua_newtable(L);
		}

		{
			int top = lua_gettop(L);

			lua_getglobal(L, "_G"); 		// persist _G
			//lua_newtable(L);			// persist target

			lua_pushinteger(L, 1);
			lua_setglobal(L, "a");
			//lua_pushstring(L, "a");
			//lua_settable(L, -3);			// persist target

			eris_persist(L, -2, -1);			// string (if succeeded)
			//std::cerr << "After persist" << std::endl;

			CHECK_EQ(lua_gettop(L), top + 2);
		}

		const char * str = lua_tostring(L, -1);
		ASSERT_LOG(str, "eris_persist returned a null string");
		persist_string = str;
		//std::cerr << "eris_persist gave a string: \n<<<\n" << str << "\n>>>\n";
		lua_pop(L, 3);

	}
	lua_close(L);
/*
	L = luaL_newstate();
	{
		ASSERT_STACK_NEUTRAL(L);

		{
			ASSERT_STACK_NEUTRAL(L);

			lua_pushboolean(L, true);
			eris_set_setting(L, "path", -1);
			lua_pop(L, 1);
		}

		load_libs(L);
	//	luaL_openlibs(L);

		lua_pushstring(L, unpersist_key); 		// unpersist_key
		lua_rawget(L, LUA_REGISTRYINDEX);  		// unpersist
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			lua_newtable(L);
		}

		std::cerr << " *** UNPERSIST TABLE:\n";
		std::cerr << full_dump(L, 0) << std::endl;
		std::cerr << " ***\n";

		lua_pushstring(L, persist_string.c_str());	// unpersist serial
		eris_unpersist(L, 1, 2);			// unpersist serial old_G

		CHECK_EQ(lua_gettop(L), 3);

		lua_getfield(L, -1, "a");			// unpersist serial old_G old_G.a
		ASSERT_LOG(lua_isnumber(L, -1), "expected a number found a " << lua_typename(L, lua_type(L, -1)));
		CHECK_EQ(lua_tointeger(L, -1), 1);
		lua_pop(L, 4);
	}
	lua_close(L);*/
}

UNIT_TEST(lua_with_ffl_objects) {
	using namespace game_logic;

	formula_class_unit_test_helper helper;
	helper.add_class_defn("foo", Formula(variant("\
{\
	properties: {\
		a : { type: \"string\", default: \"a\"},\
	},\
}")).execute());

	variant instance = Formula(variant("construct(\"foo\")")).execute();
	CHECK_EQ(instance.as_callable()->queryValue("a"), variant("a"));

	instance = Formula(variant("construct(\"foo\", {a: \"b\"})")).execute();
	CHECK_EQ(instance.as_callable()->queryValue("a"), variant("b"));

	variant instance_copy = FormulaObject::deepClone(instance);

	CHECK_EQ(instance, instance);
	CHECK_EQ(instance_copy != instance, true);

	CHECK_EQ(instance_copy.as_callable()->queryValue("a"), variant("b"));

	helper.add_class_defn("foo_lua", Formula(variant("\
{\
	properties: {\
		a : { type: \"string\", default: \"a\"},\
	},\
	lua: {\
		init: \"Anura.me.a = \'c\' \"\
	}\
}")).execute());

	variant i = Formula(variant("construct(\"foo_lua\")")).execute();
	CHECK_EQ(i.as_callable()->queryValue("a"), variant("c"));
}

/////////////////////////////////////////////////////////////////////
// Standard Lua utilities packaged as anura command line utilities //
/////////////////////////////////////////////////////////////////////

template<int alt_main(int, char * [])>
int ALTERNATE_MAIN(const std::string & prog_name, const std::vector<std::string> & args)
{
	char ** argv = (char**) malloc(sizeof(char*) * (args.size() + 2));

	argv[0] = (char *) malloc (sizeof(char) * (prog_name.size() + 1));
	strcpy(argv[0], prog_name.c_str());
	size_t idx = 1;
	for (const std::string & str : args) {
		argv[idx] = (char *) malloc(sizeof(char) * (str.size() + 1));
		strcpy(argv[idx], str.c_str());
		idx++;
	}
	argv[idx] = nullptr;

	std::stringstream info;
	info << "passing command-line arguments to utility \"" << prog_name << "\":\n";
	for (size_t i = 0; argv[i] != nullptr; ++i) {
		info << '[' << i << "] \"" << argv[i] << "\"\n";
	}
	LOG_INFO(info.str());

	int ret_code = alt_main(args.size() + 1, argv);

	for (size_t i = 0; argv[i] != nullptr; ++i) {
		free (argv[i]);
	}
	free (argv);

	return ret_code;
}

COMMAND_LINE_UTILITY(lua)
{
	ALTERNATE_MAIN<lua_standalone_interpreter_main>("lua", args);
}

COMMAND_LINE_UTILITY(luac)
{
	ALTERNATE_MAIN<lua_standalone_compiler_main>("luac", args);
}

COMMAND_LINE_UTILITY(test_persist)
{
	ALTERNATE_MAIN<lua_test_persist_main>("persist", args);
}

COMMAND_LINE_UTILITY(test_unpersist)
{
	ALTERNATE_MAIN<lua_test_unpersist_main>("unpersist", args);
}

#endif
