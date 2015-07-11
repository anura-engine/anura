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

#include "solid_renderable.hpp"
#include "xhtml_line_box.hpp"
#include "xhtml_layout_engine.hpp"
#include "xhtml_text_box.hpp"

namespace xhtml
{
	LineBox::LineBox(BoxPtr parent, const point& cursor)
		: Box(BoxId::LINE, parent, nullptr),
		  cursor_(cursor)
	{
	}

	std::string LineBox::toString() const
	{
		std::ostringstream ss;
		ss << "LineBox: " << getDimensions().content_;
		return ss.str();
	}

	void LineBox::reflowChildren(LayoutEngine& eng, const Dimensions& containing)
	{
		FixedPoint lh = !getChildren().empty() ? getChildren().front()->getLineHeight() : 0;
		FixedPoint y1 = cursor_.y + getOffset().y;
		cursor_.x = eng.getXAtPosition(y1, y1 + lh);
		FixedPoint width = eng.getWidthAtPosition(y1, y1 + lh, containing.content_.width);

		auto children = getChildren();
		clearChildren();
		for(auto& child : children) {
			if(child->id() == BoxId::TEXT) {
				lh = std::max(lh, child->getLineHeight());
				auto txt = std::dynamic_pointer_cast<TextBox>(child);
				ASSERT_LOG(txt != nullptr, "Something went wrong child box with id TEXT couldn't be cast to Text box");
				TextPtr tnode = txt->getText();
				auto it = tnode->begin();

				std::shared_ptr<TextBox> last_txt = txt;

				while(it != tnode->end()) {
					it = txt->reflow(eng, cursor_, it);

					LinePtr line = txt->getLine();
					if(line != nullptr && !line->line.empty()) {
						txt->setContentX(cursor_.x);
						txt->setContentY(cursor_.y);

						FixedPoint x_inc = txt->getWidth() + txt->getMBPWidth();
						cursor_.x += x_inc;
						width -= x_inc;
						addChild(txt);
						last_txt = txt;
						txt.reset();
					}
			
					if((line != nullptr && line->is_end_line) || width < 0) {
						cursor_.y += lh;
						y1 = cursor_.y + getOffset().y;
						cursor_.x = eng.getXAtPosition(y1, y1 + lh);
						width = eng.getWidthAtPosition(y1, y1 + lh, containing.content_.width);
						lh = 0;
						last_txt->setEOL(true);
					}

					if(it != tnode->end()) {
						txt = std::make_shared<TextBox>(*last_txt);
						txt->setEOL(false); // ugh -- inheriting the copied value of the end of line flag is not a good idea.
						lh = std::max(lh, txt->getLineHeight());
					}
				}
			} else {
				// XXX fixme
				const FixedPoint x_inc = child->getWidth() + child->getMBPWidth();

				if(width <= x_inc) {
					if(!getChildren().empty()) {
						getChildren().back()->setEOL(true);
					}
					cursor_.y += std::max(lh, child->getHeight() + child->getMBPHeight());
					y1 = cursor_.y + getOffset().y;
					cursor_.x = eng.getXAtPosition(y1, y1 + lh);
					width = eng.getWidthAtPosition(y1, y1 + lh, containing.content_.width);
				}			
				child->setContentX(cursor_.x);
				child->setContentY(cursor_.y);
				addChild(child);
				width -= x_inc;
				cursor_.x += x_inc;
				if(child->isMultiline()) {
					cursor_.y += child->getHeight() + child->getMBPHeight();
					y1 = cursor_.y + getOffset().y;
					cursor_.x = eng.getXAtPosition(y1, y1 + lh);
					width = eng.getWidthAtPosition(y1, y1 + lh, containing.content_.width);
					child->setEOL(true);
				} else if(width <= 0) {
					cursor_.y += std::max(lh, child->getHeight());
					y1 = cursor_.y + getOffset().y;
					cursor_.x = eng.getXAtPosition(y1, y1 + lh);
					width = eng.getWidthAtPosition(y1, y1 + lh, containing.content_.width);
					getChildren().back()->setEOL(true);
				}
			}
		}

		if(!getChildren().empty()) {
			getChildren().back()->setEOL(true);
		}
	}

	void LineBox::handlePreChildLayout2(LayoutEngine& eng, const Dimensions& containing)
	{
		setContentX(getMBPLeft());
		setContentY(getMBPTop() + containing.content_.height);

		setContentWidth(containing.content_.width);
		reflowChildren(eng, containing);
	}

	void LineBox::handleLayout(LayoutEngine& eng, const Dimensions& containing)
	{
		// adjust heights of lines for tallest item
		bool start_of_line = true;
		FixedPoint line_height = 0;
		std::vector<BoxPtr> current_line;
		for(auto& child : getChildren()) {
			if(start_of_line) {
				start_of_line = false;
				current_line.clear();
				line_height = 0;
			}
			if(child->id() == BoxId::TEXT) {
				line_height = std::max(line_height, child->getLineHeight());
			} else {
				line_height = std::max(line_height, child->getHeight() + child->getMBPHeight());
			}
			current_line.emplace_back(child);
			if(child->isEOL()) {
				for(auto& line_child : current_line) {
					line_child->setContentHeight(line_height);
				}
				start_of_line = true;
			}
		}

		// Our children should already be set at this point.
		// we want to compute our own width/height based on our children and set the 
		// children's x/y
		FixedPoint height = !getChildren().empty() 
			? getChildren().back()->getHeight() + getChildren().back()->getMBPHeight() + getChildren().back()->getTop() 
			: 0;
		FixedPoint width = 0;			

		// compute our width/height
		for(auto& child : getChildren()) {
			width = std::max(width, child->getLeft() + child->getWidth() + getMBPWidth());
		}
		
		setContentWidth(width);
		setContentHeight(height);

		// computer&set children X/Y offsets
		for(auto& child : getChildren()) {
			FixedPoint child_y = child->getDimensions().content_.y;
			// XXX we should implement this fully.
			auto& vertical_align = child->getStyleNode()->getVerticalAlign();
			css::CssVerticalAlign va = vertical_align->getAlign();
			switch(va) {
				case css::CssVerticalAlign::BASELINE:
					// Align the baseline of the box with the baseline of the parent box. 
					// If the box does not have a baseline, align the bottom margin edge 
					// with the parent's baseline.
					child_y += child->getBaselineOffset();
					break;
				case css::CssVerticalAlign::MIDDLE:
					// Align the vertical midpoint of the box with the baseline of the 
					// parent box plus half the x-height of the parent.
					child_y += height / 2;
					break;
				case css::CssVerticalAlign::BOTTOM:
					// Align the bottom of the aligned subtree with the bottom of the line box.
					child_y += child->getBottomOffset();
					break;
				case css::CssVerticalAlign::SUB:
					// Lower the baseline of the box to the proper position for subscripts of the 
					// parent's box. (This value has no effect on the font size of the element's text.)
				case css::CssVerticalAlign::SUPER:
					// Raise the baseline of the box to the proper position for superscripts of the 
					// parent's box. (This value has no effect on the font size of the element's text.)
				case css::CssVerticalAlign::TOP:
					// Align the top of the aligned subtree with the top of the line box.
				case css::CssVerticalAlign::TEXT_TOP:
					// Align the top of the box with the top of the parent's content area
				case css::CssVerticalAlign::TEXT_BOTTOM:
					// Align the bottom of the box with the bottom of the parent's content area
					break;
				case css::CssVerticalAlign::LENGTH: {
					// Offset align by length value. Percentages reference the line-height of the element.
					FixedPoint len = vertical_align->getLength().compute(child->getLineHeight());
					// 0 for len is the baseline.
					child_y += child->getBaselineOffset() - len;
				}
				default:  break;
			}

			child->setContentY(child_y);
		}
	}

	void LineBox::postParentLayout(LayoutEngine& eng, const Dimensions& containing)
	{
		const FixedPoint containing_width = containing.content_.width;
		const css::TextAlign ta = getParent()->getStyleNode()->getTextAlign();

		// computer&set children X offsets
		auto last_child = getChildren().cend()-1;
		for(auto it = getChildren().cbegin(); it != getChildren().cend(); ++it) {
			auto& child = *it;
			FixedPoint child_x = child->getLeft();
			switch(ta) {
				case css::TextAlign::RIGHT:		child_x = containing_width - child->getWidth(); break;
				case css::TextAlign::CENTER:	child_x = (containing_width - child->getWidth() - child_x) / 2; break;
				case css::TextAlign::JUSTIFY:	
					if(it != last_child) {
						child->justify(containing_width); 
					}
					break;
				case css::TextAlign::NORMAL:	
					if(getParent()->getStyleNode()->getDirection() == css::Direction::RTL) {
						child_x = containing_width - child->getWidth();
					}
					break;
				case css::TextAlign::LEFT:
				default:
					// use default value.
					break;
			}
			child->setContentX(child_x);
		}
	}

	void LineBox::handleRender(DisplayListPtr display_list, const point& offset) const
	{
		// do nothing
	}

	void LineBox::handleRenderBorder(DisplayListPtr display_list, const point& offset) const
	{
	}
}
