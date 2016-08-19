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

#include "asserts.hpp"
#include "hex_helper.hpp"

namespace hex
{
	std::tuple<int,int,int> oddq_to_cube_coords(const point& p)
	{
		int x1 = p.x;
		int z1 = p.y - (p.x - (p.x & 1)) / 2;
		int y1 = -(x1 + z1);
		return std::make_tuple(x1,y1,z1);
	}

	void oddq_to_cube_coords(const point& p, int* x1, int* y1, int* z1)
	{
		ASSERT_LOG(x1 != nullptr && y1 != nullptr && z1 != nullptr, "Parameter to oddq_to_cube_coords() was null.");
		*x1 = p.x;
		*z1 = p.y - (p.x - (p.x & 1)) / 2;
		*y1 = -(*x1 + *z1);
	}

	std::tuple<int,int,int> evenq_to_cube_coords(const point& p)
	{
		int x1 = p.x;
		int z1 = p.y - (p.x + (p.x & 1)) / 2;
		int y1 = -(x1 + z1);
		return std::make_tuple(x1,y1,z1);
	}

	void evenq_to_cube_coords(const point& p, int* x1, int* y1, int* z1)
	{
		ASSERT_LOG(x1 != nullptr && y1 != nullptr && z1 != nullptr, "Parameter to oddq_to_cube_coords() was null.");
		*x1 = p.x;
		*z1 = p.y - (p.x + (p.x & 1)) / 2;
		*y1 = -(*x1 + *z1);
	}

	int distance(int x1, int y1, int z1, int x2, int y2, int z2)
	{
		return (abs(x1 - x2) + abs(y1 - y2) + abs(z1 - z2)) / 2;
	}

	int distance(const point& p1, const point& p2)
	{
		int x1, y1, z1;
		std::tie(x1,y1,z1) = oddq_to_cube_coords(p1);
		int x2, y2, z2;
		std::tie(x2,y2,z2) = oddq_to_cube_coords(p2);
		return distance(x1, y1, z1, x2, y2, z2);
	}

	std::tuple<int,int,int> hex_round(float x, float y, float z) 
	{
		int rx = static_cast<int>(std::round(x));
		int ry = static_cast<int>(std::round(y));
		int rz = static_cast<int>(std::round(z));

		float x_diff = std::abs(rx - x);
		float y_diff = std::abs(ry - y);
		float z_diff = std::abs(rz - z);

		if(x_diff > y_diff && x_diff > z_diff) {
			rx = -(ry+rz);
		} else if(y_diff > z_diff) {
			ry = -(rx+rz);
		} else {
			rz = -(rx+ry);
		}
		return std::make_tuple(rx, ry, rz);
	}

	point cube_to_oddq_coords(const std::tuple<int,int,int>& xyz)
	{
		return point(std::get<0>(xyz), std::get<2>(xyz) + (std::get<0>(xyz) - (std::get<0>(xyz) & 1)) / 2);
	}

	point cube_to_oddq_coords(int x1, int y1, int z1)
	{
		return point(x1, z1 + (x1 - (x1 & 1)) / 2);
	}

	point cube_to_evenq_coords(const std::tuple<int,int,int>& xyz)
	{
		return point(std::get<0>(xyz), std::get<2>(xyz) + (std::get<0>(xyz) + (std::get<0>(xyz) & 1)) / 2);
	}

	point cube_to_evenq_coords(int x1, int y1, int z1)
	{
		return point(x1, z1 + (x1 + (x1 & 1)) / 2);
	}

	std::vector<point> line(const point& p1, const point& p2)
	{
		std::vector<point> res;
		int n = distance(p1, p2);
		int x1, y1, z1;
		std::tie(x1,y1,z1) = oddq_to_cube_coords(p1);
		int x2, y2, z2;
		std::tie(x2,y2,z2) = oddq_to_cube_coords(p2);
		for(int i = 0; i <= n; ++i) {
			const float i_over_n  = static_cast<float>(i)/n;
			const float xt = x1 * (1.0f - i_over_n) + x2 * i_over_n + 1e-6f;
			const float yt = y1 * (1.0f - i_over_n) + y2 * i_over_n + 1e-6f;
			const float zt = z1 * (1.0f - i_over_n) + z2 * i_over_n - 2e-6f;
			res.emplace_back(cube_to_oddq_coords(hex_round(xt, yt, zt)));
		}

		return res;
	}

	float rotation_between(const point& p1, const point& p2)
	{
		// hack it somewhat to just work for p1 and p2 being adjacent.
		const int dx = p2.x - p1.x;
		const int dy = p2.y - p1.y;
		ASSERT_LOG(dx <= 1 && dx >= -1 && dy >= -1 && dy <= 1, "hex::logical::rotation_between only works for adjacent tiles.");
		if(dx == 0 && dy == 0) {
			return 0.0f;
		}
		if(dy == 0) {
			return dx > 0 ? 60.0f : 300.0f;
		} else if(dy > 0) {
			return dx == 0 ? 180.0f : dx > 0 ? 120.0f : dx == 240.0f;
		} else {
			// dy < 0
			return 0.0f;
		}
	}

	point get_pixel_pos_from_tile_pos_oddq(const point& p, int HexTileSize)
	{
		return get_pixel_pos_from_tile_pos_oddq(p.x, p.y, HexTileSize);
	}

	point get_pixel_pos_from_tile_pos_oddq(int x, int y, int HexTileSize)
	{
		const int HexTileSizeHalf = HexTileSize/2;
		const int HexTileSizeThreeQuarters = (HexTileSize*3)/4;
		const int tx = x*HexTileSizeThreeQuarters;
		const int ty = HexTileSize*y + (abs(x)%2)*HexTileSizeHalf;
		return point(tx, ty);
	}

	point get_pixel_pos_from_tile_pos_evenq(const point& p, int HexTileSize)
	{
		return get_pixel_pos_from_tile_pos_evenq(p.x, p.y, HexTileSize);
	}

	point get_pixel_pos_from_tile_pos_evenq(int x, int y, int HexTileSize)
	{
		const int HexTileSizeHalf = HexTileSize/2;
		const int HexTileSizeThreeQuarters = (HexTileSize*3)/4;
		const int tx = x*HexTileSizeThreeQuarters;
		const int ty = HexTileSize*y - (abs(x)%2)*HexTileSizeHalf;
		return point(tx, ty);
	}
}
