#pragma once

#if defined(USE_LUA)

#include <memory>
#include <lua.hpp>

#include "formula_callable.hpp"

namespace lua
{
	class lua_context
	{
	public:
		lua_context();
		virtual ~lua_context();

		static lua_context& get_instance();

		std::shared_ptr<lua_State>& context() { return state_; }
		lua_State* context_ptr() { return state_.get(); }

		bool dostring(const char* str, game_logic::formula_callable* callable=NULL);
		bool dofile(const char* str, game_logic::formula_callable* callable=NULL);
	private:
		std::shared_ptr<lua_State> state_;
	};
}

#endif
