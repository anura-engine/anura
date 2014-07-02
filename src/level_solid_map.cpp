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


#include <iostream>
#include <set>

#include "level_solid_map.hpp"
#include "preferences.hpp"

namespace 
{
	void merge_SurfaceInfo(SurfaceInfo& a, const SurfaceInfo& b)
	{
		a.friction = std::max<int>(a.friction, b.friction);
		a.traction = std::max<int>(a.traction, b.traction);
		a.damage = std::max<int>(a.damage, b.damage);
		if(b.info) {
			a.info = b.info;
		}
	}
}

const std::string* SurfaceInfo::get_info_str(const std::string& key)
{
	static std::set<std::string> info_set;
	return &*info_set.insert(key).first;
}

LevelSolidMap::LevelSolidMap()
{
}

LevelSolidMap::LevelSolidMap(const LevelSolidMap& m)
{
}

LevelSolidMap& LevelSolidMap::operator=(const LevelSolidMap& m)
{
	return *this;
}

LevelSolidMap::~LevelSolidMap()
{
	clear();
}

TileSolidInfo& LevelSolidMap::insertOrFind(const tile_pos& pos)
{
	TileSolidInfo** result = insertRaw(pos);
	if(!*result) {
		*result = new TileSolidInfo;
	}

	return **result;
}

TileSolidInfo** LevelSolidMap::insertRaw(const tile_pos& pos)
{
	row* r = NULL;
	if(pos.second >= 0) {
		if(positive_rows_.size() <= static_cast<unsigned>(pos.second)) {
			positive_rows_.resize(pos.second + 1);
		}

		r = &positive_rows_[pos.second];
	} else {
		const int index = -(pos.second+1);
		if(negative_rows_.size() <= static_cast<unsigned>(index)) {
			negative_rows_.resize(index + 1);
		}

		r = &negative_rows_[index];
	}

	if(pos.first >= 0) {
		if(r->positive_cells.size() <= static_cast<unsigned>(pos.first)) {
			r->positive_cells.resize(pos.first + 1);
		}

		return &r->positive_cells[pos.first];
	} else {
		const int index = -(pos.first+1);
		if(r->negative_cells.size() <= static_cast<unsigned>(index)) {
			r->negative_cells.resize(index + 1);
		}

		return &r->negative_cells[index];
	}
}

const TileSolidInfo* LevelSolidMap::find(const tile_pos& pos) const
{
	const row* r = NULL;
	if(pos.second >= 0) {
		if(static_cast<unsigned>(pos.second) < positive_rows_.size()) {
			r = &positive_rows_[pos.second];
		} else {
			return NULL;
		}
	} else {
		const int index = -(pos.second+1);
		if(static_cast<unsigned>(index) < negative_rows_.size()) {
			r = &negative_rows_[index];
		} else {
			return NULL;
		}
	}

	if(pos.first >= 0) {
		if(static_cast<unsigned>(pos.first) < r->positive_cells.size()) {
			return r->positive_cells[pos.first];
		} else {
			return NULL;
		}
	} else {
		const int index = -(pos.first+1);
		if(static_cast<unsigned>(index) < r->negative_cells.size()) {
			return r->negative_cells[index];
		} else {
			return NULL;
		}
	}
}

void LevelSolidMap::erase(const tile_pos& pos)
{
	TileSolidInfo** info = insertRaw(pos);
	delete *info;
	*info = NULL;
}

void LevelSolidMap::clear()
{
	for(row& r : positive_rows_) {
		for(TileSolidInfo* info : r.positive_cells) {
			delete info;
		}

		for(TileSolidInfo* info : r.negative_cells) {
			delete info;
		}
	}

	for(row& r : negative_rows_) {
		for(TileSolidInfo* info : r.positive_cells) {
			delete info;
		}

		for(TileSolidInfo* info : r.negative_cells) {
			delete info;
		}
	}

	positive_rows_.clear();
	negative_rows_.clear();
}

void LevelSolidMap::merge(const LevelSolidMap& map, int xoffset, int yoffset)
{
	for(int n = 0; n != map.negative_rows_.size(); ++n) {
		for(int m = 0; m != map.negative_rows_[n].negative_cells.size(); ++m) {
			const tile_pos pos(-m - 1 + xoffset, -n - 1 + yoffset);
			TileSolidInfo& dst = insertOrFind(pos);
			const TileSolidInfo* src = map.negative_rows_[n].negative_cells[m];
			if(!src) {
				continue;
			}

			dst.all_solid = dst.all_solid || src->all_solid;
			merge_SurfaceInfo(dst.info, src->info);
			if(!dst.all_solid) {
				dst.bitmap = dst.bitmap | src->bitmap;
			}
		}

		for(int m = 0; m != map.negative_rows_[n].positive_cells.size(); ++m) {
			const tile_pos pos(m + xoffset, -n - 1 + yoffset);
			TileSolidInfo& dst = insertOrFind(pos);
			const TileSolidInfo* src = map.negative_rows_[n].positive_cells[m];
			if(!src) {
				continue;
			}

			dst.all_solid = dst.all_solid || src->all_solid;
			merge_SurfaceInfo(dst.info, src->info);
			if(!dst.all_solid) {
				dst.bitmap = dst.bitmap | src->bitmap;
			}
		}
	}

	for(int n = 0; n != map.positive_rows_.size(); ++n) {
		for(int m = 0; m != map.positive_rows_[n].negative_cells.size(); ++m) {
			const tile_pos pos(-m - 1 + xoffset, n + yoffset);
			TileSolidInfo& dst = insertOrFind(pos);
			const TileSolidInfo* src = map.positive_rows_[n].negative_cells[m];
			if(!src) {
				continue;
			}

			dst.all_solid = dst.all_solid || src->all_solid;
			merge_SurfaceInfo(dst.info, src->info);
			if(!dst.all_solid) {
				dst.bitmap = dst.bitmap | src->bitmap;
			}
		}

		for(int m = 0; m != map.positive_rows_[n].positive_cells.size(); ++m) {
			const tile_pos pos(m + xoffset, n + yoffset);
			const TileSolidInfo* src = map.positive_rows_[n].positive_cells[m];
			if(!src) {
				continue;
			}

			TileSolidInfo& dst = insertOrFind(pos);

			dst.all_solid = dst.all_solid || src->all_solid;
			merge_SurfaceInfo(dst.info, src->info);
			if(!dst.all_solid) {
				dst.bitmap = dst.bitmap | src->bitmap;
			}
		}
	}
}
