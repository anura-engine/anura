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

#include <cmath>

#include "asserts.hpp"
#include "hex_helper.hpp"
#include "unit_test.hpp"

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
		// in an even-q layout the 0,0 tile is now no longer has a top-left pixel position of 0,0
		// so we move down half a tile to compensate.
		return point(tx, ty + HexTileSizeHalf);
	}

	template<typename T>
	struct Hex
	{
		Hex(T xo, T yo, T zo) : x(xo), y(yo), z(zo) {}
		T x, y, z;
	};

	struct Axial
	{
		Axial(int qo, int ro) : q(qo), r(ro) { s = -q - r; }
		point to_point() const { return point(q, r); }
		int q, r, s;
	};

	Axial cube_to_axial(const Hex<int>& h)
	{
		return Axial(h.x, h.z);
	}

	Hex<int> cube_round(const Hex<float>& h)
	{
		int rx = static_cast<int>(std::round(h.x));
		int ry = static_cast<int>(std::round(h.y));
		int rz = static_cast<int>(std::round(h.z));

		float x_diff = std::abs(rx - h.x);
		float y_diff = std::abs(ry - h.y);
		float z_diff = std::abs(rz - h.z);

		if(x_diff > y_diff && x_diff > z_diff) {
			rx = -ry - rz;
		} else if(y_diff > z_diff) {
			ry = -rx - rz;
		} else {
			rz = -rx - ry;
		}

		return Hex<int>(rx, ry, rz);
	}

	point cube_to_evenq(const Hex<int>& h)
	{
		return point(h.x, h.z + (h.x + (h.x&1)) / 2);
	}
	
	point get_tile_pos_from_pixel_pos_evenq(const point& np, int HexTileSize)
	{
		// in an even-q layout the 0,0 tile is now no longer has a top-left pixel position of 0,0
		// so we move up half a tile to compensate.
		point p{ np.x, np.y - HexTileSize / 2 };
		const int tesselation_x_size = (3 * HexTileSize) / 2;
		const int tesselation_y_size = HexTileSize;
		const int x_base = p.x >= 0 ? p.x / tesselation_x_size * 2 : p.x / tesselation_x_size * 2 - 2;
		const int x_mod  = p.x >= 0 ? p.x % tesselation_x_size : tesselation_x_size + (p.x % tesselation_x_size);
		const int y_base = p.y >= 0 ? p.y / tesselation_y_size : p.y / tesselation_y_size - 1;
		const int y_mod  = p.y >= 0 ? p.y % tesselation_y_size : tesselation_y_size + (p.y % tesselation_y_size);
		const int m = 2;

		int x_modifier = 0;
		int y_modifier = 0;

		if(y_mod < tesselation_y_size / 2) {
			if((x_mod * m + y_mod) < (HexTileSize / 2)) {
				x_modifier = -1;
				y_modifier = 0;
			} else if ((x_mod * m - y_mod) < (HexTileSize * 3 / 2)) {
				x_modifier = 0;
				y_modifier = 0;
			} else {
				x_modifier = 1;
				y_modifier = 0;
			}

		} else {
			if((x_mod * m - (y_mod - HexTileSize / 2)) < 0) {
				x_modifier = -1;
				y_modifier = 1;
			} else if((x_mod * m + (y_mod - HexTileSize / 2)) < HexTileSize * 2) {
				x_modifier = 0;
				y_modifier = 0;
			} else {
				x_modifier = 1;
				y_modifier = 1;
			}
		}
		return point(x_base + x_modifier, y_base + y_modifier);
	}
}

UNIT_TEST(hexes)
{
	//CHECK_EQ(hex::get_pixel_pos_from_tile_pos_evenq(0, 0, 72), point(0, 0));
	//CHECK_EQ(hex::get_pixel_pos_from_tile_pos_evenq(1, 0, 72), point(54, -36));
	//CHECK_EQ(hex::get_pixel_pos_from_tile_pos_evenq(0, 1, 72), point(0, 72));
	//CHECK_EQ(hex::get_pixel_pos_from_tile_pos_evenq(1, 1, 72), point(54, 36));

	CHECK_EQ(hex::get_tile_pos_from_pixel_pos_evenq(point(-54, 36), 72), point(-1, 0));
	CHECK_EQ(hex::get_tile_pos_from_pixel_pos_evenq(point(0, 36), 72), point(-1, 0));

	CHECK_EQ(hex::get_tile_pos_from_pixel_pos_evenq(point(18, 36), 72), point(0, 0));
	CHECK_EQ(hex::get_tile_pos_from_pixel_pos_evenq(point(36, 36), 72), point(0, 0));
	CHECK_EQ(hex::get_tile_pos_from_pixel_pos_evenq(point(53, 36), 72), point(0, 0));

	CHECK_EQ(hex::get_tile_pos_from_pixel_pos_evenq(point(54, 36), 72), point(1, 0));

	CHECK_EQ(hex::get_tile_pos_from_pixel_pos_evenq(point(72, 72), 72), point(1, 1));

	CHECK_EQ(hex::get_tile_pos_from_pixel_pos_evenq(point(-18, 72), 72), point(-1, 1));
	CHECK_EQ(hex::get_tile_pos_from_pixel_pos_evenq(point(0, 108), 72), point(-1, 1));
	CHECK_EQ(hex::get_tile_pos_from_pixel_pos_evenq(point(3, 99), 72), point(-1, 1));


	//CHECK_EQ(hex::get_tile_pos_from_pixel_pos_evenq(point(36, 36), 72), point(0, 0));
	//CHECK_EQ(hex::get_tile_pos_from_pixel_pos_evenq(point(36, 108), 72), point(0, 1));
	//CHECK_EQ(hex::get_tile_pos_from_pixel_pos_evenq(point(54, -36), 72), point(1, 0));
	//CHECK_EQ(hex::get_tile_pos_from_pixel_pos_evenq(point(90, 0), 72), point(1, 0));
}
