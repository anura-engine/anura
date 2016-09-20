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

#pragma once

#include "variant.hpp"
#include "hex_fwd.hpp"

#include "Texture.hpp"

namespace hex
{
	typedef std::vector<hex::TerrainRulePtr> terrain_rule_type;

	HexTilePtr get_tile_from_type(const std::string& type_str);
	const terrain_rule_type& get_terrain_rules();
	KRE::TexturePtr get_terrain_texture(const std::string& filename, rect* area, std::vector<int>* borders);
	const std::string& get_terrain_data(const std::string& filename, rect* area=nullptr, std::vector<int>* borders=nullptr);
	bool terrain_info_exists(const std::string& name);
	std::vector<variant> get_editor_info();

	void load(const std::string& base_path);
}
