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

#include <boost/array.hpp>
#include <boost/regex.hpp>

#include <map>
#include <string>

#include "geometry.hpp"

#include "formula_callable.hpp"
#include "level_object_fwd.hpp"
#include "point_map.hpp"
#include "variant.hpp"

int parse_zorder(const variant& v, variant default_val=variant());
variant write_zorder(int zorder);

int get_named_zorder(const std::string& key, int default_value=0);

struct TilePattern;
class MultiTilePattern;

namespace 
{
	struct TilePatternCache;
}

class TileMap
{
public:
	static void init(variant node);
	static void loadAll();
	static void load(const std::string& fname, const std::string& tile_id);
	static const std::vector<std::string>& getFiles(const std::string& tile_id);

	TileMap();
	explicit TileMap(variant node);
	TileMap(const TileMap& o);
	~TileMap();

	variant write() const;
	void buildTiles(std::vector<LevelTile>* tiles, const rect* r=nullptr) const;
	bool setTile(int xpos, int ypos, const std::string& str);
	int zorder() const { return zorder_; }
	int getXSpeed() const { return x_speed_; }
	int getYSpeed() const { return y_speed_; }
	void setZOrder(int z) { zorder_ = z; }
	void setSpeed(int x_speed, int y_speed) { x_speed_ = x_speed; y_speed_ = y_speed; }
	const char* getTileFromPixelPos(int xpos, int ypos) const;
	const char* getTile(int y, int x) const;
	int getVariations(int x, int y) const;
	void flipVariation(int x, int y, int delta=0);

	//variants are not thread-safe, so this function clears out variant
	//info to prepare the tile map to be placed into a worker thread.
	void prepareForCopyToWorkerThread();

#ifndef NO_EDITOR
	//Functions for rebuilding all live tile maps when there is a change
	//to tile map data. prepareRebuildAll() should be called before
	//the change, and rebuildAll() after.
	static void prepareRebuildAll();
	static void rebuildAll();
#endif

private:
	void buildPatterns();
	const std::vector<const TilePattern*>& getPatterns() const;

	int variation(int x, int y) const;
	const TilePattern* getMatchingPattern(int x, int y, TilePatternCache& cache, bool* face_right) const;
	variant getValue(const std::string& key) const { return variant(); }
	int xpos_, ypos_;
	int x_speed_, y_speed_;
	int zorder_;

	typedef boost::array<char, 4> tile_string;

	//a map of all of our strings, which maps into pattern_index.
	std::vector<std::vector<int>> map_;

	//an entry which holds one of the strings found in this map, as well
	//as the patterns it matches.
	struct PatternIndexEntry 
	{
		PatternIndexEntry() { for(int n = 0; n != str.size(); ++n) { str[n] = 0; } }
		tile_string str;
		mutable std::vector<const boost::regex*> matching_patterns;
	};

	const PatternIndexEntry& getTileEntry(int y, int x) const;

	std::vector<PatternIndexEntry> pattern_index_;

	int getPatternIndexEntry(const tile_string& str);

	//the subset of all multi tile patterns which might be valid for this map.
	std::vector<const MultiTilePattern*> multi_patterns_;

	typedef std::pair<point, int> point_zorder;
	//function to apply the first found matching multi pattern.
	//mapping represents all the tiles added in our zorder.
	//different_zorder_mapping represents the mappings in different zorders
	//to this tile_map.
	void applyMatchingMultiPattern(int& x, int y,
		const MultiTilePattern& pattern,
		point_map<LevelObject*>& mapping,
		std::map<point_zorder, LevelObject*>& different_zorder_mapping) const;

	//the subset of all global patterns which might be valid for this map.
	std::vector<const TilePattern*> patterns_;

	//when we generate patterns_ we check the underlying vector's version.
	//when it is updated it will get a new version and so we'll have to
	//update our view into it.
	int patterns_version_;

	typedef std::vector<std::vector<int>> VariationsType;
	VariationsType variations_;

#ifndef NO_EDITOR
	variant node_;
#endif
};
