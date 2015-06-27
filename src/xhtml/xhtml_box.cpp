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

#include "xhtml_absolute_box.hpp"
#include "xhtml_block_box.hpp"
#include "xhtml_box.hpp"
#include "xhtml_inline_element_box.hpp"
#include "xhtml_layout_engine.hpp"
#include "xhtml_line_box.hpp"
#include "xhtml_root_box.hpp"

namespace xhtml
{
	using namespace css;

	namespace
	{
		std::string fp_to_str(const FixedPoint& fp)
		{
			std::ostringstream ss;
			ss << (static_cast<float>(fp)/LayoutEngine::getFixedPointScaleFloat());
			return ss.str();
		}
	}

	std::ostream& operator<<(std::ostream& os, const Rect& r)
	{
		os << "(" << fp_to_str(r.x) << ", " << fp_to_str(r.y) << ", " << fp_to_str(r.width) << ", " << fp_to_str(r.height) << ")";
		return os;
	}

	Box::Box(BoxId id, BoxPtr parent, StyleNodePtr node)
		: id_(id),
		  node_(node),
		  parent_(parent),
		  dimensions_(),
		  boxes_(),
		  absolute_boxes_(),
		  background_info_(node),
		  border_info_(node),
		  offset_(),
		  line_height_(0),
		  end_of_line_(false),
		  is_replaceable_(false)
	{
		if(getNode() != nullptr && getNode()->id() == NodeId::ELEMENT) {
			is_replaceable_ = getNode()->isReplaced();
		}

		init();
	}

	void Box::init()
	{
		// skip for line/text
		if(id_ == BoxId::LINE) {
			return;
		}

		auto& lh = node_->getLineHeight();
		if(lh != nullptr) {
			if(lh->isPercent() || lh->isNumber()) {
				line_height_ = static_cast<FixedPoint>(lh->compute() * node_->getFont()->getFontSize() * 96.0/72.0);
			} else {
				line_height_ = lh->compute();
			}
		}
	}

	RootBoxPtr Box::createLayout(StyleNodePtr node, int containing_width, int containing_height)
	{
		LayoutEngine e;
		// search for the body element then render that content.
		node->preOrderTraversal([&e, containing_width, containing_height](StyleNodePtr node){
			auto n = node->getNode();
			if(n->id() == NodeId::ELEMENT && n->hasTag(ElementId::HTML)) {
				e.layoutRoot(node, nullptr, point(containing_width * LayoutEngine::getFixedPointScale(), containing_height * LayoutEngine::getFixedPointScale()));
				return false;
			}
			return true;
		});
		node->getNode()->layoutComplete();
		return e.getRoot();
	}

	bool Box::ancestralTraverse(std::function<bool(const ConstBoxPtr&)> fn) const
	{
		if(fn(shared_from_this())) {
			return true;
		}
		auto parent = getParent();
		if(parent != nullptr) {
			return parent->ancestralTraverse(fn);
		}
		return false;
	}

	void Box::preOrderTraversal(std::function<void(BoxPtr, int)> fn, int nesting)
	{
		fn(shared_from_this(), nesting);
		// floats, absolutes
		for(auto& child : boxes_) {
			child->preOrderTraversal(fn, nesting+1);
		}
		for(auto& abs : absolute_boxes_) {
			abs->preOrderTraversal(fn, nesting+1);
		}
	}

	void Box::addAbsoluteElement(LayoutEngine& eng, const Dimensions& containing, BoxPtr abs_box)
	{
		absolute_boxes_.emplace_back(abs_box);
		abs_box->layout(eng, containing);
	}

	bool Box::hasChildBlockBox() const
	{
		for(auto& child : boxes_) {
			if(child->isBlockBox()) {
				return true;
			}
		}
		return false;
	}

	void Box::layout(LayoutEngine& eng, const Dimensions& containing)
	{
		std::unique_ptr<LayoutEngine::FloatContextManager> fcm;
		if(getParent() && getParent()->isFloat()) {
			fcm.reset(new LayoutEngine::FloatContextManager(eng, FloatList()));
		}
		point cursor;
		// If we have a clear flag set, then move the cursor in the layout engine to clear appropriate floats.
		if(node_ != nullptr) {
			eng.moveCursorToClearFloats(node_->getClear(), cursor);
		}

		NodePtr node = getNode();

		std::unique_ptr<RenderContext::Manager> ctx_manager;
		if(node != nullptr) {
			ctx_manager.reset(new RenderContext::Manager(node->getProperties()));
		}

		handlePreChildLayout(eng, containing);

		LineBoxPtr open = std::make_shared<LineBox>(shared_from_this(), cursor);

		if(node_ != nullptr) {
			const std::vector<StyleNodePtr>& node_children = node_->getChildren();
			if(!node_children.empty()) {
				boxes_ = eng.layoutChildren(node_children, shared_from_this(), open);
			}
			if(open != nullptr && !open->boxes_.empty()) {
				boxes_.emplace_back(open);
			}
		}

		// xxx offs
		offset_ = (getParent() != nullptr ? getParent()->getOffset() : point()) + point(dimensions_.content_.x, dimensions_.content_.y);

		for(auto& child : boxes_) {
			if(child->isFloat()) {
				child->layout(eng, dimensions_);
				eng.addFloat(child);
			}
		}

		handlePreChildLayout2(eng, containing);

		for(auto& child : boxes_) {
			if(!child->isFloat()) {
				child->layout(eng, dimensions_);
				handlePostChildLayout(eng, child);
			}
		}
		
		handleLayout(eng, containing);
		//layoutAbsolute(eng, containing);

		for(auto& child : boxes_) {
			child->postParentLayout(eng, dimensions_);
		}

		// need to call this after doing layout, since we need to now what the computed padding/border values are.
		border_info_.init(dimensions_);
		background_info_.init(dimensions_);
	}

	void Box::calculateVertMPB(FixedPoint containing_height)
	{
		if(border_info_.isValid(Side::TOP)) {
			setBorderTop(getStyleNode()->getBorderWidths()[0]->compute());
		}
		if(border_info_.isValid(Side::BOTTOM)) {
			setBorderBottom(getStyleNode()->getBorderWidths()[2]->compute());
		}

		setPaddingTop(getStyleNode()->getPadding()[0]->compute(containing_height));
		setPaddingBottom(getStyleNode()->getPadding()[2]->compute(containing_height));

		setMarginTop(getStyleNode()->getMargin()[0]->getLength().compute(containing_height));
		setMarginBottom(getStyleNode()->getMargin()[2]->getLength().compute(containing_height));
	}

	void Box::calculateHorzMPB(FixedPoint containing_width)
	{		
		if(border_info_.isValid(Side::LEFT)) {
			setBorderLeft(getStyleNode()->getBorderWidths()[1]->compute());
		}
		if(border_info_.isValid(Side::RIGHT)) {
			setBorderRight(getStyleNode()->getBorderWidths()[3]->compute());
		}

		setPaddingLeft(getStyleNode()->getPadding()[1]->compute(containing_width));
		setPaddingRight(getStyleNode()->getPadding()[3]->compute(containing_width));

		if(!getStyleNode()->getMargin()[1]->isAuto()) {
			setMarginLeft(getStyleNode()->getMargin()[1]->getLength().compute(containing_width));
		}
		if(!getStyleNode()->getMargin()[3]->isAuto()) {
			setMarginRight(getStyleNode()->getMargin()[3]->getLength().compute(containing_width));
		}
	}

	void Box::render(DisplayListPtr display_list, const point& offset) const
	{
		auto node = getNode();
		std::unique_ptr<RenderContext::Manager> ctx_manager;
		if(node != nullptr && node->id() == NodeId::ELEMENT) {
			// only instantiate on element nodes.
			ctx_manager.reset(new RenderContext::Manager(node->getProperties()));
		}

		point offs = offset;
		offs += point(dimensions_.content_.x, dimensions_.content_.y);

		if(node_ != nullptr && node_->getPosition() == Position::RELATIVE) {
			if(getStyleNode()->getLeft()->isAuto()) {
				if(!getStyleNode()->getRight()->isAuto()) {
					offs.x -= getStyleNode()->getRight()->getLength().compute(getParent()->getWidth());
				}
				// the other case here evaluates as no-change.
			} else {
				if(getStyleNode()->getRight()->isAuto()) {
					offs.x += getStyleNode()->getLeft()->getLength().compute(getParent()->getWidth());
				} else {
					// over-constrained.
					if(getStyleNode()->getDirection() == Direction::LTR) {
						// left wins
						offs.x += getStyleNode()->getLeft()->getLength().compute(getParent()->getWidth());
					} else {
						// right wins
						offs.x -= getStyleNode()->getRight()->getLength().compute(getParent()->getWidth());
					}
				}
			}

			if(getStyleNode()->getTop()->isAuto()) {
				if(!getStyleNode()->getBottom()->isAuto()) {
					offs.y -= getStyleNode()->getBottom()->getLength().compute(getParent()->getHeight());
				}
				// the other case here evaluates as no-change.
			} else {
				// Either bottom is auto in which case top wins or over-constrained in which case top wins.
				offs.y += getStyleNode()->getTop()->getLength().compute(getParent()->getHeight());
			}
		}

		handleRenderBackground(display_list, offs);
		handleRenderBorder(display_list, offs);
		handleRender(display_list, offs);
		for(auto& child : getChildren()) {
			if(!child->isFloat()) {
				child->render(display_list, offs);
			}
		}
		for(auto& child : getChildren()) {
			if(child->isFloat()) {
				child->render(display_list, offs);
			}
		}
		for(auto& ab : absolute_boxes_) {
			ab->render(display_list, point(0, 0));
		}
		handleEndRender(display_list, offs);

		// set the active rect on any parent node.
		if(node != nullptr) {
			auto& dims = getDimensions();
			const int x = (offs.x - dims.padding_.left - dims.border_.left) / LayoutEngine::getFixedPointScale();
			const int y = (offs.y - dims.padding_.top - dims.border_.top) / LayoutEngine::getFixedPointScale();
			const int w = (dims.content_.width + dims.padding_.left + dims.padding_.right + dims.border_.left + dims.border_.right) / LayoutEngine::getFixedPointScale();
			const int h = (dims.content_.height + dims.padding_.top + dims.padding_.bottom + dims.border_.top + dims.border_.bottom) / LayoutEngine::getFixedPointScale();
			node->setActiveRect(rect(x, y, w, h));
		}
	}

	void Box::handleRenderBackground(DisplayListPtr display_list, const point& offset) const
	{
		auto& dims = getDimensions();
		NodePtr node = getNode();
		if(node != nullptr && node->hasTag(ElementId::BODY)) {
			//dims = getRootDimensions();
		}
		background_info_.render(display_list, offset, dims);
	}

	void Box::handleRenderBorder(DisplayListPtr display_list, const point& offset) const
	{
		border_info_.render(display_list, offset, getDimensions());
	}

}
