/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <sstream>

#include "asserts.hpp"
#include "formula.hpp"
#include "hex_map.hpp"
#include "hex_object.hpp"
#include "hex_tile.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "module.hpp"
#include "variant_utils.hpp"

namespace hex 
{
	static const int HexTileSize = 72;

	HexMap::HexMap() 
		: zorder_(-1000), 
		width_(0), 
		height_(0), 
		x_(0), 
		y_(0)
	{
	}
	
	HexMap::HexMap(variant node)
		: zorder_(node["zorder"].as_int(-1000)), 
		x_(node["x"].as_int(0)), 
		y_(node["y"].as_int(0)),
		width_(node["width"].as_int()), 
		height_(0)
	{
		int index = 0;
		for(auto tile_str : node["tiles"].as_list_string()) {
			const int x = index%width_;
			const int y = index/width_;
			tiles_.emplace_back(new HexObject(tile_str, x, y, this));
			++index;
		}
		height_ = tiles_.size()/width_;
		calculateTileAdjacency();
	}

	HexMap::~HexMap()
	{
	}

	void HexMap::draw() const
	{
	#if defined(USE_SHADERS)
	#ifndef NO_EDITOR
		try {
	#endif
			gles2::manager manager(shader_);
	#endif
			for(auto tile_ptr : tiles_) {
				tile_ptr->draw();
			}
	#if defined(USE_SHADERS) && !defined(NO_EDITOR)
		} catch(validation_failure_exception& e) {
			gles2::shader::set_runtime_error("HEX MAP SHADER ERROR: " + e.msg);
		}
	#endif
	}

	void HexMap::build()
	{
		for(const std::string& rule : HexObject::getRules()) {
			for(auto tile_ptr : tiles_) {
				tile_ptr->applyRules(rule);
			}
		}
	}

	void HexMap::calculateTileAdjacency()
	{
		for(auto tile : tiles_) {
			tile->initNeighbors();
		}
	}

	variant HexMap::write() const
	{
		variant_builder res;
		res.add("x", x_);
		res.add("y", y_);
		res.add("zorder", zorder_);

		std::vector<variant> v;
		for(auto tile : tiles_) {
			v.push_back(variant(tile->type()));
		}

		res.add("tiles", variant(&v));

		return res.build();
	}

	HexObjectPtr HexMap::getHexTile(Direction d, int x, int y) const
	{
		int ox = x;
		int oy = y;
		assert(x_ == 0 && y_ == 0);
		x -= x_;
		y -= y_;
		if(d == Direction::NORTH) {
			y -= 1;
		} else if(d == Direction::SOUTH) {
			y += 1;
		} else if(d == Direction::NORTH_WEST) {
			y -= (abs(ox)%2==0) ? 1 : 0;
			x -= 1;
		} else if(d == Direction::NORTH_EAST) {
			y -= (abs(ox)%2==0) ? 1 : 0;
			x += 1;
		} else if(d == Direction::SOUTH_WEST) {
			y += (abs(ox)%2) ? 1 : 0;
			x -= 1;
		} else if(d == Direction::SOUTH_EAST) {
			y += (abs(ox)%2) ? 1 : 0;
			x += 1;
		} else {
			ASSERT_LOG(false, "Unrecognised direction: " << static_cast<int>(d));
		}
		if(x < 0 || y < 0 || static_cast<unsigned>(y) >= height_ || static_cast<unsigned>(x) >= width_) {
			return HexObjectPtr();
		}

		const int index = y*width_ + x;
		assert(index >= 0 && static_cast<unsigned>(index) < tiles_.size());
		return tiles_[index];
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(HexMap)
		DEFINE_FIELD(x_size, "int")
			return variant(obj.width());
		DEFINE_FIELD(y_size, "int")
			return variant(obj.height());
		DEFINE_FIELD(size, "[int,int]")
			std::vector<variant> v;
			v.push_back(variant(obj.width()));
			v.push_back(variant(obj.height()));
			return variant(&v);
	END_DEFINE_CALLABLE(HexMap)

	point HexMap::getTilePosFromPixelPos(int mx, int my)
	{
		const int tesselation_x_size = (3*HexTileSize)/2;
		const int tesselation_y_size = HexTileSize;
		const int x_base = (mx>=0) ? mx / tesselation_x_size * 2 : mx / tesselation_x_size * 2 - 2;
		const int x_mod  = (mx>=0) ? mx % tesselation_x_size : tesselation_x_size + (mx % tesselation_x_size);
		const int y_base = (my>=0) ? my / tesselation_y_size : my / tesselation_y_size - 1;
		const int y_mod  = (my>=0) ? my % tesselation_y_size : tesselation_y_size + (my % tesselation_y_size);
		const int m = 2;

		int x_modifier = 0;
		int y_modifier = 0;

		if(y_mod < tesselation_y_size / 2) {
			if((x_mod * m + y_mod) < (HexTileSize / 2)) {
				x_modifier = -1;
				y_modifier = -1;
			} else if ((x_mod * m - y_mod) < (HexTileSize * 3 / 2)) {
				x_modifier = 0;
				y_modifier = 0;
			} else {
				x_modifier = 1;
				y_modifier = -1;
			}

		} else {
			if((x_mod * m - (y_mod - HexTileSize / 2)) < 0) {
				x_modifier = -1;
				y_modifier = 0;
			} else if((x_mod * m + (y_mod - HexTileSize / 2)) < HexTileSize * 2) {
				x_modifier = 0;
				y_modifier = 0;
			} else {
				x_modifier = 1;
				y_modifier = 0;
			}
		}
		return point(x_base + x_modifier, y_base + y_modifier);
	}

	HexObjectPtr HexMap::getTileFromPixelPos(int mx, int my) const
	{
		point p = getTilePosFromPixelPos(mx, my);
		return getTileAt(p.x, p.y);
	}

	point HexMap::getPixelPosFromTilePos(int x, int y)
	{
		const int HexTileSizeHalf = HexTileSize/2;
		const int HexTileSizeThreeQuarters = (HexTileSize*3)/4;
		const int tx = x*HexTileSizeThreeQuarters;
		const int ty = HexTileSize*y + (abs(x)%2)*HexTileSizeHalf;
		return point(tx, ty);
	}

	HexObjectPtr HexMap::getTileAt(int x, int y) const
	{
		x -= x_;
		y -= y_;
		if(x < 0 || y < 0 || static_cast<unsigned>(y) >= height_ || static_cast<unsigned>(x) >= width_) {
			return HexObjectPtr();
		}

		const int index = y*width_ + x;
		assert(index >= 0 && static_cast<unsigned>(index) < tiles_.size());
		return tiles_[index];
	}

	bool HexMap::setTile(int xx, int yy, const std::string& tile)
	{
		if(xx < 0 || yy < 0 || static_cast<unsigned>(xx) >= width_ || static_cast<unsigned>(yy) >= height_) {
			return false;
		}

		const int index = yy*width_ + xx;
		assert(index >= 0 && static_cast<unsigned>(index) < tiles_.size());

		tiles_[index].reset(new HexObject(tile, xx, yy, this));
		for(auto t : tiles_) {
			t->neighborsChanged();
		}
		return true;
	}

	point HexMap::locInDir(int x, int y, Direction d)
	{
		int ox = x;
		int oy = y;
		if(d == Direction::NORTH) {
			y -= 1;
		} else if(d == Direction::SOUTH) {
			y += 1;
		} else if(d == Direction::NORTH_WEST) {
			y -= (abs(ox)%2==0) ? 1 : 0;
			x -= 1;
		} else if(d == Direction::NORTH_EAST) {
			y -= (abs(ox)%2==0) ? 1 : 0;
			x += 1;
		} else if(d == Direction::SOUTH_WEST) {
			y += (abs(ox)%2) ? 1 : 0;
			x -= 1;
		} else if(d == Direction::SOUTH_EAST) {
			y += (abs(ox)%2) ? 1 : 0;
			x += 1;
		} else {
			ASSERT_LOG(false, "Unrecognised direction: " << static_cast<int>(d));
		}
		return point(x, y);
	}

	point HexMap::locInDir(int x, int y, const std::string& s)
	{
		if(s == "north" || s == "n") {
			return locInDir(x, y, Direction::NORTH);
		} else if(s == "south" || s == "s") {
			return locInDir(x, y, Direction::SOUTH);
		} else if(s == "north_west" || s == "nw" || s == "northwest") {
			return locInDir(x, y, Direction::NORTH_WEST);
		} else if(s == "north_east" || s == "ne" || s == "northeast") {
			return locInDir(x, y, Direction::NORTH_EAST);
		} else if(s == "south_west" || s == "sw" || s == "southwest") {
			return locInDir(x, y, Direction::SOUTH_WEST);
		} else if(s == "south_east" || s == "se" || s == "southeast") {
			return locInDir(x, y, Direction::SOUTH_EAST);
		}
		ASSERT_LOG(false, "Unreognised direction " << s);
		return point();
	}

	game_logic::formula_ptr HexMap::createFormula(const variant& v)
	{
		return game_logic::formula_ptr(new game_logic::formula(v));
	}

	bool HexMap::executeCommand(const variant& var)
	{
		bool result = true;
		if(var.is_null()) {
			return result;
		}

		if(var.is_list()) {
			const int num_elements = var.num_elements();
			for(int n = 0; n != num_elements; ++n) {
				if(var[n].is_null() == false) {
					result = executeCommand(var[n]) && result;
				}
			}
		} else {
			game_logic::command_callable* cmd = var.try_convert<game_logic::command_callable>();
			if(cmd != NULL) {
				cmd->runCommand(*this);
			}
		}
		return result;
	}
}
