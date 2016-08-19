/*
	Copyright (C) 2013-2016 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#pragma once

#include <tuple>

#include "geometry.hpp"

namespace hex
{
	std::tuple<int, int, int> oddq_to_cube_coords(const point& p);
	std::tuple<int, int, int> evenq_to_cube_coords(const point& p);
	void oddq_to_cube_coords(const point& p, int* x1, int* y1, int* z1);
	void evenq_to_cube_coords(const point& p, int* x1, int* y1, int* z1);
	int distance(int x1, int y1, int z1, int x2, int y2, int z2);
	int distance(const point& p1, const point& p2);
	std::tuple<int,int,int> hex_round(float x, float y, float z);
	point cube_to_oddq_coords(const std::tuple<int, int, int>& xyz);
	point cube_to_oddq_coords(int x1, int y1, int z1);
	point cube_to_evenq_coords(const std::tuple<int, int, int>& xyz);
	point cube_to_evenq_coords(int x1, int y1, int z1);
	std::vector<point> line(const point& p1, const point& p2);
	float rotation_between(const point& p1, const point& p2);
	point get_pixel_pos_from_tile_pos_oddq(const point& p, int HexTileSize);
	point get_pixel_pos_from_tile_pos_oddq(int x, int y, int HexTileSize);
	point get_pixel_pos_from_tile_pos_evenq(const point& p, int HexTileSize);
	point get_pixel_pos_from_tile_pos_evenq(int x, int y, int HexTileSize);
}

