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
