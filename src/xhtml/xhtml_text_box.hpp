/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include "xhtml_box.hpp"
#include "xhtml_text_node.hpp"

namespace xhtml
{
	class TextBox : public Box
	{
	public:
		TextBox(BoxPtr parent, StyleNodePtr node);
		std::string toString() const override;
		Text::iterator reflow(LayoutEngine& eng, point& cursor, Text::iterator it);
		Text::iterator begin() { return it_; }
		Text::iterator end();
		LinePtr getLine() const { return line_; }
		TextPtr getText() const { return txt_; }
		void justify(FixedPoint containing_width) override;
	private:
		void handleLayout(LayoutEngine& eng, const Dimensions& containing) override;
		void handleRender(DisplayListPtr display_list, const point& offset) const override;
		void handleRenderBackground(DisplayListPtr display_list, const point& offset) const override;
		void handleRenderBorder(DisplayListPtr display_list, const point& offset) const override;
		void handleRenderShadow(DisplayListPtr display_list, const point& offset, KRE::FontRenderablePtr fontr, float w, float h) const;
		FixedPoint calculateWidth() const;
		LinePtr line_;
		TextPtr txt_;
		Text::iterator it_;
		FixedPoint justification_;

		// for text shadows
		struct Shadow {
			Shadow() : x_offset(0), y_offset(0), blur(0), color() {}
			Shadow(float xo, float yo, float br, const KRE::ColorPtr& c) : x_offset(xo), y_offset(yo), blur(br), color(c) {}
			float x_offset;
			float y_offset;
			float blur;
			KRE::ColorPtr color;
		};
		std::vector<Shadow> shadows_;
	};
}
