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

#pragma once

#if defined(USE_LUA)

#include <memory>
#include "eris/lua.h"

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

namespace game_logic { class FormulaObject; }

namespace lua
{
	class LuaContext;

	class CompiledChunk
	{
	public:
		CompiledChunk() {}
		virtual ~CompiledChunk() {}

		void resetChunks() { chunks_.clear(); chunks_it_ = chunks_.begin(); }
		void addChunk(const void* p, size_t sz) {
			const char* pp = static_cast<const char*>(p);
			chunks_.push_back(std::vector<char>(pp,pp+sz));
		}
		bool run(const LuaContext &) const;
		const std::vector<char>& current() const { return *chunks_it_; }
		void next() { ++chunks_it_; }
		void setName(const std::string &);
	private:
		typedef std::vector<std::vector<char>> chunk_list_type;
		chunk_list_type chunks_;
		mutable chunk_list_type::const_iterator chunks_it_;

		std::string chunk_name_;
	};

	class LuaCompiled : public game_logic::FormulaCallable, public CompiledChunk
	{
	public:
		LuaCompiled();
		virtual ~LuaCompiled();
	private:
		DECLARE_CALLABLE(LuaCompiled);
	};
	typedef ffl::IntrusivePtr<LuaCompiled> LuaCompiledPtr;

	class LuaContext : public reference_counted_object
	{
	public:
		LuaContext();
		explicit LuaContext(game_logic::FormulaObject&);
		virtual ~LuaContext();

		lua_State* getState() const { return state_; }

		void setSelfObject(game_logic::FormulaObject&);
		void setSelfCallable(game_logic::FormulaCallable& callable);

		bool execute(const variant& value, game_logic::FormulaCallable* callable=nullptr);

		bool dostring(const std::string&name, const std::string& str, game_logic::FormulaCallable* callable=nullptr);
		bool dofile(const std::string&name, const std::string& str, game_logic::FormulaCallable* callable=nullptr);

		LuaCompiledPtr compile(const std::string& name, const std::string& str);

		CompiledChunk* compileChunk(const std::string& name, const std::string& str);

		std::string persist();
	private:
		void init();

		lua_State * state_;

		LuaContext(const LuaContext&);

	};

	typedef ffl::IntrusivePtr<LuaContext> LuaContextPtr;

	class LuaFunctionReference : public game_logic::FormulaCallable
	{
	public:
		LuaFunctionReference(lua_State* L, int ref);
		virtual ~LuaFunctionReference();

		virtual variant call() const;
		virtual variant call(const variant & args) const;
		void set_return_type(const variant_type_ptr &) const;
	private:
		LuaContextPtr L_;
		int ref_;
		mutable variant_type_ptr target_return_type_;

		DECLARE_CALLABLE(LuaFunctionReference);

		LuaFunctionReference();
		LuaFunctionReference(const LuaFunctionReference&);
	};

	typedef ffl::IntrusivePtr<LuaFunctionReference> LuaFunctionReferencePtr;
}

#endif
