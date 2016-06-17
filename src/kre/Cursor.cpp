/*
	Copyright (C) 2013-2016 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "Cursor.hpp"
#include "Surface.hpp"
#include "SDL.h"

namespace KRE
{
	namespace 
	{
		static bool g_init = false;

		struct CursorImpl : public Cursor
		{
			explicit CursorImpl(SDL_Cursor* p) : cursor_(p) {}
			~CursorImpl() {};
			void setCursor() override { SDL_SetCursor(cursor_); }
			SDL_Cursor* cursor_;	
			CursorImpl() = delete;
		};

		typedef std::map<std::string, CursorPtr> cursor_map;
		cursor_map& get_cursors()
		{
			static cursor_map res;
			if(res.empty()) {
#define DEFINE_CURSOR(s)				\
	do{									\
		auto c = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_##s); \
		res[#s] = std::unique_ptr<CursorImpl>(new CursorImpl(c)); \
	} while(0)
				DEFINE_CURSOR(ARROW);
				DEFINE_CURSOR(IBEAM);
				DEFINE_CURSOR(WAIT);
				DEFINE_CURSOR(CROSSHAIR);
				DEFINE_CURSOR(WAITARROW);
				DEFINE_CURSOR(SIZENWSE);
				DEFINE_CURSOR(SIZENESW);
				DEFINE_CURSOR(SIZEWE);
				DEFINE_CURSOR(SIZENS);
				DEFINE_CURSOR(SIZEALL);
				DEFINE_CURSOR(NO);
				DEFINE_CURSOR(HAND);
#undef DEFINE_CURSOR
			}
			return res;
		}
	}

	bool are_cursors_initialized()
	{
		return g_init;
	}

	bool initialize_cursors(const variant& node)
	{
		if(g_init == true) {
			get_cursors().clear();
		}
		auto& cursors = get_cursors();
		for(const auto& p : node.as_map()) {
			const std::string name = p.first.as_string();
			const std::string img = p.second["image"].as_string();
			const int hot_x = p.second["hot_x"].as_int();
			const int hot_y = p.second["hot_y"].as_int();

			auto surf = Surface::create(img);
			ASSERT_LOG(surf != nullptr, "Unable to create image surface");
			cursors[name] = surf->createCursorFromSurface(hot_x, hot_y);
		}
		g_init = true;
		return g_init;
	}

	void set_cursor(const std::string& name)
	{
		auto& cursors = get_cursors();
		auto it = cursors.find(name);
		ASSERT_LOG(it != cursors.end(), "Unable to find cursor on list.");
		it->second->setCursor();
	}
}

