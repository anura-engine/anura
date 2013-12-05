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
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <sstream>

#include "asserts.hpp"
#include "foreach.hpp"
#include "formula.hpp"
#include "hex_map.hpp"
#include "hex_object.hpp"
#include "hex_tile.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "module.hpp"
#include "variant_utils.hpp"

namespace hex {

static const int HexTileSize = 72;

hex_map::hex_map(variant node)
	: zorder_(node["zorder"].as_int(-1000)), 
	x_(node["x"].as_int(0)), 
	y_(node["y"].as_int(0)),
	width_(node["width"].as_int()), height_(0)
{
	int index = 0;
	for(auto tile_str : node["tiles"].as_list_string()) {
		const int x = index%width_;
		const int y = index/width_;
		tiles_.push_back(hex_object_ptr(new hex_object(tile_str, x, y, this)));
		++index;
	}

	height_ = tiles_.size()/width_;

#ifdef USE_SHADERS
	if(node.has_key("shader")) {
		shader_.reset(new gles2::shader_program(node["shader"]));
	} else {
		shader_.reset();
	}
#endif
}

void hex_map::draw() const
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

void hex_map::build()
{
	foreach(const std::string& rule, hex_object::get_rules()) {
		for(auto tile_ptr : tiles_) {
			tile_ptr->apply_rules(rule);
		}
	}
}

variant hex_map::write() const
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

#if defined(USE_SHADERS)
	if(shader_) {
		res.add("shader", shader_->write());
	}
#endif
	return res.build();
}

hex_object_ptr hex_map::get_hex_tile(direction d, int x, int y) const
{
	int ox = x;
	int oy = y;
	x -= x_;
	y -= y_;
	if(d == NORTH) {
		y -= 1;
	} else if(d == SOUTH) {
		y += 1;
	} else if(d == NORTH_WEST) {
		y -= (abs(ox)%2==0) ? 1 : 0;
		x -= 1;
	} else if(d == NORTH_EAST) {
		y -= (abs(ox)%2==0) ? 1 : 0;
		x += 1;
	} else if(d == SOUTH_WEST) {
		y += (abs(ox)%2) ? 1 : 0;
		x -= 1;
	} else if(d == SOUTH_EAST) {
		y += (abs(ox)%2) ? 1 : 0;
		x += 1;
	} else {
		ASSERT_LOG(false, "Unrecognised direction: " << d);
	}
	if(x < 0 || y < 0 || y >= height_ || x >= width_) {
		return hex_object_ptr();
	}

	const int index = y*width_ + x;
	assert(index >= 0 && index < tiles_.size());
	return tiles_[index];
}

variant hex_map::get_value(const std::string& key) const
{
	if(key == "x_size") {
		return variant(width());
	} else if(key == "y_size") {
		return variant(height());
	} else if(key == "size") {
		std::vector<variant> v;
		v.push_back(variant(width()));
		v.push_back(variant(height()));
		return variant(&v);
	}

	return variant();
}

void hex_map::set_value(const std::string& key, const variant& value)
{
}

point hex_map::get_tile_pos_from_pixel_pos(int mx, int my)
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

hex_object_ptr hex_map::get_tile_from_pixel_pos(int mx, int my) const
{
	point p = get_tile_pos_from_pixel_pos(mx, my);
	return get_tile_at(p.x, p.y);
}

point hex_map::get_pixel_pos_from_tile_pos(int x, int y)
{
	const int HexTileSizeHalf = HexTileSize/2;
	const int HexTileSizeThreeQuarters = (HexTileSize*3)/4;
	const int tx = x*HexTileSizeThreeQuarters;
	const int ty = HexTileSize*y + (abs(x)%2)*HexTileSizeHalf;
	return point(tx, ty);
}

hex_object_ptr hex_map::get_tile_at(int x, int y) const
{
	x -= x_;
	y -= y_;
	if(x < 0 || y < 0 || y >= height_ || x >= width_) {
		return hex_object_ptr();
	}

	const int index = y*width_ + x;
	assert(index >= 0 && index < tiles_.size());
	return tiles_[index];
}

bool hex_map::set_tile(int xx, int yy, const std::string& tile)
{
	if(xx < 0 || yy < 0 || xx >= width_ || yy >= height_) {
		return false;
	}

	const int index = yy*width_ + xx;
	assert(index >= 0 && index < tiles_.size());

	tiles_[index].reset(new hex_object(tile, xx, yy, this));
	return true;
}

point hex_map::loc_in_dir(int x, int y, direction d)
{
	int ox = x;
	int oy = y;
	if(d == NORTH) {
		y -= 1;
	} else if(d == SOUTH) {
		y += 1;
	} else if(d == NORTH_WEST) {
		y -= (abs(ox)%2==0) ? 1 : 0;
		x -= 1;
	} else if(d == NORTH_EAST) {
		y -= (abs(ox)%2==0) ? 1 : 0;
		x += 1;
	} else if(d == SOUTH_WEST) {
		y += (abs(ox)%2) ? 1 : 0;
		x -= 1;
	} else if(d == SOUTH_EAST) {
		y += (abs(ox)%2) ? 1 : 0;
		x += 1;
	} else {
		ASSERT_LOG(false, "Unrecognised direction: " << d);
	}
	return point(x, y);
}

point hex_map::loc_in_dir(int x, int y, const std::string& s)
{
	if(s == "north" || s == "n") {
		return loc_in_dir(x, y, NORTH);
	} else if(s == "south" || s == "s") {
		return loc_in_dir(x, y, SOUTH);
	} else if(s == "north_west" || s == "nw" || s == "northwest") {
		return loc_in_dir(x, y, NORTH_WEST);
	} else if(s == "north_east" || s == "ne" || s == "northeast") {
		return loc_in_dir(x, y, NORTH_EAST);
	} else if(s == "south_west" || s == "sw" || s == "southwest") {
		return loc_in_dir(x, y, SOUTH_WEST);
	} else if(s == "south_east" || s == "se" || s == "southeast") {
		return loc_in_dir(x, y, SOUTH_EAST);
	}
	ASSERT_LOG(false, "Unreognised direction " << s);
	return point();
}

game_logic::formula_ptr hex_map::create_formula(const variant& v)
{
	return game_logic::formula_ptr(new game_logic::formula(v));
}

bool hex_map::execute_command(const variant& var)
{
	bool result = true;
	if(var.is_null()) {
		return result;
	}

	if(var.is_list()) {
		const int num_elements = var.num_elements();
		for(int n = 0; n != num_elements; ++n) {
			if(var[n].is_null() == false) {
				result = execute_command(var[n]) && result;
			}
		}
	} else {
		game_logic::command_callable* cmd = var.try_convert<game_logic::command_callable>();
		if(cmd != NULL) {
			cmd->run_command(*this);
		}
	}
	return result;
}

}
