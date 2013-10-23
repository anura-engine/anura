#pragma once

#if defined(USE_LUA)

#include <memory>
#include <lua.hpp>

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

		bool dostring(const char* str);
		bool dofile(const char* str);
	private:
		std::shared_ptr<lua_State> state_;
	};
}

#endif