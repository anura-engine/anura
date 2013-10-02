#ifndef INPUT_HPP_INCLUDED
#define INPUT_HPP_INCLUDED

#include "SDL.h"

namespace input
{
	int sdl_poll_event(SDL_Event* event);
	Uint32 sdl_get_mouse_state(int* x, int* y);
}

#endif
