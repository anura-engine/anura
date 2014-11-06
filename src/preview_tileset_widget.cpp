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
#include <iostream>

#include "graphics.hpp"
#include "foreach.hpp"
#include "preview_tileset_widget.hpp"
#include "tile_map.hpp"

namespace gui {

preview_tileset_widget::preview_tileset_widget(const tile_map& tiles)
  : width_(0), height_(0)
{
	set_environment();
	tiles.build_tiles(&tiles_);
	init();
}

preview_tileset_widget::preview_tileset_widget(const variant& v, game_logic::formula_callable* e)
	: widget(v,e)
{
	tile_map(v["tile_map"]).build_tiles(&tiles_);
	init();
}

void preview_tileset_widget::init()
{
	foreach(const level_tile& t, tiles_) {
		const int w = t.x + t.object->width();
		const int h = t.y + t.object->height();

		width_ = std::max(width_, w);
		height_ = std::max(height_, h);
	}

	set_dim(width_, height_);
}

void preview_tileset_widget::handle_draw() const
{
	if(width_ == 0 || height_ == 0) {
		return;
	}

	const GLfloat scale = std::min(GLfloat(width())/width_, GLfloat(height())/height_);
	glPushMatrix();
	glTranslatef(GLfloat(x()), GLfloat(y()), 0);
	glScalef(scale, scale, 0.0);
	foreach(const level_tile& t, tiles_) {
		graphics::blit_queue q;
		level_object::queue_draw(q, t);
		q.do_blit();
	}
	glPopMatrix();
}

void preview_tileset_widget::set_value(const std::string& key, const variant& v)
{
	if(key == "tile_map") {
		tile_map(v).build_tiles(&tiles_);
		init();
	}
	widget::set_value(key, v);
}

variant preview_tileset_widget::get_value(const std::string& key) const
{
	return widget::get_value(key);
}

}
