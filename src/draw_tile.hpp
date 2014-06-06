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
#ifndef DRAW_TILE_HPP_INCLUDED
#define DRAW_TILE_HPP_INCLUDED

#include "kre/Geometry.hpp"
#include "kre/Material.hpp"

#include "color_utils.hpp"

namespace graphics {
class blit_queue;
}

struct level_tile;
struct hex_level_tile;

class tile_corner
{
public:
	short vertex[2];
	float uv[2];
};

void queue_draw_tile(graphics::blit_queue& q, const level_tile& t);
int get_tile_corners(tile_corner* result, const KRE::MaterialPtr& t, const rect& area, int tile_num, int x, int y, bool reverse);
void queue_draw_from_tilesheet(graphics::blit_queue& q, const KRE::MaterialPtr& t, const rect& area, int tile_num, int x, int y, bool reverse);

bool is_tile_opaque(const KRE::MaterialPtr& t, int tile_num);
bool is_tile_using_alpha_channel(const KRE::MaterialPtr& t, int tile_num);
bool is_tile_solid_color(const KRE::MaterialPtr& t, int tile_num, graphics::color& col);

rect get_tile_non_alpha_area(const KRE::MaterialPtr& t, int tile_num);

#endif
