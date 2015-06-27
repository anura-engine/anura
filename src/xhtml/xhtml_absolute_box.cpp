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

#include "xhtml_absolute_box.hpp"
#include "xhtml_layout_engine.hpp"

namespace xhtml
{
	using namespace css;

	AbsoluteBox::AbsoluteBox(BoxPtr parent, StyleNodePtr node)
		: Box(BoxId::ABSOLUTE, parent, node),
		  container_()
	{
	}

	std::string AbsoluteBox::toString() const
	{
		std::ostringstream ss;
		ss << "AbsoluteBox: " << getDimensions().content_;
		return ss.str();
	}

	void AbsoluteBox::handlePreChildLayout(LayoutEngine& eng, const Dimensions& containing)
	{
		Rect container = containing.content_;

		// Find the first ancestor with non-static position
		auto parent = getParent();
		if(parent != nullptr && !parent->ancestralTraverse([&container](const ConstBoxPtr& box) {
			if(box->getStyleNode()->getPosition() != Position::STATIC) {
				container = box->getDimensions().content_;
				return true;
			}
			return false;
		})) {
			// couldn't find anything use the layout engine dimensions
			container = eng.getDimensions().content_;
		}
		container_ = container;

		// we expect top/left and either bottom/right or width/height
		// if the appropriate thing isn't set then we use the parent container dimensions.
		RenderContext& ctx = RenderContext::get();
		const FixedPoint containing_width = container.width;
		const FixedPoint containing_height = container.height;
		
		FixedPoint left = container.x;
		if(!getStyleNode()->getLeft()->isAuto()) {
			left = getStyleNode()->getLeft()->getLength().compute(containing_width);
		}
		FixedPoint top = container.y;
		if(!getStyleNode()->getTop()->isAuto()) {
			top = getStyleNode()->getTop()->getLength().compute(containing_height);
		}

		FixedPoint width = container.width;
		if(!getStyleNode()->getRight()->isAuto()) {
			width = container.width - (getStyleNode()->getRight()->getLength().compute(containing_width) + left);
		}

		// if width/height properties are set they override right/bottom.
		if(!getStyleNode()->getWidth()->isAuto()) {
			width = getStyleNode()->getWidth()->getLength().compute(containing_width);
		}

		calculateHorzMPB(containing_width);
		calculateVertMPB(containing_height);

		setContentX(left + getMBPLeft());
		setContentY(top + getMBPTop());
		setContentWidth(width - getMBPWidth());
	}

	void AbsoluteBox::handlePostChildLayout(LayoutEngine& eng, BoxPtr child)
	{
		setContentHeight(child->getTop() + child->getHeight() + child->getMBPBottom());
	}

	void AbsoluteBox::handleLayout(LayoutEngine& eng, const Dimensions& containing)
	{
		const FixedPoint containing_height = container_.height;

		FixedPoint top = container_.y;
		if(!getStyleNode()->getTop()->isAuto()) {
			top = getStyleNode()->getTop()->getLength().compute(containing_height);
		}

		FixedPoint height = container_.height;
		if(!getStyleNode()->getBottom()->isAuto()) {
			height = container_.height - (getStyleNode()->getBottom()->getLength().compute(containing_height) + top);
		}

		if(!getStyleNode()->getHeight()->isAuto()) {
			height = getStyleNode()->getHeight()->getLength().compute(containing_height);
		}

		setContentHeight(height - getMBPHeight());
	}

	void AbsoluteBox::handleRender(DisplayListPtr display_list, const point& offset) const
	{
		// XXX
	}
}
