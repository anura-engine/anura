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

#include "xhtml_inline_block_box.hpp"
#include "xhtml_layout_engine.hpp"

namespace xhtml
{
	using namespace css;

	InlineBlockBox::InlineBlockBox(BoxPtr parent, StyleNodePtr node)
		: Box(BoxId::INLINE_BLOCK, parent, node),
		  multiline_(false)
	{
	}

	std::string InlineBlockBox::toString() const
	{
		std::ostringstream ss;
		ss << "InlineBlockBox: " << getDimensions().content_;
		if(isEOL()) {
			ss << " ; end-of-line";
		}
		return ss.str();
	}

	void InlineBlockBox::handleLayout(LayoutEngine& eng, const Dimensions& containing)
	{
		layoutChildren(eng);
		layoutHeight(containing);

		if(!isReplaceable()) {
			if(getChildren().size() > 1) {
				multiline_ = true;
			} else if(!getChildren().empty() && getChildren().front()->id() == BoxId::LINE && getChildren().front()->getChildren().size() > 1) {
				multiline_ = true;
			}
		}

		if(isReplaceable()) {
			NodePtr node = getNode();
			node->setDimensions(rect(0, 0, getWidth() / LayoutEngine::getFixedPointScale(), getHeight() / LayoutEngine::getFixedPointScale()));
		}
	}

	void InlineBlockBox::layoutWidth(const Dimensions& containing)
	{
		RenderContext& ctx = RenderContext::get();
		const FixedPoint containing_width = containing.content_.width;

		auto css_width = getStyleNode()->getWidth();
		FixedPoint width = 0;
		if(!css_width->isAuto()) {
			width = css_width->getLength().compute(containing_width);
			setContentWidth(width);
		}

		calculateHorzMPB(containing_width);
		auto css_margin_left = getStyleNode()->getMargin()[static_cast<int>(Side::LEFT)];
		auto css_margin_right = getStyleNode()->getMargin()[static_cast<int>(Side::RIGHT)];

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
			setContentWidth(underflow);
		}
	}

	void InlineBlockBox::layoutChildren(LayoutEngine& eng)
	{
		// calculate height if not specified from children.
		// same for width?
		if(!getChildren().empty()) {
			FixedPoint width = 0;
			for(auto& child : getChildren()) {
				width = std::max(width, child->getLeft() + child->getWidth() + child->getMBPWidth());
			}
			if(getStyleNode()->getWidth()->isAuto()) {
				setContentWidth(width);
			}
		}
	}

	void InlineBlockBox::handlePostChildLayout(LayoutEngine& eng, BoxPtr child)
	{
		// Called after every child is laid out.
		setContentHeight(getHeight() + child->getHeight() + child->getMBPBottom());
	}

	void InlineBlockBox::handlePreChildLayout2(LayoutEngine& eng, const Dimensions& containing)
	{
		if(!getChildren().empty() || !isReplaceable()) {
			setContentHeight(0);
		} else if(isReplaceable()) {
			NodePtr node = getNode();
			const rect& r = node->getDimensions();
			setContentWidth(r.w() * LayoutEngine::getFixedPointScale());
			setContentHeight(r.h() * LayoutEngine::getFixedPointScale());
		}
	}

	void InlineBlockBox::layoutHeight(const Dimensions& containing)
	{
		RenderContext& ctx = RenderContext::get();
		// a set height value overrides the calculated value.
		auto& css_height = getStyleNode()->getHeight();
		if(!css_height->isAuto()) {
			setContentHeight(css_height->getLength().compute(containing.content_.height));
		}
		// XXX deal with min-height and max-height
		//auto min_h = ctx.getComputedValue(Property::MIN_HEIGHT).getValue<Length>().compute(containing.content_.height);
		//if(getDimensions().content_.height() < min_h) {
		//	setContentHeight(min_h);
		//}
		//auto css_max_h = ctx.getComputedValue(Property::MAX_HEIGHT).getValue<Height>();
		//if(!css_max_h.isNone()) {
		//	FixedPoint max_h = css_max_h.getValue<Length>().compute(containing.content_.height);
		//	if(getDimensions().content_.height() > max_h) {
		//		setContentHeight(max_h);
		//	}
		//}
	}

	void InlineBlockBox::handlePreChildLayout(LayoutEngine& eng, const Dimensions& containing)
	{
		layoutWidth(containing);
		calculateVertMPB(containing.content_.height);
	}

	void InlineBlockBox::handleRender(DisplayListPtr display_list, const point& offset) const
	{
		NodePtr node = getNode();
		if(node != nullptr && node->isReplaced()) {
			auto r = node->getRenderable();
			if(r == nullptr) {
				LOG_ERROR("No renderable returned for repalced element: " << node->toString());
			} else {
				r->setPosition(glm::vec3(static_cast<float>(offset.x)/LayoutEngine::getFixedPointScaleFloat(),
					static_cast<float>(offset.y)/LayoutEngine::getFixedPointScaleFloat(),
					0.0f));
				display_list->addRenderable(r);
			}
		}
	}
}
