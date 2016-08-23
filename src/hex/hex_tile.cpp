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

#include "hex_tile.hpp"
#include "hex_loader.hpp"

namespace hex
{
	HexTile::HexTile(const variant& value)
		: id_(),
		  name_(),
		  str_(value["string"].as_string()),
		  editor_group_(value["editor_group"].as_string_default()),
		  editor_name_(value["editor_name"].as_string_default()),
		  editor_image_(value["editor_image"].as_string_default()),
		  symbol_image_(value["symbol_image"].as_string_default()),
		  icon_image_(value["icon_image"].as_string_default()),
		  help_topic_text_(value["help_topic_text"].as_string_default()),
		  hidden_(value["hidden"].as_bool(false)),
		  recruit_onto_(value["recruit_onto"].as_bool(false)),
		  hide_help_(value["hide_help"].as_bool(false)),
		  submerge_(value["submerge"].as_float(0.0f)),
		  image_rect_(),
		  symbol_image_filename_()
	{
		//gives_income
		//heals
		//recruit_from
		//unit_height_adjust
		//mvt_alias

		if(!symbol_image_.empty()) {
			symbol_image_filename_ = get_terrain_data(symbol_image_, &image_rect_);
		} else if(!editor_image_.empty()) {
			symbol_image_filename_ = get_terrain_data(editor_image_, &image_rect_);
		}
		if(symbol_image_filename_.empty()) {
			LOG_WARN("No image available for tile: " << (id_.empty() ? str_ : id_));
		}
	}

	HexTilePtr HexTile::create(const variant& value)
	{
		return HexTilePtr(new HexTile(value));
	}

	void HexTile::surrenderReferences(GarbageCollector* collector)
	{
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(HexTile)
		DEFINE_FIELD(image_rect, "[int,int,int,int]")
			return obj.image_rect_.write();
		DEFINE_FIELD(symbol_image_file, "string")
			return variant(obj.symbol_image_filename_);
		DEFINE_FIELD(string, "string")
			return variant(obj.getString());
	END_DEFINE_CALLABLE(HexTile)
}
