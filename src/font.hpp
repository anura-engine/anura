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
#ifndef FONT_HPP_INCLUDED
#define FONT_HPP_INCLUDED

#include <string>
#include <vector>

#include "graphics.hpp"
#include "texture.hpp"

namespace font {

bool is_init();

struct manager {
	manager();
	~manager();
};

struct error {
};

graphics::texture render_text(const std::string& text,
                              const SDL_Color& color, int size, const std::string& font_name="");
graphics::texture render_text_uncached(const std::string& text,
                                       const SDL_Color& color, int size, const std::string& font_name="");

int char_width(int size, const std::string& fn="");
int char_height(int size, const std::string& fn="");

void reload_font_paths();
std::vector<std::string> get_available_fonts();
std::string get_default_monospace_font();


}

#endif
