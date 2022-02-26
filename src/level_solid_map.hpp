/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>

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

struct SurfaceInfo
{
	SurfaceInfo() : friction(0), traction(0), damage(-1), info(0)
	{}
	int friction, traction, damage;
	const std::string* info;
	static const std::string* get_info_str(const std::string& key);
};

struct TileSolidInfo
{
	TileSolidInfo() : bitmap(TileSize*TileSize), all_solid(false)
	{}
	tile_bitmap bitmap;
	SurfaceInfo info;
	bool all_solid;
};

class LevelSolidMap
{
public:
	LevelSolidMap();
	LevelSolidMap(const LevelSolidMap& m);
	LevelSolidMap& operator=(const LevelSolidMap& m);
	~LevelSolidMap();
	TileSolidInfo& insertOrFind(const tile_pos& pos);
	const TileSolidInfo* find(const tile_pos& pos) const;
	void erase(const tile_pos& pos);
	void clear();

	void merge(const LevelSolidMap& m, int xoffset, int yoffset);
private:

	TileSolidInfo** insertRaw(const tile_pos& pos);

	struct row {
		std::vector<TileSolidInfo*> positive_cells, negative_cells;
	};

	std::vector<row> positive_rows_, negative_rows_;
};
