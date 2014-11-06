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
     in a product, an acknowledgement in the product documentation would be
     appreciated but is not required.

     2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.

     3. This notice may not be removed or altered from any source
     distribution.
*/
#ifndef IPHONE_CONTROLS_HPP_INCLUDED
#define IPHONE_CONTROLS_HPP_INCLUDED

#include "graphics.hpp"

class rect;

void translate_mouse_coords (int *x, int *y);

class iphone_controls
{
public:
	static bool up();
	static bool down();
	static bool left();
	static bool right();
	static bool attack();
	static bool jump();
	static bool tongue();
	static bool water_dir(float* x, float* y);

	static void set_underwater(bool value);
	static void set_can_interact(bool value);
	static void set_on_platform(bool value);
	static void set_standing(bool value);

	static void draw();

	static void read_controls();
#if defined(TARGET_OS_HARMATTAN) || defined(TARGET_BLACKBERRY) || defined(__ANDROID__)
	static void handle_event(const SDL_Event& event);
#endif

private:
	static bool hittest_button (const rect& r);
};

#endif
