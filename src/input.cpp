/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#include "WindowManager.hpp"

#include "input.hpp"

namespace input
{
	int sdl_poll_event(SDL_Event* event)
	{
		const int result = SDL_PollEvent(event);
		if(result) {
			switch(event->type) {
			case SDL_MOUSEMOTION: {
				int x = event->motion.x;
				int y = event->motion.y;
				KRE::WindowManager::getMainWindow()->mapMousePosition(&x, &y);
				event->motion.x = x;
				event->motion.y = y;
				break;
			}

			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEBUTTONDOWN: {
				int x = event->button.x;
				int y = event->button.y;
				KRE::WindowManager::getMainWindow()->mapMousePosition(&x, &y);
				event->button.x = x;
					event->button.y = y;
				break;
			}
		
			}
		}

		return result;
	}
	
	Uint32 sdl_get_mouse_state(int* x, int* y)
	{
		const Uint32 result = SDL_GetMouseState(x, y);
		KRE::WindowManager::getMainWindow()->mapMousePosition(x, y);
		return result;
	}
}
