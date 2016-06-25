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

#include "xhtml_block_box.hpp"
#include "xhtml_layout_engine.hpp"

namespace xhtml
{
	using namespace css;

	BlockBox::BlockBox(const BoxPtr& parent, const StyleNodePtr& node, const RootBoxPtr& root)
		: Box(BoxId::BLOCK, parent, node, root),
		  child_height_(0)
	{
	}

	std::string BlockBox::toString() const
	{
		std::ostringstream ss;
		ss << "BlockBox: " << getDimensions().content_ << (isFloat() ? " floating" : "");
		NodePtr node = getNode();
		if(node != nullptr && node->id() == NodeId::ELEMENT) {
			ss << " <" << node->getTag() << ">";
		}
		ss << " " << getOffset();
		return ss.str();
	}

	void BlockBox::handleLayout(LayoutEngine& eng, const Dimensions& containing)
	{
		if(isReplaceable()) {
			layoutChildren(eng);
		} else {
			layoutChildren(eng);
			layoutHeight(containing);
		}

		if(isFloat()) {
			FixedPoint top = 0;
			const FixedPoint lh = getStyleNode()->getHeight()->isAuto() ? getLineHeight() : getStyleNode()->getHeight()->getLength().compute(containing.content_.height);
			const FixedPoint box_w = getDimensions().content_.width;

			FixedPoint y = 0;
			FixedPoint x = 0;

			FixedPoint y1 = y + getOffset().y;
			FixedPoint left = getStyleNode()->getFloat() == Float::LEFT ? eng.getXAtPosition(y1, y1 + lh) + x : eng.getX2AtPosition(y1, y1 + lh);
			FixedPoint w = eng.getWidthAtPosition(y1, y1 + lh, containing.content_.width);
			bool placed = false;
			while(!placed) {
				if(w >= box_w) {
					left = left - (getStyleNode()->getFloat() == Float::LEFT ? x : box_w) + getMBPLeft();
					top = y + getMBPTop() + containing.content_.height;
					placed = true;
				} else {
					y += lh;
					y1 = y + getOffset().y;
					left = getStyleNode()->getFloat() == Float::LEFT ? eng.getXAtPosition(y1, y1 + lh) + x : eng.getX2AtPosition(y1, y1 + lh);
					w = eng.getWidthAtPosition(y1, y1 + lh, containing.content_.width);
				}
			}
			setContentX(left);
			setContentY(top);
		}
	}

	void BlockBox::handlePreChildLayout(LayoutEngine& eng, const Dimensions& containing)
	{
		if(isReplaceable()) {
			NodePtr node = getNode();
			calculateHorzMPB(containing.content_.width);
			setContentRect(Rect(0, 0, node->getDimensions().w() * LayoutEngine::getFixedPointScale(), node->getDimensions().h() * LayoutEngine::getFixedPointScale()));
			auto css_width = getStyleNode()->getWidth();
			auto css_height = getStyleNode()->getHeight();
			if(!css_width->isAuto()) {
				setContentWidth(css_width->getLength().compute(containing.content_.width));
			}
			if(!css_height->isAuto()) {
				setContentHeight(css_height->getLength().compute(containing.content_.height));
			}
			if(!css_width->isAuto() || !css_height->isAuto()) {
				node->setDimensions(rect(0, 0, getDimensions().content_.width/LayoutEngine::getFixedPointScale(), getDimensions().content_.height/LayoutEngine::getFixedPointScale()));
			}
		} else {
			layoutWidth(containing);

			//if(!getStyleNode->getHeight()->).isAuto()) {
			//	setContentHeight(getStyleNode->getHeight()->).getLength().compute(containing.content_.height));
			//}
		}

		calculateVertMPB(containing.content_.height);

		FixedPoint left = getMBPLeft();
		FixedPoint top = getMBPTop() + containing.content_.height;
		if(getStyleNode()->getPosition() == Position::FIXED) {
			const FixedPoint containing_width = containing.content_.width;
			const FixedPoint containing_height = containing.content_.height;
			left = containing.content_.x;
			if(!getStyleNode()->getLeft()->isAuto()) {
				left = getStyleNode()->getLeft()->getLength().compute(containing_width);
			}
			top = containing.content_.y;
			if(!getStyleNode()->getTop()->isAuto()) {
				top = getStyleNode()->getTop()->getLength().compute(containing_height);
			}
		}
	
		setContentX(left);
		setContentY(top);
	}

	void BlockBox::handlePostChildLayout(LayoutEngine& eng, BoxPtr child)
	{
		// Called after every child is laid out.
		setContentHeight(child->getTop() + child->getHeight() + child->getMBPBottom());
	}

	void BlockBox::layoutWidth(const Dimensions& containing)
	{
		RenderContext& ctx = RenderContext::get();
		const FixedPoint containing_width = containing.content_.width;

		const auto& css_width = getStyleNode()->getWidth();
		FixedPoint width = 0;
		if(!css_width->isAuto()) {
			width = css_width->getLength().compute(containing_width);
			setContentWidth(width);
		}

		calculateHorzMPB(containing_width);
		const auto& css_margin_left = getStyleNode()->getMargin()[static_cast<int>(Side::LEFT)];
		const auto& css_margin_right = getStyleNode()->getMargin()[static_cast<int>(Side::RIGHT)];

		FixedPoint total = getMBPWidth() + width;
			
		if(!css_width->isAuto() && total > containing.content_.width) {
			if(css_margin_left->isAuto()) {
				setMarginLeft(0);
			}
			if(css_margin_right->isAuto()) {
				setMarginRight(0);
			}
		}

		// If negative is overflow.
		FixedPoint underflow = containing.content_.width - total;

		if(css_width->isAuto()) {
			if(css_margin_left->isAuto()) {
				setMarginLeft(0);
			}
			if(css_margin_right->isAuto()) {
				setMarginRight(0);
			}
			if(underflow >= 0) {
				width = underflow;
			} else {
				width = 0;
				const auto rmargin = css_margin_right->getLength().compute(containing_width);
				setMarginRight(rmargin + underflow);
			}
			setContentWidth(width);
		} else if(!css_margin_left->isAuto() && !css_margin_right->isAuto()) {
			setMarginRight(getDimensions().margin_.right + underflow);
		} else if(!css_margin_left->isAuto() && css_margin_right->isAuto()) {
			setMarginRight(underflow);
		} else if(css_margin_left->isAuto() && !css_margin_right->isAuto()) {
			setMarginLeft(underflow);
		} else if(css_margin_left->isAuto() && css_margin_right->isAuto()) {
			setMarginLeft(underflow / 2);
			setMarginRight(underflow / 2);
		} 

		if(isFloat()) {
			setMarginLeft(0);
			setMarginRight(0);
		}
	}

	void BlockBox::layoutChildren(LayoutEngine& eng)
	{
		// XXX we should add collapsible margins to children here.
		child_height_ = 0;
		FixedPoint width = 0;

		for(auto& child : getChildren()) {
			if(!child->isFloat()) {
				child_height_ = std::max(child_height_, child->getHeight() + child->getTop() + child->getMBPBottom());
				width = std::max(width, child->getLeft() + child->getWidth() + child->getMBPWidth());
			}
		}
		if(getStyleNode()->getWidth()->isAuto() && !isReplaceable()) {
			//setContentWidth(width);
		}
		auto css_height = getStyleNode()->getHeight();
		if(css_height->isAuto() && !isReplaceable()) {
			setContentHeight(child_height_);
		}
	}
	
	void BlockBox::layoutHeight(const Dimensions& containing)
	{
		// a set height value overrides the calculated value.
		if(!getStyleNode()->getHeight()->isAuto()) {
			FixedPoint h = getStyleNode()->getHeight()->getLength().compute(containing.content_.height);
			if(h > child_height_) {
				/* apply overflow properties */
			}
			setContentHeight(h);
		}
		// XXX deal with min-height and max-height
		//auto& min_h = getCssMinHeight().compute(containing.content_.height);
		//if(getHeight() < min_h) {
		//	setContentHeight(min_h);
		//}
		//auto& css_max_h = getCssMaxHeight();
		//if(!css_max_h.isNone()) {
		//	FixedPoint max_h = css_max_h.getValue<Length>().compute(containing.content_.height);
		//	if(getHeight() > max_h) {
		//		setContentHeight(max_h);
		//	}
		//}
	}

	void BlockBox::handleRender(const KRE::SceneTreePtr& scene_tree, const point& offset) const
	{
		if(isReplaceable()) {
			scene_tree->addObject(getNode()->getRenderable());
		}
	}

}
