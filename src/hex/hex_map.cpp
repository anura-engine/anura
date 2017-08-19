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

#include <boost/algorithm/string.hpp>

#include "asserts.hpp"
#include "filesystem.hpp"
#include "geometry.hpp"
#include "hex_map.hpp"
#include "hex_tile.hpp"
#include "hex_loader.hpp"
#include "hex_renderable.hpp"
#include "profile_timer.hpp"
#include "tile_rules.hpp"
#include "variant_utils.hpp"

namespace hex
{
	namespace 
	{
		const std::vector<point> even_q_odd_col{ point(0,-1), point(1,-1), point(1,0), point(0,1), point(-1,0), point(-1,-1) };
		const std::vector<point> even_q_even_col{ point(0,-1), point(1,0), point(1,1), point(0,1), point(-1,1), point(-1,0) };
	}

	HexMap::HexMap(const std::string& filename)
		: tiles_(),
		  x_(0),
		  y_(0),
		  width_(0),
		  height_(0),
		  starting_positions_(),
		  changed_(true),
		  rebuild_(true),
		  renderable_(nullptr),
		  rx_(0),
		  ry_(0)
	{
		int max_x = -1;
		// assume a old-style map.
		int y = 0;
		auto contents = sys::read_file(filename);
		std::vector<std::string> lines;
		boost::split(lines, contents, boost::is_any_of("\n\r"), boost::token_compress_on);
		for(const auto& line : lines) {
			int x = 0;
			if(line.empty()) {
				continue;
			}
			std::vector<std::string> types;
			boost::split(types, line, boost::is_any_of(","), boost::token_compress_off);
			for(const auto& type : types) {
				process_type_string(x, y, type);
				++x;
				if(x > max_x) {
					max_x = x;
				}
			}
			++y;
		}
		width_ = max_x;
		height_ = y;
		LOG_INFO("HexMap size: " << width_ << "," << height_);
	}

	HexMap::HexMap(const variant& v)
		: tiles_(),
		  x_(v["x"].as_int32(0)),
		  y_(v["y"].as_int32(0)),
		  width_(v["width"].as_int32()),
		  height_(0),
		  starting_positions_(),
		  changed_(true),
		  rebuild_(true),
		  renderable_(nullptr),
		  rx_(0),
		  ry_(0)
	{
		ASSERT_LOG(v.has_key("tiles") && v["tiles"].is_list(), "No 'tiles' attribute in map.");
		height_ = v["tiles"].num_elements() / width_;
		int y = 0;
		int x = 0;
		for(auto tile_str : v["tiles"].as_list_string()) {
			process_type_string(x, y, tile_str);
			if(++x >= width_) {
				x = 0;
				++y;
			}
		}
		LOG_INFO("HexMap size: " << width_ << "," << height_);
	}

	std::string HexMap::parse_type_string(const std::string& type, std::string* full_type, std::string* type_str, std::string* mod_str) const
	{
		ASSERT_LOG(full_type != nullptr && type_str != nullptr && mod_str != nullptr, "One of the type strings was null.");
		*full_type = boost::trim_copy(type);
		auto pos = full_type->find(' ');
		if(pos != std::string::npos) {
			*full_type = full_type->substr(pos+1);
		}
		*type_str = *full_type;
		pos = type_str->find('^');
		if(pos != std::string::npos) {
			*mod_str = type_str->substr(pos + 1);
			*type_str = type_str->substr(0, pos);
		}
		pos = type_str->find(' ');
		std::string player_pos;
		if(pos != std::string::npos) {
			player_pos = type_str->substr(0, pos);
			*type_str = type_str->substr(pos + 1);
		}

		return player_pos;
	}

	void HexMap::process_type_string(int x, int y, const std::string& type)
	{
		std::string full_type, type_str, mod_str;
		std::string player_pos = parse_type_string(type, &full_type, &type_str, &mod_str);

		if(!player_pos.empty()) {
			starting_positions_.emplace_back(point(x, y), player_pos);
			LOG_INFO("Starting position " << player_pos << ": " << x << "," << y);
		}

		auto tile = get_tile_from_type(type_str);
		tiles_.emplace_back(x, y, tile, this);
		tiles_.back().setTypeStr(full_type, type_str, mod_str);
	}

	HexMap::~HexMap()
	{
	}

	HexObject* HexMap::getNeighbour(point hex, int direction)
	{
		ASSERT_LOG(direction >= 0 && direction < 6, "Direction out of bounds: " << direction);
		int x = 0;
		int y = 0;
		if(hex.x & 1) {
			x = hex.x + even_q_odd_col[direction].x;
			y = hex.y + even_q_odd_col[direction].y;
		} else {
			x = hex.x + even_q_even_col[direction].x;
			y = hex.y + even_q_even_col[direction].y;
		}
		int index = x + y * width_;
		if(index < 0 || index >= static_cast<int>(tiles_.size())) {
			return nullptr;
		}
		return &tiles_[index];
	}

	void HexMap::build()
	{
		profile::manager pman("HexMap::build()");
		for(auto& tile : tiles_) {
			tile.clear();
		}
		auto& terrain_rules = hex::get_terrain_rules();
		for(auto& tr : terrain_rules) {
			tr->match(ffl::IntrusivePtr<HexMap>(this));
		}
	}

	void HexMap::build_single(HexObject* obj)
	{
		profile::manager pman("HexMap::build_single()");
		std::vector<HexObject*> neighbours;
		for(int dir = 0; dir != 6; ++dir) {
			auto n = getNeighbour(obj->getPosition(), dir);
			if(n != nullptr) {
				neighbours.emplace_back(n);
				n->clear();
			}
		}
		auto& terrain_rules = hex::get_terrain_rules();
		for(auto& tr : terrain_rules) {
			tr->match(obj);
			for(auto& n : neighbours) {
				tr->match(n);
			}
		}
	}

	const HexObject* HexMap::getTileAt(int x, int y) const
	{
		x -= x_;
		y -= y_;
		if (x < 0 || y < 0 || y >= height_ || x >= width_) {
			return nullptr;
		}

		const int index = y * width_ + x;
		assert(index >= 0 && index < static_cast<int>(tiles_.size()));
		return &tiles_[index];
	}

	const HexObject* HexMap::getTileAt(const point& p) const 
	{
		return getTileAt(p.x, p.y);
	}

	HexMapPtr HexMap::create(const std::string& filename)
	{
		return ffl::IntrusivePtr<HexMap>(new HexMap(filename));
	}

	HexMapPtr HexMap::create(const variant& v)
	{
		return ffl::IntrusivePtr<HexMap>(new HexMap(v));
	}

	void HexMap::process()
	{
		if(rebuild_) {
			rebuild_ = false;
			changed_ = false;
			tiles_changed_.clear();
			build();
			renderable_->update(width_, height_, tiles_);
		}

		if(changed_) {
			changed_ = false;

			if(!tiles_changed_.empty()) {
				for(auto& index : tiles_changed_) {
					tiles_[index].clear();
					build_single(&tiles_[index]);
				}
			}
			renderable_->update(width_, height_, tiles_);

			tiles_changed_.clear();
		}
		if(renderable_) {
			renderable_->setPosition(rx_, ry_, 0);
		}
	}

	variant HexMap::write() const
	{
		variant_builder res;
		res.add("x", x_);
		res.add("y", y_);
		res.add("width", width_);
		for(const auto& t : tiles_) {
			res.add("tiles", t.getFullTypeString());
		}
		return res.build();
	}

	void HexMap::surrenderReferences(GarbageCollector* collector)
	{
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(HexMap)
		DEFINE_FIELD(tile_height, "int")
			return variant(g_hex_tile_size);
		DEFINE_FIELD(width, "int")
			return variant(obj.getWidth());
		DEFINE_FIELD(height, "int")
			return variant(obj.getHeight());

		DEFINE_FIELD(x, "int")
			return variant(obj.rx_);
		DEFINE_SET_FIELD
			obj.rx_ = value.as_int();
		DEFINE_FIELD(y, "int")
			return variant(obj.ry_);
		DEFINE_SET_FIELD
			obj.ry_ = value.as_int();

		BEGIN_DEFINE_FN(tile_at, "([int,int]) ->builtin hex_tile")
			variant v = FN_ARG(0);
			int x = v[0].as_int();
			int y = v[1].as_int();

			auto tile = obj.getTileAt(x, y);
			ASSERT_LOG(tile, "Illegal tile at " << x << ", " << y);

			return variant(tile->getTileType().get());
		END_DEFINE_FN

		BEGIN_DEFINE_FN(write, "() -> map")
			return obj.write();
		END_DEFINE_FN

		BEGIN_DEFINE_FN(set_tile_at, "([int,int], string) ->commands")
			variant v = FN_ARG(0);
			const int x = v[0].as_int();
			const int y = v[1].as_int();
			std::string name = FN_ARG(1).as_string();

			LOG_INFO("Set tile at: " << x << "," << y << " to '" << name << "'");

			std::string full_type, type_str, mod_str;
			std::string player_pos = obj.parse_type_string(name, &full_type, &type_str, &mod_str);
			
			auto tile = get_tile_from_type(type_str);

			const int index = y * obj.getWidth() + x;
			ASSERT_LOG(index >= 0 && index < static_cast<int>(obj.tiles_.size()), 
				"Index out of bounds." << index << " >= " << obj.tiles_.size());

			ffl::IntrusivePtr<HexMap> map_ref = &const_cast<HexMap&>(obj);

			return variant(new game_logic::FnCommandCallable("set_tile_at", [=]() {
				map_ref->setChanged();
				map_ref->tiles_changed_.emplace(index);
				map_ref->tiles_[index] = HexObject(x, y, tile, map_ref.get());
				map_ref->tiles_[index].setTypeStr(full_type, type_str, mod_str);
			}));
		END_DEFINE_FN

		BEGIN_DEFINE_FN(rebuild, "() -> commands")
			ffl::IntrusivePtr<HexMap> map_ref = &const_cast<HexMap&>(obj);
			return variant(new game_logic::FnCommandCallable("rebuild", [=]() {
				map_ref->setChangedRebuild();
			}));
		END_DEFINE_FN

	END_DEFINE_CALLABLE(HexMap)

	HexObject::HexObject(int x, int y, const HexTilePtr& tile, const HexMap* parent)
		: parent_(parent),
		  pos_(x, y),
		  tile_(tile),
		  type_str_(),
		  mod_str_(),
		  full_type_str_(),
		  flags_(),
		  temp_flags_(),
		  images_()
	{
	}

	const HexObject* HexObject::getTileAt(int x, int y) const 
	{ 
		ASSERT_LOG(parent_ != nullptr, "Parent HexMap was null.");
		return parent_->getTileAt(x, y); 
	}

	const HexObject* HexObject::getTileAt(const point& p) const 
	{
		ASSERT_LOG(parent_ != nullptr, "Parent HexMap was null.");
		return parent_->getTileAt(p);
	}

	void HexObject::setTempFlags() const
	{
		for(const auto& f : temp_flags_) {
			flags_.emplace(f);
		}
	}

	void HexObject::clear()
	{
		images_.clear();
		flags_.clear();
		temp_flags_.clear();
	}

	void HexObject::addImage(const ImageHolder& holder)
	{
		if(holder.name.empty()) {
			return;
		}
		LOG_INFO("Hex" << pos_ << ": " << holder.name << "; layer: " << holder.layer << "; base: " << holder.base << "; center: " << holder.center << "; offset: " << holder.offset);
		images_.emplace_back(holder);
	}
}
