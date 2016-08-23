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

#include "asserts.hpp"

#include "json_parser.hpp"
#include "filesystem.hpp"
#include "hex_loader.hpp"
#include "hex_tile.hpp"
#include "module.hpp"
#include "tile_rules.hpp"
#include "profile_timer.hpp"

namespace
{
	typedef std::map<std::string, hex::HexTilePtr> tile_map_type;
	tile_map_type& get_tile_map()
	{
		static tile_map_type res;
		return res;
	}

	hex::terrain_rule_type& get_terrain_rules()
	{
		static hex::terrain_rule_type res;
		return res;
	}

	struct TerrainFileInfo
	{
		TerrainFileInfo(const std::string& img, const rect& a, const std::vector<int>& b) : image_name(img), area(a), border(b) {}
		std::string image_name;
		rect area;
		std::vector<int> border;
	};

	typedef std::map<std::string, TerrainFileInfo> file_info_map_type;
	file_info_map_type& get_file_info()
	{
		static file_info_map_type res;
		return res;
	}

	typedef std::map<std::string, KRE::TexturePtr> texture_map_type; 
	texture_map_type& get_textures()
	{
		static texture_map_type res;
		return res;
	}
}

namespace hex
{
	void load_terrain_files(const variant& v);
	void load_tile_data(const variant& v);
	void load_terrain_data(const variant& v);

	void load(const std::string& base_path)
	{
		// XXX we should make this a threaded load.
		// Load terrain textures first
		profile::manager pman("load_hex_textures");
		std::vector<std::string> files;
		std::vector<std::string> dirs;
		module::get_files_in_dir("images/terrain/", &files, &dirs);
		for(const auto& p : files) {
			//auto pos = p.find("images/");
			//std::string fname = p.substr(pos + 7);			
			get_textures().emplace(p, KRE::Texture::createTexture("terrain/" + p));
		}

		if(!sys::file_exists(module::map_file(base_path + "terrain.cfg"))
			|| !sys::file_exists(module::map_file(base_path + "terrain-file-data.cfg"))
			|| !sys::file_exists(module::map_file(base_path + "terrain-graphics.cfg"))) {
			LOG_INFO("No hex terrain information found.");
			return;
		}
		// Load hex data from files -- order of initialization is important.
		try {
			hex::load_terrain_files(json::parse_from_file(base_path + "terrain-file-data.cfg"));
		} catch(json::ParseError& e) {
			ASSERT_LOG(false, "Error parsing hex " << (base_path + "terrain-file-data.cfg") << " file data: " << e.errorMessage());
		}

		try {
			hex::load_tile_data(json::parse_from_file(base_path + "terrain.cfg"));
		} catch(json::ParseError& e) {		
			ASSERT_LOG(false, "Error parsing hex " << (base_path + "terrain.cfg") << " file data: " << e.errorMessage());
		}

		try {
			hex::load_terrain_data(json::parse_from_file(base_path + "terrain-graphics.cfg"));
		} catch(json::ParseError& e) {		
			ASSERT_LOG(false, "Error parsing hex " << (base_path + "terrain-graphics.cfg") << " file data: " << e.errorMessage());
		}
	
	}

	void load_tile_data(const variant& v)
	{
		profile::manager pman("load_tile_data");
		ASSERT_LOG(v.is_map() && v.has_key("terrain_type") && v["terrain_type"].is_list(), 
			"Expected hex tile data to be a map with 'terrain_type' key.");
		const auto& tt_data = v["terrain_type"].as_list();
		for(const auto& tt : tt_data) {
			ASSERT_LOG(tt.is_map(), "Expected inner items of 'terrain_type' to be maps." << tt.to_debug_string());
			auto tile = HexTile::create(tt);

			auto it = get_tile_map().find(tile->getString());
			ASSERT_LOG(it == get_tile_map().end(), "Duplicate tile string id's found: " << tile->getString());
			get_tile_map()[tile->getString()] = tile;
		}
		LOG_INFO("Loaded " << get_tile_map().size() << " hex tiles into memory.");
	}

	void load_terrain_data(const variant& v)
	{
		profile::manager pman("load_terrain_data");
		ASSERT_LOG(v.is_map() && v.has_key("terrain_graphics") && v["terrain_graphics"].is_list(), 
			"Expected hex tile data to be a map with 'terrain_type' key.");
		const auto& tg_data = v["terrain_graphics"].as_list();
		for(const auto& tg : tg_data) {
			ASSERT_LOG(tg.is_map(), "Expected inner items of 'terrain_type' to be maps." << tg.to_debug_string());
			auto tr = TerrainRule::create(tg);
			if(tr->tryEliminate()) {
				::get_terrain_rules().emplace_back(tr);
				//LOG_INFO("Keep Rule: " << tr->toString());
			} else {
				//LOG_INFO("Removed Rule: " << tr->toString());
			}
		}
		LOG_INFO("Loaded " << get_terrain_rules().size() << " terrain rules into memory.");
	}

	void load_terrain_files(const variant& v)
	{
		profile::manager pman("load_terrain_files");
		ASSERT_LOG(v.is_map(), "Expected terrain file info to be a map. " << v.to_debug_string());
		auto& fi = get_file_info();
		for(const auto& file_data : v.as_map()) {
			ASSERT_LOG(file_data.second.has_key("rect") && file_data.second.has_key("image"), "Need rect and image attributes: " << file_data.second.to_debug_string());
			std::vector<int> borders;
			if(file_data.second.has_key("border")) {
				borders = file_data.second["border"].as_list_int();
			}
			// XX we should consider storing the rect data as normalised.
			fi.emplace(file_data.first.as_string(),  TerrainFileInfo(file_data.second["image"].as_string(), rect(file_data.second["rect"]), borders));
		}
		LOG_INFO("Loaded information for " << fi.size() << " terrain files into memory.");
	}

	HexTilePtr get_tile_from_type(const std::string& type_str)
	{
		auto it = get_tile_map().find(type_str);
		ASSERT_LOG(it != get_tile_map().end(), "No tile definition for type: " << type_str);
		return it->second;
	}

	const terrain_rule_type& get_terrain_rules()
	{
		return ::get_terrain_rules();
	}

	KRE::TexturePtr get_terrain_texture(const std::string& filename, rect* area, std::vector<int>* borders)
	{
		auto& fileinfo = get_file_info();
		auto it = fileinfo.find(filename);
		if(it != fileinfo.end()) {
			if(area) {
				*area = it->second.area;
			}
			if(borders) {
				*borders = it->second.border;
			}
			std::string fname = it->second.image_name;
			const auto pos = it->second.image_name.rfind('/');
			if(pos != std::string::npos) {
				fname = it->second.image_name.substr(pos + 1);
			}
			auto tex_it = get_textures().find(fname);
			ASSERT_LOG(tex_it != get_textures().end(), "No texture found for name: " << fname);
			return tex_it->second;
		}
		LOG_ERROR("Unable to find file information for '" << filename << "' in the file information data.");
		return nullptr;
	}

	const std::string& get_terrain_data(const std::string& filename, rect* area, std::vector<int>* borders)
	{
		auto& fileinfo = get_file_info();
		auto it = fileinfo.find(filename);
		ASSERT_LOG(it != fileinfo.end(), "No terrain file information for '" << filename << "'");
		if(area) {
			*area = it->second.area;
		}
		if(borders) {
			*borders = it->second.border;
		}
		return it->second.image_name;
	}

	bool terrain_info_exists(const std::string& name)
	{
		return get_file_info().find(name) != get_file_info().end();
	}

	std::vector<variant> get_editor_info()
	{
		std::vector<variant> res;
		for(const auto& tm : get_tile_map()) {
			res.emplace_back(variant(tm.second.get()));
		}
		return res;
	}
}
