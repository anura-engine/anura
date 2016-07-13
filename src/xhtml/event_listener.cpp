/*
Copyright (C) 2016 by Kristina Simpson <sweet.kristas@gmail.com>

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

#include "event_listener.hpp"

event_listener::event_listener()
{
}

event_listener::~event_listener()
{
}

bool event_listener::mouse_motion(bool claimed, const point& p, unsigned keymod)
{
	return handle_mouse_motion(claimed, p, keymod);
}

bool event_listener::mouse_button_up(bool claimed, const point& p, unsigned buttons, unsigned keymod)
{
	return handle_mouse_button_up(claimed, p, buttons, keymod);
}

bool event_listener::mouse_button_down(bool claimed, const point& p, unsigned buttons, unsigned keymod)
{
	return handle_mouse_button_down(claimed, p, buttons, keymod);
}

bool event_listener::mouse_wheel(bool claimed, const point& p, const point& delta, int direction)
{
	return handle_mouse_wheel(claimed, p, delta, direction);
}

