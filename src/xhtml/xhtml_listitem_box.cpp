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

#include <iomanip>

#include "Blittable.hpp"

#include "to_roman.hpp"
#include "utf8_to_codepoint.hpp"

#include "xhtml_block_box.hpp"
#include "xhtml_listitem_box.hpp"
#include "xhtml_layout_engine.hpp"

namespace xhtml
{
	using namespace css;

	namespace 
	{
		const char32_t marker_disc = 0x2022;
		const char32_t marker_circle = 0x25e6;
		const char32_t marker_square = 0x25a0;
		const char32_t marker_lower_greek = 0x03b1 - 1;
		const char32_t marker_lower_greek_end = 0x03c9;
		const char32_t marker_lower_latin = 0x0061 - 1;
		const char32_t marker_lower_latin_end = 0x007a;
		const char32_t marker_upper_latin = 0x0041 - 1;
		const char32_t marker_upper_latin_end = 0x005A;
		const char32_t marker_armenian = 0x0531 - 1;
		const char32_t marker_armenian_end = 0x0556;
		const char32_t marker_georgian = 0x10d0 - 1;
		const char32_t marker_georgian_end = 0x10f6;
	}

	ListItemBox::ListItemBox(BoxPtr parent, StyleNodePtr node, int count)
		: Box(BoxId::LIST_ITEM, parent, node),
		  count_(count),
		  marker_(utils::codepoint_to_utf8(marker_disc))
	{
		addChild(std::make_shared<BlockBox>(parent, node));
	}

	std::string ListItemBox::toString() const 
	{
		std::ostringstream ss;
		ss << "ListItemBox: " << getDimensions().content_ << (isFloat() ? " floating" : "");
		return ss.str();
	}

	void ListItemBox::handleLayout(LayoutEngine& eng, const Dimensions& containing) 
	{
		auto lst = getStyleNode()->getListStyleType();
		switch(lst) {
			case ListStyleType::DISC: /* is the default */ break;
			case ListStyleType::CIRCLE:
				marker_ = utils::codepoint_to_utf8(marker_circle);
				break;
			case ListStyleType::SQUARE:
				marker_ = utils::codepoint_to_utf8(marker_square);
				break;
			case ListStyleType::DECIMAL: {
				std::ostringstream ss;
				ss << std::dec << count_ << ".";
				marker_ = ss.str();
				break;
			}
			case ListStyleType::DECIMAL_LEADING_ZERO: {
				std::ostringstream ss;
				ss << std::dec << std::setfill('0') << std::setw(2) << count_ << ".";
				marker_ = ss.str();
				break;
			}
			case ListStyleType::LOWER_ROMAN:
				if(count_ < 4000) {
					marker_ = to_roman(count_, true) + ".";
				}
				break;
			case ListStyleType::UPPER_ROMAN:
				if(count_ < 4000) {
					marker_ = to_roman(count_, false) + ".";
				}
				break;
			case ListStyleType::LOWER_GREEK:
				if(count_ <= (marker_lower_greek_end - marker_lower_greek + 1)) {
					marker_ = utils::codepoint_to_utf8(marker_lower_greek + count_) + ".";
				}
				break;
			case ListStyleType::LOWER_ALPHA:
			case ListStyleType::LOWER_LATIN:
				if(count_ <= (marker_lower_latin_end - marker_lower_latin + 1)) {
					marker_ = utils::codepoint_to_utf8(marker_lower_latin + count_) + ".";
				}
				break;
			case ListStyleType::UPPER_ALPHA:
			case ListStyleType::UPPER_LATIN:
				if(count_ <= (marker_upper_latin_end - marker_upper_latin + 1)) {
					marker_ = utils::codepoint_to_utf8(marker_upper_latin + count_) + ".";
				}
				break;
			case ListStyleType::ARMENIAN:
				if(count_ <= (marker_armenian_end - marker_armenian + 1)) {
					marker_ = utils::codepoint_to_utf8(marker_armenian + count_) + ".";
				}
				break;
			case ListStyleType::GEORGIAN:
				if(count_ <= (marker_georgian_end - marker_georgian + 1)) {
					marker_ = utils::codepoint_to_utf8(marker_georgian + count_) + ".";
				}
				break;
			case ListStyleType::NONE:
			default: 
				marker_.clear();
				break;
		}

		FixedPoint top = getMBPTop() + containing.content_.height;
		FixedPoint left = getMBPLeft();

		if(isFloat()) {
			// XXX fixme to use a more intelligent approach than iterating every pixel!
			const FixedPoint lh = 65536;//getDimensions().content_.height;
			const FixedPoint box_w = getDimensions().content_.width;

			FixedPoint y = getMBPTop();
			FixedPoint x = getMBPLeft();

			FixedPoint y1 = y + getOffset().y;
			left = getStyleNode()->getFloat() == Float::LEFT ? eng.getXAtPosition(y1, y1 + lh) + x : eng.getX2AtPosition(y1, y1 + lh);
			FixedPoint w = eng.getWidthAtPosition(y1, y1 + lh, containing.content_.width);
			bool placed = false;
			while(!placed) {
				if(w >= box_w) {
					left = left - (getStyleNode()->getFloat() == Float::LEFT ? x : box_w);
					top = y;
					placed = true;
				} else {
					y += lh;
					y1 = y + getOffset().y;
					left = getStyleNode()->getFloat() == Float::LEFT ? eng.getXAtPosition(y1, y1 + lh) + x : eng.getX2AtPosition(y1, y1 + lh);
					w = eng.getWidthAtPosition(y1, y1 + lh, containing.content_.width);
				}
			}
		}

		setContentX(left);
		setContentY(top);

		auto& css_height = getStyleNode()->getHeight();
		if(!css_height->isAuto()) {
			FixedPoint h = css_height->getLength().compute(containing.content_.height);
			//if(h > child_height_) {
			//	/* apply overflow properties */
			//}
			setContentHeight(h);
		}

	}

	void ListItemBox::handlePreChildLayout(LayoutEngine& eng, const Dimensions& containing)
	{
		FixedPoint containing_width = containing.content_.width;

		calculateHorzMPB(containing_width);
		calculateVertMPB(containing.content_.height);

		const auto& css_width = getStyleNode()->getWidth();
		FixedPoint width = 0;
		if(!css_width->isAuto()) {
			width = css_width->getLength().compute(containing_width);
		} else {
			width = containing_width;
		}

		setContentWidth(width);
		setContentHeight(0);
	}

	void ListItemBox::handlePostChildLayout(LayoutEngine& eng, BoxPtr child) 
	{
		setContentHeight(getHeight() + child->getHeight() + child->getMBPBottom());
	}

	void ListItemBox::handleRender(DisplayListPtr display_list, const point& offset) const 
	{
		// XXX should figure out if there is a cleaner way of doing this, basically we want the list marker to be offset by the 
		// content's first child's position.
		auto y = getBaselineOffset();
		if(getChildren().size() > 0) {
			if(getChildren().front()->getChildren().size() > 0) {
				y = getChildren().front()->getChildren().front()->getBaselineOffset();
			}
		}
		auto& fnt = getStyleNode()->getFont();

		auto& img = getStyleNode()->getListStyleImage();
		if(getStyleNode()->getListStyleImage() != nullptr) {
			// There are some other calculations we should do here if there is an
			// intrinsic ration to use.
			int em = static_cast<int>(fnt->getFontSize() / 72.0f * static_cast<float>(RenderContext::get().getDPI()));
			auto tex = img->getTexture(em, em);
			if(tex != nullptr) {
				auto rend = std::make_shared<KRE::Blittable>(tex);
				rend->setCentre(KRE::Blittable::Centre::BOTTOM_LEFT);
				rend->setCentreCoords(pointf(0.0f, y / LayoutEngine::getFixedPointScaleFloat()));
				display_list->addRenderable(rend);
			}
		} else {
			auto path = fnt->getGlyphPath(marker_);
			std::vector<point> new_path;
			FixedPoint path_width = path.back().x - path.front().x + fnt->calculateCharAdvance(' ');

			for(auto& p : path) {
				new_path.emplace_back(p.x + offset.x - 5 - path_width, p.y + offset.y + y);
			}
			auto fontr = fnt->createRenderableFromPath(nullptr, marker_, new_path);
			fontr->setColorPointer(getStyleNode()->getColor());
			display_list->addRenderable(fontr);
		}
	}

}
