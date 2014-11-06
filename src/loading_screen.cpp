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
#include <string>
#include <iostream>

#include "font.hpp"
#include "foreach.hpp"
#include "surface_cache.hpp"
#include "loading_screen.hpp"
#include "custom_object_type.hpp"
#include "raster.hpp"
#include "graphical_font.hpp"
#include "geometry.hpp"
#include "i18n.hpp"
#include "module.hpp"
#include "texture.hpp"
#include "variant.hpp"

loading_screen::loading_screen (int items) : items_(items), status_(0),
                                             started_at_(SDL_GetTicks())
{
	try {
		background_ = graphics::texture::get("backgrounds/loading_screen.png");

		if(graphics::screen_height() > 0 && float(graphics::screen_width())/float(graphics::screen_height()) <= 1.4) {
			splash_ = graphics::texture::get("splash.jpg");
		} else {
			splash_ = graphics::texture::get("splash-wide.jpg");
		}
	} catch(graphics::load_image_error& e) {
	}
}

void loading_screen::load(variant node)
{
	//custom_object_type::get("frogatto_playable");
	foreach(variant preload_node, node["preload"].as_list())
	{
		draw_and_increment(preload_node["message"].as_string());
		if(preload_node["type"].as_string() == "object")
		{
			custom_object_type::get(preload_node["name"].as_string());
		} else if(preload_node["type"].as_string() == "texture") {
			graphics::texture::get(preload_node["name"].as_string());
		}
	}
}

void loading_screen::draw (const std::string& message)
{
	//std::cerr << "*** Drawing loading screen with message: " << message << "\n";
	
	get_main_window()->prepare_raster();
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	if(splash_.valid()) {
		//draw the splash screen while loading
		graphics::blit_texture(splash_, 0, 0, graphics::screen_width(), graphics::screen_height());
	} else {
		draw_internal(message);
	}
	
	get_main_window()->swap();
	//SDL_Delay(500); //make it possible to see on fast computers; for debugging
}

void loading_screen::draw_internal (const std::string& message)
{
	const int bar_width = 100;
	const int bar_height = 10;
	int screen_w = graphics::screen_width();
	int screen_h = graphics::screen_height();

	int bg_w = background_.width();
	int bg_h = background_.height();
	graphics::blit_texture(background_, screen_w/2-bg_w, std::max(screen_h/2-bg_h, 0), bg_w*2, bg_h*2);
	
	int bar_origin_x = graphics::screen_width()/2 - bar_width/2;
	rect bg(screen_w/2 - bar_width/2, screen_h/2 - bar_height/2, bar_width, bar_height);
	graphics::draw_rect(bg, graphics::color(96, 96, 96, 255));
	float amount_done = (float)status_ / (float)items_;
	rect bar(screen_w/2 - bar_width/2, screen_h/2 - bar_height/2, bar_width*amount_done, bar_height);
	graphics::draw_rect(bar, graphics::color(255, 255, 255, 255));
	
	std::string font = module::get_default_font();
	if(font == "bitmap") {
		const_graphical_font_ptr font = graphical_font::get("door_label");
		// explicitly translate loading messages
		if(font) {
			rect text_size = font->dimensions(i18n::tr(message));
			font->draw(screen_w/2 - text_size.w()/2, screen_h/2 + bar_height/2 + 5, i18n::tr(message));
		}
	} else {
		// todo: we need to load this information (x,y offsets, colors, sizes from a customisation file)
		SDL_Color color = {255,255,255,255};
		int size = 18;
		graphics::texture tex = font::render_text_uncached(i18n::tr(message), color, size, font);
		graphics::blit_texture(tex, screen_w/2 - tex.width()/2, screen_h/2 - tex.height()/2 + bar_height + 10);
	}
}

void loading_screen::increment_status ()
{
	status_++;
}

void loading_screen::set_number_of_items (int items)
{
	items_ = items;
}

void loading_screen::finish_loading()
{
	//display the splash screen for a minimum amount of time, if there is one.
	if(!splash_.valid()) {
		return;
	}

	while(started_at_ + 3000 > SDL_GetTicks()) {
		draw_and_increment("Loading");
		SDL_Delay(20);
	}
}
