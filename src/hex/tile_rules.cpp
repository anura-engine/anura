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

#include <iomanip>
#include <boost/algorithm/string.hpp>

#include "hex_helper.hpp"
#include "hex_loader.hpp"
#include "hex_map.hpp"
#include "tile_rules.hpp"

#include "random.hpp"
#include "unit_test.hpp"

namespace 
{
	const int HexTileSize = 72;	// XXX abstract this elsewhere.

	std::string rot_replace(const std::string& str, const std::vector<std::string>& rotations, int rot)
	{
		//if(rot == 0) {
		//	return str;
		//}
		auto pos = str.find("@R");
		if(pos == std::string::npos) {
			return str;
		}

		std::string res;
		bool done = false;
		auto start_pos = 0;
		while(pos != std::string::npos) {
			res += str.substr(start_pos, pos - start_pos);
			int index = str[pos + 2] - '0';
			ASSERT_LOG(index >= 0 && index <= 5, "Invalid @R value: " << index);
			res += rotations[(index + rot)%rotations.size()];
			start_pos = pos + 3;
			pos = str.find("@R", start_pos);
		}
		res += str.substr(start_pos);
		//LOG_DEBUG("replaced " << str << " with " << res);
		return res;
	}

	// n*60 is the degrees of rotation
	// c is the center point
	// p is the co-ordinate to rotate.
	point rotate_point(int n, const point& c, const point& p)
	{
		point res = p;
		int x_p, y_p, z_p;
		int x_c, y_c, z_c;
		if(n > 0) {
			hex::evenq_to_cube_coords(p, &x_p, &y_p, &z_p);
			hex::evenq_to_cube_coords(c, &x_c, &y_c, &z_c);

			const int p_from_c_x = x_p - x_c;
			const int p_from_c_y = y_p - y_c;
			const int p_from_c_z = z_p - z_c;

			int r_from_c_x = p_from_c_x, 
				r_from_c_y = p_from_c_y, 
				r_from_c_z = p_from_c_z;
			for(int i = 0; i != n; ++i) {
				const int x = r_from_c_x, y = r_from_c_y, z = r_from_c_z;
				r_from_c_x = -z;
				r_from_c_y = -x;
				r_from_c_z = -y;
			}
			const int r_x = r_from_c_x + x_c;
			const int r_y = r_from_c_y + y_c;
			const int r_z = r_from_c_z + z_c;
			res = hex::cube_to_evenq_coords(r_x, r_y, r_z);
		}
		return res;
	}

	bool string_match(const std::string& s1, const std::string& s2)
	{
		std::string::const_iterator s1it = s1.cbegin();
		std::string::const_iterator s2it = s2.cbegin();
		while(s1it != s1.cend() && s2it != s2.cend()) {
			if(*s1it == '*') {
				++s1it;
				if(s1it == s1.cend()) {
					// an asterisk at the end of the string matches all, so just return a match.
					return true;
				}
				while(s2it != s2.cend() && *s2it != *s1it) {
					++s2it;
				}
				if(s2it == s2.cend() && s1it != s1.cend()) {
					return false;
				}
				++s1it;
				++s2it;
			} else {
				if(*s1it++ != *s2it++) {
					return false;
				}
			}
		}
		if(s1it != s1.cend() || s2it != s2.cend()) {
			return false;
		}
		return true;
	}

	point add_hex_coord(const point& p1, const point& p2) 
	{
		int x_p1, y_p1, z_p1;
		int x_p2, y_p2, z_p2;
		hex::evenq_to_cube_coords(p1, &x_p1, &y_p1, &z_p1);
		hex::evenq_to_cube_coords(p2, &x_p2, &y_p2, &z_p2);
		return hex::cube_to_evenq_coords(x_p1 + x_p2, y_p1 + y_p2, z_p1 + z_p2);
	}

	point sub_hex_coord(const point& p1, const point& p2)
	{
		int x_p1, y_p1, z_p1;
		int x_p2, y_p2, z_p2;
		hex::evenq_to_cube_coords(p1, &x_p1, &y_p1, &z_p1);
		hex::evenq_to_cube_coords(p2, &x_p2, &y_p2, &z_p2);
		return hex::cube_to_evenq_coords(x_p1 - x_p2, y_p1 - y_p2, z_p1 - z_p2);
	}

	point center_point(const point& from_center, const point& to_center, const point& p)
	{
		int x_p, y_p, z_p;
	    int x_c, y_c, z_c;
        hex::evenq_to_cube_coords(p, &x_p, &y_p, &z_p);
        hex::evenq_to_cube_coords(from_center, &x_c, &y_c, &z_c);

        const int p_from_c_x = x_p - x_c;
        const int p_from_c_y = y_p - y_c;
        const int p_from_c_z = z_p - z_c;
			
		int x_r, y_r, z_r;
		hex::evenq_to_cube_coords(to_center, &x_r, &y_r, &z_r);
		return hex::cube_to_evenq_coords(x_r + p_from_c_x, y_r + p_from_c_y, z_r + p_from_c_z);
	}

	point pixel_distance(const point& from, const point& to, int hex_size)
	{
		auto f = hex::get_pixel_pos_from_tile_pos_evenq(from, hex_size);
		auto t = hex::get_pixel_pos_from_tile_pos_evenq(to, hex_size);
		return t - f;
	}
}

namespace hex
{
	TerrainRule::TerrainRule(const variant& v)
		: absolute_position_(nullptr),
		  mod_position_(nullptr),
		  rotations_(),
		  set_flag_(),
		  no_flag_(),
		  has_flag_(),
		  map_(),
		  center_(),
		  tile_data_(),
		  image_(),
		  pos_offset_(),
		  probability_(v["probability"].as_int32(100))
	{
		if(v.has_key("x")) {
			absolute_position_ = std::unique_ptr<point>(new point(v["x"].as_int32()));
		}
		if(v.has_key("y")) {
			if(absolute_position_ != nullptr) {
				absolute_position_->y = v["y"].as_int32();
			} else {
				absolute_position_ = std::unique_ptr<point>(new point(0, v["y"].as_int32()));
			}
		}
		if(v.has_key("mod_x")) {
			mod_position_ = std::unique_ptr<point>(new point(v["mod_x"].as_int32()));
		}
		if(v.has_key("mod_y")) {
			if(mod_position_ != nullptr) {
				mod_position_->y = v["mod_y"].as_int32();
			} else {
				mod_position_ = std::unique_ptr<point>(new point(0, v["mod_y"].as_int32()));
			}
		}
		if(v.has_key("rotations")) {
			rotations_ = v["rotations"].as_list_string();
		}
		std::vector<std::string> set_no_flag;
		if(v.has_key("set_no_flag")) {
			set_no_flag = v["set_no_flag"].as_list_string();
		}
		if(v.has_key("set_flag")) {
			set_flag_ = v["set_flag"].as_list_string();
		}
		for(const auto& snf : set_no_flag) {
			set_flag_.emplace_back(snf);
		}
		if(v.has_key("no_flag")) {
			no_flag_ = v["no_flag"].as_list_string();
		}
		for(const auto& snf : set_no_flag) {
			no_flag_.emplace_back(snf);
		}
		if(v.has_key("has_flag")) {
			has_flag_ = v["has_flag"].as_list_string();
		}
		if(v.has_key("map")) {
			map_ = v["map"].as_list_string();
		}
		
		if(v.has_key("image")) {
			const auto& img_v = v["image"];
			if(img_v.is_list()) {
				for(const auto& img : img_v.as_list()) {
					image_.emplace_back(new TileImage(img));
				}
			} else if(v.is_map()) {
				image_.emplace_back(new TileImage(v["image"]));
			}
		}
	}

	std::string TerrainRule::toString() const
	{
		std::stringstream ss;
		if(absolute_position_) {
			ss << "x,y: " << *absolute_position_ << "; ";
		}
		if(mod_position_) {
			ss << "mod_x/y: " << *mod_position_ << "; ";
		}
		if(!rotations_.empty()) {
			ss << "rotations:";
			for(const auto& rot : rotations_) {
				ss << " " << rot;
			}
			ss << "; ";
		}
		if(!image_.empty()) {
			ss << "images: ";
			for(const auto& img : image_) {
				ss << " " << img->toString();
			}
			ss << "; ";
		}
		if(!tile_data_.empty()) {
			ss << "tiles: ";
			for(const auto& td : tile_data_) {
				ss << " " << td->toString();
			}
			ss << "; ";
		}
	
		return ss.str();
	}

	void TerrainRule::preProcessMap(const variant& tiles)
	{
		if(!tiles.is_null()) {
			if(tiles.is_list()) {
				for(const auto& tile : tiles.as_list()) {
					auto td = std::unique_ptr<TileRule>(new TileRule(shared_from_this(), tile));
					tile_data_.emplace_back(std::move(td));
				}
			} else if(tiles.is_map()) {
				tile_data_.emplace_back(std::unique_ptr<TileRule>(new TileRule(shared_from_this(), tiles)));
			} else {
				ASSERT_LOG(false, "Tile data was neither list or map.");
			}
		}

		// Map processing.
		if(map_.empty()) {
			return;
		}
		std::string first_line = boost::trim_copy(map_.front());
		const bool odd_start = first_line.front() == ',';
		int lineno = odd_start ? 0 : 1;
		auto td = std::unique_ptr<TileRule>(new TileRule(shared_from_this()));
		std::vector<point> coord_list;
		for(const auto& map_line : map_) {
			std::vector<std::string> strs;
			std::string ml = boost::erase_all_copy(boost::erase_all_copy(map_line, "\t"), " ");
			boost::split(strs, ml, boost::is_any_of(","));
			// valid symbols are asterisk(*), period(.) and tile references(0-9).
			// '.' means this rule does not apply to this hex
			// '*' means this rule applies to this hex, but this hex can be any terrain type
			// an empty string is an odd line.
			int colno = 0;
			for(auto& str : strs) {
				const int x = colno * 2 - ((lineno + 1) % 2);
				const int y = lineno / 2;
				if(str == ".") {
					coord_list.emplace_back(x, y);
				} else if(str.empty()) {
					// ignore
				} else if(str == "*") {
					td->addPosition(point(x, y));
					coord_list.emplace_back(x, y);
				} else {
					coord_list.emplace_back(x, y);
					try {
						int pos = boost::lexical_cast<int>(str);
						bool found = false;
						for(auto& td : tile_data_) {
							if(td->getMapPos() == pos) {
								td->addPosition(point(x, y));
								if(pos == 1) {
									center_ = point(x, y);
								}
								found = true;
							}
						}
						ASSERT_LOG(found, "No tile for pos: " << pos);
					} catch(boost::bad_lexical_cast&) {
						ASSERT_LOG(false, "Unable to convert to number" << str);
					}
				}
				if(!((lineno % 2) != 0 && strs.front().empty())) {
					++colno;
				}
			}
			++lineno;
		}

		// Code to calculate the offset needed when an image is specified in the base terrain_graphics element.
		if(!image_.empty()) {
			const int max_loops = rotations_.empty() ? 1 : rotations_.size();
			pos_offset_.resize(max_loops);
			if(odd_start) {
				for(int rot = 0; rot != max_loops; ++rot) {
					//pos_offset_[rot] = pixel_distance(center_, point(0, -1), HexTileSize) + point(18, 0);
					pos_offset_[rot] = point(0, -HexTileSize);
				}
			} else {
				if(rotations_.empty()) {
					pos_offset_[0] = point();
				} else {
					for(int rot = 0; rot != max_loops; ++rot) {
						point min_coord(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
						for(const auto& p : coord_list) {
							const point rotated_p = rotate_point(rot, center_, p);
							if(rotated_p.x <= min_coord.x) {
								if(!(rotated_p.x == min_coord.x && rotated_p.y > min_coord.y)) {
									min_coord = rotated_p;
								}
							}
						}
						if(rot % 2) {
							// odd needs offsetting then 0,0 added
							pos_offset_[rot] = pixel_distance(point(0, 1), min_coord, HexTileSize);// - point(0, 72);;
						} else {
							// even just need to choose the minimum x/y tile. -- done above.
							pos_offset_[rot] = pixel_distance(center_, min_coord, HexTileSize) + point(0, 72);
						}
					}
				}
			}
		}

		/*for(auto& td : tile_data_) {
			td->center(center_, point(0, 0));
		}
		center_ = point(0, 0);*/

		if(!td->getPosition().empty()) {
			tile_data_.emplace_back(std::move(td));
		}
	}

	point TerrainRule::calcOffsetForRotation(int rot)
	{
		if(image_.empty()) {
			return point();
		}
		return pos_offset_[rot];
	}

	// return false to remove this rule. true if it should be kept.
	bool TerrainRule::tryEliminate()
	{
		// If rule has no images, we keep it since it may have flags.
		bool has_image = false;
		for(auto& td : tile_data_) {
			has_image |= td->hasImage();
		}
		if(!has_image && image_.empty()) {
			return true;
		}

		// Basically we should check these as part of initialisation and discard if the particular combination specified in the 
		// image tag isn't valid.
		bool ret = false;
		for(auto& td : tile_data_) {
			ret |= td->eliminate(rotations_);
		}
		for(auto& img : image_) {
			const bool keep = img->eliminate(rotations_);
			ret |= keep;
			//if(!keep) {
			//	LOG_INFO("would eliminate: " << img->toString());
			//}
		}
		return ret;
	}

	void TerrainRule::applyImage(HexObject* hex, int rot)
	{
		point offs = calcOffsetForRotation(rot);
		for(const auto& img : image_) {
			if(img) {
				hex->addImage(img->genHolder(rot, offs));
			}
		}
	}

	TerrainRulePtr TerrainRule::create(const variant& v)
	{
		auto tr = std::make_shared<TerrainRule>(v);
		tr->preProcessMap(v["tile"]);
		return tr;
	}

	TileRule::TileRule(TerrainRulePtr parent, const variant& v)
		: parent_(parent),
		  position_(),
		  pos_(v["pos"].as_int32(0)),
		  type_(),
		  set_flag_(),
		  no_flag_(),
		  has_flag_(),
		  image_(nullptr),
		  pos_rotations_(),
		  min_pos_()
	{
		if(v.has_key("x") || v.has_key("y")) {
			position_.emplace_back(v["x"].as_int32(0), v["y"].as_int32(0));
		}
		std::vector<std::string> set_no_flag;
		if(v.has_key("set_no_flag")) {
			set_no_flag = v["set_no_flag"].as_list_string();
		}
		if(v.has_key("set_flag")) {
			set_flag_ = v["set_flag"].as_list_string();
		}
		for(const auto& snf : set_no_flag) {
			set_flag_.emplace_back(snf);
		}
		if(v.has_key("no_flag")) {
			no_flag_ = v["no_flag"].as_list_string();
		}
		for(const auto& snf : set_no_flag) {
			no_flag_.emplace_back(snf);
		}
		if(v.has_key("has_flag")) {
			has_flag_ = v["has_flag"].as_list_string();
		}
		if(v.has_key("type")) {
			type_ = v["type"].as_list_string();
		}
		// ignore name as I've not seen any instances in a tile def.
		if(v.has_key("image")) {
			image_.reset(new TileImage(v["image"]));
		}
	}

	// To match * type
	TileRule::TileRule(TerrainRulePtr parent)
		: parent_(parent),
		  position_(),
		  pos_(0),
		  type_(),
		  set_flag_(),
		  no_flag_(),
		  has_flag_(),
		  image_(nullptr),
		  pos_rotations_(),
		  min_pos_()
	{
		type_.emplace_back("*");
	}

	void TileRule::center(const point& from_center, const point& to_center)
	{
		for(auto& p : position_) {
	        int x_p, y_p, z_p;
	        int x_c, y_c, z_c;
            hex::evenq_to_cube_coords(p, &x_p, &y_p, &z_p);
            hex::evenq_to_cube_coords(from_center, &x_c, &y_c, &z_c);

            const int p_from_c_x = x_p - x_c;
            const int p_from_c_y = y_p - y_c;
            const int p_from_c_z = z_p - z_c;
			
			int x_r, y_r, z_r;
			hex::evenq_to_cube_coords(to_center, &x_r, &y_r, &z_r);
			p = hex::cube_to_evenq_coords(x_r + p_from_c_x, y_r + p_from_c_y, z_r + p_from_c_z);
		}
	}

	bool TileRule::eliminate(const std::vector<std::string>& rotations)
	{
		if(image_) {
			return image_->eliminate(rotations);
		}
		//return true;
		return false;
	}

	std::string TileRule::toString()
	{
		std::stringstream ss;
		ss << "TileRule: ";
		bool no_comma = true;
		if(!has_flag_.empty()) {
			ss << "has:";
			for(auto& hf : has_flag_) {
				ss << (no_comma ? "" : ",") << " \"" << hf << "\"";
				if(no_comma) {
					no_comma = false;
				}
			}
		}
		
		if(!set_flag_.empty()) {
			no_comma = true;
			ss << "; set:";
			for(auto& sf : set_flag_) {
				ss << (no_comma ? "" : ",") << " \"" << sf << "\"";
				if(no_comma) {
					no_comma = false;
				}
			}
		}

		if(!no_flag_.empty()) {
			no_comma = true;
			ss << "; no:";
			for(auto& nf : no_flag_) {
				ss << (no_comma ? "" : ",") << " \"" << nf << "\"";
				if(no_comma) {
					no_comma = false;
				}
			}
		}

		no_comma = true;
		ss << "; types:";
		for(auto& type : type_) {
			ss << (no_comma ? "" : ",") << " \"" << type << "\"";
			if(no_comma) {
				no_comma = false;
			}
		}
		
		no_comma = true;
		ss << "; positions:";
		for(auto& pos : position_) {
			ss << (no_comma ? "" : ", ") << pos;
			if(no_comma) {
				no_comma = false;
			}
		}

		if(image_) {
			ss << "; image: " << image_->toString();
		}
		return ss.str();
	}

	bool TileRule::matchFlags(const HexObject* obj, TerrainRule* tr, const std::vector<std::string>& rs, int rot)
	{
		const auto& has_flag = has_flag_.empty() ? tr->getHasFlags() : has_flag_;
		for(auto& f : has_flag) {
			if(!obj->hasFlag(rot_replace(f, rs, rot))) {
				return false;
			}
		}
		const auto& no_flag = no_flag_.empty() ? tr->getNoFlags() : no_flag_;
		for(auto& f : no_flag) {
			if(obj->hasFlag(rot_replace(f, rs, rot))) {
				return false;
			}
		}
		return true;
	}

	bool TileRule::match(const HexObject* obj, TerrainRule* tr, const std::vector<std::string>& rs, int rot)
	{
		if(obj == nullptr) {
			/*for(auto& type : type_) {
				if(type == "*") {
					return true;
				}
			}*/
			return false;
		}

		const std::string& hex_type_full = obj->getFullTypeString();
		const std::string& hex_type = obj->getTypeString();
		bool invert_match = false;
		bool tile_match = true;
		for(auto& type : type_) {
			type = rot_replace(type, rs, rot);
			if(type == "!") {
				invert_match = !invert_match;
				continue;
			}
			const bool matches = type == "*" || string_match(type, hex_type_full) || string_match(type, hex_type);
			if(!matches) {
				if(invert_match == true) {
					tile_match = true;
				} else {
					tile_match = false;
				}
			} else {
				if(invert_match == false) {
					tile_match = true;
					break;
				} else {
					tile_match = false;
				}
			}
		}

		if(tile_match) {
			if(!matchFlags(obj, tr, rs, rot)) {
				return false;
			}

			const auto& set_flag = set_flag_.empty() ? tr->getSetFlags() : set_flag_;
			for(auto& f : set_flag) {
				if(obj != nullptr) {
					obj->addTempFlag(rot_replace(f, rs, rot));
				}
			}
		}

		return tile_match;
	}

	void TileRule::applyImage(HexObject* hex, int rot)
	{
		if(image_) {
			hex->addImage(image_->genHolder(rot, point()));
		}
	}

	TileImage::TileImage(const variant& v)
		: layer_(v["layer"].as_int32(0)),
		  image_name_(v["name"].as_string_default("")),
		  random_start_(v["random_start"].as_bool(true)),
		  base_(),
		  center_(),
		  opacity_(1.0f),
		  crop_(),
		  variants_(),
		  variations_(),
		  image_files_(),
		  animation_frames_(),
		  animation_timing_(v["animation_timing"].as_int32(0)),
		  is_animated_(false)
	{
		if(v.has_key("O")) {
			opacity_ = v["O"]["param"].as_float();
		}
		if(v.has_key("CROP")) {
			crop_ = rect(v["CROP"]["param"]);
		}
		if(v.has_key("base")) {
			base_ = point(v["base"]);
		}
		if(v.has_key("center")) {
			center_ = point(v["center"]);
		}
		if(v.has_key("variant")) {
			for(const auto& ivar : v["variant"].as_list()) {
				variants_.emplace_back(ivar);
			}
		}
		if(v.has_key("animation_frames")) {
			animation_frames_ = v["animation_frames"].as_list_int();
			is_animated_ = true;
		}
		if(v.has_key("variations")) {
			auto vars = v["variations"].as_list_string();
			auto pos = image_name_.find("@V");
			auto pos_r = image_name_.find("@R");
			if(pos_r != std::string::npos) {
				variations_ = vars;
			} else {
				for(const auto& var : vars) {
					std::string name = image_name_.substr(0, pos) + var + image_name_.substr(pos + 2);
					if(terrain_info_exists(name)) {
						variations_.emplace_back(var);
					}
				}
			}
		}
	}

	const std::string& TileImage::getNameForRotation(int rot)
	{
		auto it = image_files_.find(rot);
		if(it == image_files_.end()) {
			static std::string res;
			return res;
		}
		ASSERT_LOG(it != image_files_.end(), "No image for rotation: " << rot << " : " << toString());
		ASSERT_LOG(!it->second.empty(), "No files for rotation: " << rot);
		return it->second[rng::generate() % it->second.size()];
	}

	bool TileImage::isValidForRotation(int rot)
	{
		return image_files_.find(rot) != image_files_.end();
	}

	std::string TileImage::toString() const
	{
		std::stringstream ss;
		ss << "name:" << image_name_ << "; layer(" << layer_ << "); base: " << base_;
		if(!variations_.empty()) {
			ss << "; variations:";
			for(auto& var : variations_) {
				ss << " " << var;
			}
		}
		return ss.str();
	}

	ImageHolder TileImage::genHolder(int rot, const point& offs)
	{
		ImageHolder res;
		res.name = getNameForRotation(rot);
		res.base = getBase();
		res.center = getCenter();
		res.crop = getCropRect();
		res.is_animated = is_animated_;
		res.layer = getLayer();
		res.offset = offs;
		res.opacity = getOpacity();
		if(is_animated_) {
			res.animation_frames = image_files_[rot];
		}
		res.animation_timing = animation_timing_;
		return res;
	}

	bool TileImage::eliminate(const std::vector<std::string>& rotations)
	{
		// Calculate whether particular rotations are valid.
		// return true if we should keep this, false if there are
		// no valid terrain images available.
		auto pos_v = image_name_.find("@V");
		auto pos_r = image_name_.find("@R");
		auto pos_a = image_name_.find("@A");

		if(pos_a != std::string::npos) {
			ASSERT_LOG(pos_v == std::string::npos && pos_r == std::string::npos, 
				"Found an animation string with @V or @R specifier which isn't valid. " << image_name_);
			if(!animation_frames_.empty()) {
				for(const auto& ani : animation_frames_) {
					std::stringstream ss;
					ss 	<< image_name_.substr(0, pos_a)
						<< std::dec << std::setw(2) << std::setfill('0') << ani
						<< image_name_.substr(pos_a + 2);
					const std::string img_name = ss.str();
					if(terrain_info_exists(img_name)) {
						image_files_[0].emplace_back(img_name);
					}
				}
			} else {
				if(terrain_info_exists(image_name_)) {
					image_files_[0].emplace_back(image_name_);
				}
			}
			return !image_files_.empty();
		}

		if(pos_r == std::string::npos) {
			if(!variations_.empty()) {
				for(const auto& var : variations_) {
					pos_v = image_name_.find("@V");
					std::string img_name = image_name_.substr(0, pos_v) + var + image_name_.substr(pos_v + 2);
					if(terrain_info_exists(img_name)) {
						image_files_[0].emplace_back(img_name);
					}
				}
			} else {
				if(terrain_info_exists(image_name_)) {
					image_files_[0].emplace_back(image_name_);
				}
			}
			return !image_files_.empty();
		}
		// rotate all the combinations and test them
		for(int rot = 0; rot != 6; ++rot) {
			std::string name = rot_replace(image_name_, rotations, rot);
			if(pos_v == std::string::npos) {
				if(terrain_info_exists(name)) {
					image_files_[rot].emplace_back(name);
				}
				continue;
			}
			for(const auto& var : variations_) {
				pos_v = name.find("@V");
				std::string img_name = name.substr(0, pos_v) + var + name.substr(pos_v + 2);
				if(terrain_info_exists(img_name)) {
					image_files_[rot].emplace_back(img_name);
				} else {
				}
			}
		}

		return !image_files_.empty();
	}

	TileImageVariant::TileImageVariant(const variant& v)
		: tod_(v["tod"].as_string_default("")),
		  name_(v["name"].as_string_default("")),
		  random_start_(v["random_start"].as_bool(true)),
		  has_flag_(),
		  crop_(),
		  animation_frames_(),
		  animation_timing_(v["animation_timing"].as_int32(0)),
		  layer_(v["layer"].as_int32(0))
	{
		if(v.has_key("has_flag")) {
			has_flag_ = v["has_flag"].as_list_string();
		}
		if(v.has_key("CROP")) {
			crop_ = rect(v["CROP"]["param"]);
		}
		if(v.has_key("animation_frames")) {
			animation_frames_ = v["animation_frames"].as_list_int();
		}
	}

	std::string TileImage::getName() const
	{
		// XXX WIP
		std::string name = image_name_;
		auto pos = name.find("@V");
		if(!variations_.empty() && pos != std::string::npos) {
			int index = rng::generate() % variations_.size();
			const std::string& var = variations_[index];
			name = name.substr(0, pos) + var + name.substr(pos + 2);
		}
		return name;
	}

	bool TerrainRule::match(const HexMapPtr& hmap)
	{
		if(absolute_position_) {
			ASSERT_LOG(tile_data_.size() != 1, "Number of tiles is not correct in rule.");
			if(!tile_data_[0]->match(hmap->getTileAt(*absolute_position_), this, std::vector<std::string>(), 0)) {
				return false;
			}
		}

		// check rotations.
		ASSERT_LOG(rotations_.size() == 6 || rotations_.empty(), "Set of rotations not of size 6(" << rotations_.size() << ").");
		const int max_loop = rotations_.empty() ? 1 : rotations_.size();

		for(auto& hex : hmap->getTilesMutable()) {
			for(int rot = 0; rot != max_loop; ++rot) {
				if(mod_position_) {
					auto& pos = hex.getPosition();
					if((pos.x % mod_position_->x) != 0 || (pos.y % mod_position_->y) != 0) {
						continue;
					}
				}

				std::vector<std::pair<HexObject*, TileRule*>> obj_to_set_flags;
				// We expect tiles to have position data.
				bool tile_match = true;

				if(!image_.empty()) {
					bool res = false;
					for(const auto& img : image_) {
						res |= img->isValidForRotation(rot);
					}
					if(!res) {
						// XXX should we check for images in tile tags?
						continue;
					}
				}

				bool match_pos = true;
				auto td_it = tile_data_.cbegin();
				for(; td_it != tile_data_.cend() && match_pos; ++td_it) {
					const auto& td = *td_it;
					ASSERT_LOG(td->hasPosition(), "tile data doesn't have an x,y position.");
					const auto& pos_data = td->getPosition();

					for(const auto& p : pos_data) {
						//point rot_p = sub_hex_coord(add_hex_coord(hex.getPosition(), rotate_point(rot, center_, p)), center_);
						point rot_p = rotate_point(rot, add_hex_coord(center_, hex.getPosition()), add_hex_coord(p, hex.getPosition()));
						auto new_obj = const_cast<HexObject*>(hmap->getTileAt(rot_p));
						if(td->match(new_obj, this, rotations_, rot)) {
							//match_pos = true;
							if(new_obj) {
								obj_to_set_flags.emplace_back(std::make_pair(new_obj, td.get()));
							}
						} else {
							match_pos = false;
							if(new_obj) {
								new_obj->clearTempFlags();
							}
							break;
						}
					}
				}
				if(!match_pos) {
					tile_match = false;
					for(auto& obj : obj_to_set_flags) {
						obj.first->clearTempFlags();
					}
					obj_to_set_flags.clear();
				}

				if(tile_match) {
					if(probability_ != 100) {
						auto rand_no = rng::generate() % 100;
						if(rand_no > probability_) {
							for(auto& obj : obj_to_set_flags) {
								obj.first->clearTempFlags();
							}
							obj_to_set_flags.clear();
							continue;
						}
					}
					// XXX need to fix issues when other tiles have images that need to match a different hex
					//tile_data_.front()->applyImage(&hex, rotations_, rot);
					applyImage(&hex, rot);
					for(auto& obj : obj_to_set_flags) {
						obj.first->setTempFlags();
						obj.second->applyImage(obj.first, rot);
					}
				}
			}
		}
		return false;
	}
}

UNIT_TEST(point_rotate)
{
	CHECK_EQ(rotate_point(0, point(2, 2), point(2, 1)), point(2, 1));
	CHECK_EQ(rotate_point(1, point(2, 2), point(2, 1)), point(3, 2));
	CHECK_EQ(rotate_point(2, point(2, 2), point(2, 1)), point(3, 3));
	CHECK_EQ(rotate_point(3, point(2, 2), point(2, 1)), point(2, 3));
	CHECK_EQ(rotate_point(4, point(2, 2), point(2, 1)), point(1, 3));
	CHECK_EQ(rotate_point(5, point(2, 2), point(2, 1)), point(1, 2));
	CHECK_EQ(rotate_point(1, point(3, 3), point(3, 2)), point(4, 2));
}

UNIT_TEST(string_match)
{
	CHECK_EQ(string_match("*", "Any string"), true);
	CHECK_EQ(string_match("Chs", "Ch"), false);
	CHECK_EQ(string_match("G*", "Gg"), true);
	CHECK_EQ(string_match("G*^Fp", "Gg^Fp"), true);
	CHECK_EQ(string_match("Re", "Rd"), false);
	CHECK_EQ(string_match("*^Bsb|", "Gg^Bsb|"), true);
	CHECK_EQ(string_match("*^Bsb|", "Gg^Fp"), false);
	//CHECK_EQ(string_match("Aa", "Aa^Fpa"), true);
}

UNIT_TEST(rot_replace)
{
	CHECK_EQ(rot_replace("transition-@R0-@R1-x", std::vector<std::string>{"n", "ne", "se", "s", "sw", "nw"}, 1), "transition-ne-se-x");
	CHECK_EQ(rot_replace("xyzzy", std::vector<std::string>{}, 0), "xyzzy");
	CHECK_EQ(rot_replace("transition-@R0", std::vector<std::string>{"n", "ne", "se", "s", "sw", "nw"}, 0), "transition-n");
	CHECK_EQ(rot_replace("transition-@R0", std::vector<std::string>{"n", "ne", "se", "s", "sw", "nw"}, 1), "transition-ne");
	CHECK_EQ(rot_replace("transition-@R0", std::vector<std::string>{"n", "ne", "se", "s", "sw", "nw"}, 5), "transition-nw");
}

UNIT_TEST(pixel_distance)
{
	CHECK_EQ(pixel_distance(point(0,0), point(0,1), 72), point(0, 72));
	CHECK_EQ(pixel_distance(point(0,0), point(1,0), 72), point(54, -36));
}

