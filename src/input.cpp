#include "input.hpp"
#include "raster.hpp"

namespace input
{
int sdl_poll_event(SDL_Event* event)
{
	const int result = SDL_PollEvent(event);
	switch(event->type) {
	case SDL_MOUSEMOTION: {
		int x = event->motion.x;
		int y = event->motion.y;
		graphics::map_mouse_position(&x, &y);
		event->motion.x = x;
		event->motion.y = y;
		break;
	}

	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEBUTTONDOWN: {
		int x = event->button.x;
		int y = event->button.y;
		graphics::map_mouse_position(&x, &y);
		event->button.x = x;
		event->button.y = y;
		break;
	}
		
	}

	return result;
}
	
Uint32 sdl_get_mouse_state(int* x, int* y)
{
	const Uint32 result = SDL_GetMouseState(x, y);
	graphics::map_mouse_position(x, y);
	return result;
}

}
