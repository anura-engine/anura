/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
	setEnvironment();
	tiles.buildTiles(&tiles_);
	init();
}

preview_tileset_widget::preview_tileset_widget(const variant& v, game_logic::FormulaCallable* e)
	: widget(v,e)
{
	tile_map(v["tile_map"]).buildTiles(&tiles_);
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

	setDim(width_, height_);
}

void preview_tileset_widget::handleDraw() const
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

void preview_tileset_widget::setValue(const std::string& key, const variant& v)
{
	if(key == "tile_map") {
		tile_map(v).buildTiles(&tiles_);
		init();
	}
	widget::setValue(key, v);
}

variant preview_tileset_widget::getValue(const std::string& key) const
{
	return widget::getValue(key);
}

}
