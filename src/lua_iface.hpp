/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

  This software is provided 'as-is', without any express or implied
  warranty. In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

     1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgement in the product documentation would be
     appreciated but is not required.

     2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.

     3. This notice may not be removed or altered from any source
     distribution.
*/

#pragma once

#if defined(USE_LUA)

#include <memory>
#include <lua.hpp>

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

namespace lua
{
	class compiled_chunk
	{
	public:
		compiled_chunk() {}
		virtual ~compiled_chunk() {}

		void reset_chunks() { chunks_.clear(); chunks_it_ = chunks_.begin(); }
		void add_chunk(const void* p, size_t sz) {
			const char* pp = static_cast<const char*>(p);
			chunks_.push_back(std::vector<char>(pp,pp+sz));
		}
		bool run(lua_State* L) const;
		const std::vector<char>& current() const { return *chunks_it_; }
		void next() { ++chunks_it_; }
	private:
		typedef std::vector<std::vector<char>> chunk_list_type;
		chunk_list_type chunks_;
		mutable chunk_list_type::const_iterator chunks_it_;
	};

	class lua_compiled : public game_logic::formula_callable, public compiled_chunk
	{
	public:
		lua_compiled();
		virtual ~lua_compiled();
	private:
		DECLARE_CALLABLE(lua_compiled);
	};
	typedef boost::intrusive_ptr<lua_compiled> lua_compiled_ptr;

	class lua_function_reference : public game_logic::formula_callable
	{
	public:
		lua_function_reference(lua_State* L, int ref);
		virtual ~lua_function_reference();

		virtual variant get_value(const std::string& key) const;
		virtual variant call();
	private:
		lua_State* L_; 
		int ref_;

		lua_function_reference();
		lua_function_reference(const lua_function_reference&);
	};

	typedef boost::intrusive_ptr<lua_function_reference> lua_function_reference_ptr;

	class lua_context
	{
	public:
		lua_context();
		explicit lua_context(game_logic::formula_callable& callable);
		virtual ~lua_context();

		static lua_context& get_instance();

		std::shared_ptr<lua_State>& context() { return state_; }
		lua_State* context_ptr() { return state_.get(); }

		void set_self_callable(game_logic::formula_callable& callable);

		bool execute(const variant& value, game_logic::formula_callable* callable=NULL);

		bool dostring(const std::string&name, const std::string& str, game_logic::formula_callable* callable=NULL);
		bool dofile(const std::string&name, const std::string& str, game_logic::formula_callable* callable=NULL);

		lua_compiled_ptr compile(const std::string& name, const std::string& str);

		compiled_chunk* compile_chunk(const std::string& name, const std::string& str);
	private:
		void init();

		std::shared_ptr<lua_State> state_;

		lua_context(const lua_context&);
	};
}

#endif
