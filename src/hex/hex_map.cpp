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
#include "hex_map.hpp"
#include "hex_tile.hpp"
#include "hex_loader.hpp"
#include "hex_renderable.hpp"
#include "profile_timer.hpp"
#include "tile_rules.hpp"

namespace hex
{
	HexMap::HexMap(const std::string& filename)
		: tiles_(),
		  x_(0),
		  y_(0),
		  width_(0),
		  height_(0),
		  starting_positions_(),
		  changed_(false),
		  renderable_(nullptr)
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
		  changed_(false),
		  renderable_(nullptr)
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
	}

	void HexMap::process_type_string(int x, int y, const std::string& type)
	{
		std::string full_type = boost::trim_copy(type);
		auto pos = full_type.find(' ');
		if(pos != std::string::npos) {
			full_type = full_type.substr(pos+1);
		}
		std::string type_str = full_type;
		pos = type_str.find('^');
		std::string mod_str;
		if(pos != std::string::npos) {
			mod_str = type_str.substr(pos + 1);
			type_str = type_str.substr(0, pos);
		}
		pos = type_str.find(' ');
		std::string player_pos;
		if(pos != std::string::npos) {
			player_pos = type_str.substr(0, pos);
			type_str = type_str.substr(pos + 1);
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

	void HexMap::build()
	{
		profile::manager pman("HexMap::build()");
		auto& terrain_rules = hex::get_terrain_rules();
		for(auto& tr : terrain_rules) {
			tr->match(boost::intrusive_ptr<HexMap>(this));
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
		return boost::intrusive_ptr<HexMap>(new HexMap(filename));
	}

	HexMapPtr HexMap::create(const variant& v)
	{
		return boost::intrusive_ptr<HexMap>(new HexMap(v));
	}

	void HexMap::process()
	{
		if(changed_) {
			changed_ = false;
			renderable_->update(width_, height_, tiles_);
		}
	}

	void HexMap::surrenderReferences(GarbageCollector* collector)
	{
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(HexMap)
		DEFINE_FIELD(dummy, "int")
			return variant(0);
	END_DEFINE_CALLABLE(HexMap)

	HexObject::HexObject(int x, int y, const HexTilePtr & tile, const HexMap* parent)
		: parent_(parent),
		  pos_(x, y),
		  tile_(tile),
		  type_str_(),
		  mod_str_()
	{
	}

	const HexObject* HexObject::getTileAt(int x, int y) const 
	{ 
		return parent_->getTileAt(x, y); 
	}

	const HexObject* HexObject::getTileAt(const point& p) const 
	{
		return parent_->getTileAt(p);
	}

	void HexObject::setTempFlags() const
	{
		for(const auto& f : temp_flags_) {
			flags_.emplace(f);
		}
	}

	void HexObject::clearImages()
	{
		images_.clear();
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
