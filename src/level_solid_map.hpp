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
