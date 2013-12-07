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
#ifndef HEX_TILE_CPP_INCLUDED
#define HEX_TILE_CPP_INCLUDED

#include <boost/intrusive_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <vector>
#include <map>

#include "decimal.hpp"
#include "graphics.hpp"
#include "formula_callable.hpp"
#include "frame.hpp"
#include "hex_object_fwd.hpp"
#include "raster.hpp"
#include "texture.hpp"
#include "variant.hpp"

namespace hex {

/*
	class hex_tile;

	class basic_hex_tile : public game_logic::formula_callable
	{
	public:
		explicit basic_hex_tile(variant node, hex_tile* owner);
		virtual ~basic_hex_tile();
		
		std::string type() const;
		int chance() const { return chance_; }
		int zorder() const { return zorder_; }
		hex_tile* owner() const { return owner_; }

		virtual void draw(int x, int y) const;
		virtual void get_texture();
		virtual variant write() const;

	protected:
		virtual variant get_value(const std::string&) const;
		virtual void set_value(const std::string& key, const variant& value);
	private:
		hex_tile* owner_;
		graphics::texture texture_;
		std::string image_;
		rect rect_;
		std::vector<variant> nodes_;
		int offset_x_;
		int offset_y_;

		boost::scoped_ptr<frame> frame_;
		mutable int cycle_;
		// Chance that we will use a variation instead of base pattern. 
		// 100 means we definitely will use a variation.
		int chance_;

		int zorder_;

		// Private default constructor and copy constructor to stop them
		// from being used.
		basic_hex_tile() {}
		basic_hex_tile(basic_hex_tile&) {}
	};

	typedef boost::intrusive_ptr<basic_hex_tile> basic_hex_tile_ptr;
	typedef boost::intrusive_ptr<const basic_hex_tile> const_basic_hex_tile_ptr;

	struct strlen_compare
	{
		bool operator()(const std::string& lhs, const std::string& rhs) const
		{
			return lhs.length() < rhs.length();
		}
	};

	typedef std::map<std::string, std::vector<basic_hex_tile_ptr>, strlen_compare> transition_map;

	class hex_tile : public game_logic::formula_callable
	{
	public:

		explicit hex_tile(const std::string& type, variant node);
		virtual ~hex_tile();
		virtual variant write() const;

		std::string type() const { return type_; }
		std::string name() const { return name_; }

		basic_hex_tile_ptr get_single_tile();
		transition_map* find_transition(const std::string& key);
		basic_hex_tile_ptr get_transition_tile(const std::string& key);
		variant get_transitions();
		
		struct editor_info
		{
			std::string name;
			std::string type;
			std::string image;
			mutable graphics::texture texture;
			std::string group;
			rect image_rect;
			void draw(int tx, int ty) const;
		};

		editor_info& get_editor_info() { return editor_info_; } 

	protected:
		virtual variant get_value(const std::string&) const;
		virtual void set_value(const std::string& key, const variant& value);
	private:
		std::string type_;
		std::string name_;
		std::vector<basic_hex_tile_ptr> variations_;
		std::map<std::string, transition_map> transitions_;

		editor_info editor_info_;
	};

	typedef boost::intrusive_ptr<hex_tile> hex_tile_ptr;
	typedef boost::intrusive_ptr<const hex_tile> const_hex_tile_ptr;
	*/

class tile_sheet
{
public:
	explicit tile_sheet(variant node);
	const graphics::texture& get_texture() const { return texture_; }
	rect get_area(int index) const;
private:
	graphics::texture texture_;
	rect area_;
	int nrows_, ncols_, pad_;
};

typedef boost::shared_ptr<const tile_sheet> tile_sheet_ptr;

class tile_type;
typedef boost::intrusive_ptr<const tile_type> tile_type_ptr;


class tile_type : public game_logic::formula_callable
{
public:
	tile_type(const std::string& id, variant node);
		
	struct editor_info
	{
		std::string name;
		std::string type;
		mutable graphics::texture texture;
		std::string group;
		rect image_rect;
		void draw(int tx, int ty) const;
	};

	const std::string& id() const { return id_; }

	const editor_info& get_editor_info() const { return editor_info_; } 

	const std::vector<int>& sheet_indexes() const { return sheet_indexes_; }

	void draw(int x, int y) const;

	//The lowest bit of adjmap indicates if this tile type occurs to the north
	//of the target tile, the next lowest for the north-east and so forth.
	void draw_adjacent(int x, int y, unsigned char adjmap) const;

	decimal height() const { return height_; }

	variant write() const;
private:
	DECLARE_CALLABLE(tile_type);
	std::string id_;
	tile_sheet_ptr sheet_;
	decimal height_;

	std::vector<int> sheet_indexes_;

	struct AdjacencyPattern {
		AdjacencyPattern() : init(false), depth(0)
		{}
		bool init;
		int depth;
		std::vector<int> sheet_indexes;
	};

	mutable AdjacencyPattern adjacency_patterns_[64];
	void calculate_adjacency_pattern(unsigned char adjmap) const;

	editor_info editor_info_;
};

}

#endif // HEX_TILE_CPP_INCLUDED
