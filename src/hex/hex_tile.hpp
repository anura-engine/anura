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

#include <string>

#include "hex_fwd.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "geometry.hpp"

namespace hex
{
	class HexTile : public game_logic::FormulaCallable
	{
	public:
		HexTile(const variant& value);
		const std::string& getId() const { return id_; }
		const std::string& geName() const { return name_; }
		const std::string& getString() const { return str_; }

		const std::string& getEditorGroup() const { return str_; }
		const std::string& getEditorName() const { return str_; }
		const std::string& getSymbolImage() const { return str_; }
		const std::string& getIconImage() const { return str_; }
		const std::string& getHelpTopicText() const { return str_; }
		bool isHidden() const { return hidden_; }
		bool canRecruitOnto() const { return recruit_onto_; }
		bool isHelpHidden() const { return hide_help_; }

		void setId(const std::string& id) { id_ = id; }
		void setName(const std::string& name) { name_ = name; }
		void setEditorGroup(const std::string& editor_group) { editor_group_ = editor_group; }
		void setEditorName(const std::string& editor_name) { editor_name_ = editor_name; }
		void setEditorImage(const std::string& editor_image) { editor_image_ = editor_image; }
		void setSymbolImage(const std::string& symbol_image) { symbol_image_ = symbol_image; }
		void setIconImage(const std::string& icon_image) { icon_image_ = icon_image; }
		void setHelpTopicText(const std::string& help_topic_text) { help_topic_text_ = help_topic_text; }
		void setHidden(bool hidden=true) { hidden_ = hidden; }
		void setRecruitable(bool recruitable=true) { recruit_onto_ = recruitable; }
		void setHideHelp(bool hidden=true) { hide_help_ = hidden; }
		void setSubmerge(float submerge) { submerge_ = submerge; }

		void surrenderReferences(GarbageCollector* collector) override;

		static HexTilePtr create(const variant& value);
	private:
		DECLARE_CALLABLE(HexTile);

		std::string id_;
		std::string name_;
		std::string str_;
		std::string editor_group_;
		std::string editor_name_;
		std::string editor_image_;
		// minimap image
		std::string symbol_image_;
		// icon image.
		std::string icon_image_;
		std::string help_topic_text_;
		bool hidden_;
		bool recruit_onto_;
		bool hide_help_;
		float submerge_;
		rect image_rect_;
		std::string symbol_image_filename_;
	};
}
