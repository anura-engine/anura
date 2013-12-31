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
#pragma once
#ifndef HEX_OBJECT_HPP_INCLUDED
#define HEX_OBJECT_HPP_INCLUDED

#include <boost/intrusive_ptr.hpp>
#include <vector>

#include "graphics.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "raster.hpp"
#include "texture.hpp"
#include "variant.hpp"
#include "hex_object_fwd.hpp"
#include "hex_map.hpp"
#include "hex_tile.hpp"

namespace hex {

class hex_map;

class hex_object : public game_logic::formula_callable
{
public:
	hex_object(const std::string& type, int x, int y, const hex_map* owner);
	virtual ~hex_object() {}

	virtual variant get_value(const std::string&) const;
	virtual void set_value(const std::string& key, const variant& value);

	virtual void draw() const;
	
	void build();
	void apply_rules(const std::string& rule);

	void neighbors_changed();

	const std::string& type() const { return type_; }
	virtual bool execute_command(const variant& var);

	hex_object_ptr get_tile_in_dir(enum direction d) const;
	hex_object_ptr get_tile_in_dir(const std::string& s) const;

	int x() const { return x_; }
	int y() const { return y_; }

	tile_type_ptr tile() const { return tile_; }

	static std::vector<std::string> get_rules();
	static std::vector<tile_type_ptr> get_hex_tiles();
	static std::vector<tile_type_ptr>& get_editor_tiles();

	static tile_type_ptr get_hex_tile(const std::string& type);
private:

	// map coordinates.
	int x_;
	int y_;

	tile_type_ptr tile_;

	struct NeighborType {
		NeighborType() : dirmap(0) {}
		tile_type_ptr type;
		unsigned char dirmap;
	};

	mutable std::vector<NeighborType> neighbors_;
	mutable bool neighbors_init_;
	void init_neighbors() const;

	// String representing the base type of this tile.
	std::string type_;
	// raw pointer to the map that owns this.
	const hex_map* owner_map_;

#ifdef USE_SHADERS
	// shader to draw tile with
	gles2::shader_program_ptr shader_;
#endif

	//forbidden operations
	hex_object(hex_object&);
	void operator=(const hex_object&);
};

}

#endif
