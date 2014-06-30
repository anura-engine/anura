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
#ifndef LEVEL_SOLID_MAP_HPP_INCLUDED
#define LEVEL_SOLID_MAP_HPP_INCLUDED

#include <boost/dynamic_bitset.hpp>
#include <map>
#include <vector>

#ifndef MAX_TILE_SIZE
#define MAX_TILE_SIZE 64
#endif

extern int g_tile_scale;
extern int g_tile_size;

#define TileSize (g_tile_size*g_tile_scale)

typedef std::pair<int,int> tile_pos;
typedef boost::dynamic_bitset<> tile_bitmap;

struct surface_info {
	surface_info() : friction(0), traction(0), damage(-1), info(0)
	{}
	int friction, traction, damage;
	const std::string* info;
	static const std::string* get_info_str(const std::string& key);
};

struct tile_solid_info {
	tile_solid_info() : bitmap(TileSize*TileSize), all_solid(false)
	{}
	tile_bitmap bitmap;
	surface_info info;
	bool all_solid;
};

class level_solid_map {
public:
	level_solid_map();
	level_solid_map(const level_solid_map& m);
	level_solid_map& operator=(const level_solid_map& m);
	~level_solid_map();
	tile_solid_info& insert_or_find(const tile_pos& pos);
	const tile_solid_info* find(const tile_pos& pos) const;
	void erase(const tile_pos& pos);
	void clear();

	void merge(const level_solid_map& m, int xoffset, int yoffset);
private:

	tile_solid_info** insert_raw(const tile_pos& pos);

	struct row {
		std::vector<tile_solid_info*> positive_cells, negative_cells;
	};

	std::vector<row> positive_rows_, negative_rows_;
};

#endif
