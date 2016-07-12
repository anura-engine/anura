/*
Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>

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

#include <memory>

#include "SDL.h"

#include "geometry.hpp"

class event_listener
{
public:
	event_listener();
	virtual ~event_listener();
	bool mouse_motion(bool claimed, const point& p, unsigned keymod);
	bool mouse_button_up(bool claimed, const point& p, unsigned buttons, unsigned keymod);
	bool mouse_button_down(bool claimed, const point& p, unsigned buttons, unsigned keymod);
	bool mouse_wheel(bool claimed, const point& p, const point& delta, int direction);
private:
	virtual bool handle_mouse_motion(bool claimed, const point& p, unsigned keymod) = 0;
	virtual bool handle_mouse_button_up(bool claimed, const point& p, unsigned buttons, unsigned keymod) = 0;
	virtual bool handle_mouse_button_down(bool claimed, const point& p, unsigned buttons, unsigned keymod) = 0;	
	virtual bool handle_mouse_wheel(bool claimed, const point& p, const point& delta, int direction) = 0;
};

typedef std::shared_ptr<event_listener> event_listener_ptr;
