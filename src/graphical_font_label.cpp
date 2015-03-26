/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#include "Canvas.hpp"
#include "ModelMatrixScope.hpp"

#include "graphical_font_label.hpp"

namespace gui 
{
	GraphicalFontLabel::GraphicalFontLabel(const std::string& text, const std::string& font, int size)
	  : text_(text), 
	    font_(GraphicalFont::get(font)), 
		size_(size)
	{
		setEnvironment();
		ASSERT_LOG(font_.get(), "UNKNOWN FONT: " << font);
		resetTextDimensions();
	}

	GraphicalFontLabel::GraphicalFontLabel(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v,e),
		  text_(v["text"].as_string_default("TEXT")), 
	      font_(GraphicalFont::get(v.has_key("font") ? v["font"].as_string() : "door_label")), 
		  size_(v["size"].as_int(2))
	{
		ASSERT_LOG(font_.get(), "UNKNOWN FONT: " << v["font"].as_string());
		resetTextDimensions();
	}

	void GraphicalFontLabel::handleDraw() const
	{
		auto tr = KRE::Canvas::getCurrentTranslation();
		KRE::ModelManager2D mm(static_cast<int>(tr.x), static_cast<int>(tr.y));
		font_->draw(x(), y(), text_, size_);
	}

	void GraphicalFontLabel::resetTextDimensions()
	{
		rect dim = font_->dimensions(text_, size_);
		Widget::setDim(dim.w(), dim.h());
	}

	void GraphicalFontLabel::setText(const std::string& text)
	{
		text_ = text;
		resetTextDimensions();
	}

	BEGIN_DEFINE_CALLABLE(GraphicalFontLabel, Widget)
		DEFINE_FIELD(text, "string")
			return variant(obj.text_);
		DEFINE_SET_FIELD
			obj.setText(value.as_string());

		DEFINE_FIELD(font, "string")
			return variant(obj.font_->id());
		DEFINE_SET_FIELD
			obj.font_ = GraphicalFont::get(value.as_string());
			ASSERT_LOG(obj.font_.get(), "UNKNOWN FONT: " << value.as_string());
			obj.resetTextDimensions();
			
		DEFINE_FIELD(size, "int")
			return variant(obj.size_);
		DEFINE_SET_FIELD
			obj.size_ = value.as_int();
			obj.resetTextDimensions();
	END_DEFINE_CALLABLE(GraphicalFontLabel)
}
