/*
	Copyright (C) 2015-2016 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "xhtml_layout_engine.hpp"
#include "xhtml_line_box.hpp"

namespace xhtml
{
	LineBox::LineBox(const BoxPtr& parent, const StyleNodePtr& node, const RootBoxPtr& root)
		: Box(BoxId::LINE, parent, node, root)
	{
	}

	std::string LineBox::toString() const 
	{
		std::ostringstream ss;
		ss << "LineBox: " << getDimensions().content_;
		return ss.str();
	}

	void LineBox::handleLayout(LayoutEngine& eng, const Dimensions& containing)
	{
		calculateHorzMPB(containing.content_.width);
		calculateVertMPB(containing.content_.height);

        int child_height = 0;
        FixedPoint width = 0;
        for(auto& child : getChildren()) {
			if(!child->isFloat()) {
				//child_height += child->getMBPHeight() + child->getHeight();
				child_height = std::max(child_height, child->getTop() + child->getMBPBottom() + child->getHeight());
				width = std::max(width, child->getLeft() + child->getWidth() + child->getMBPWidth());
			}
		}
		setContentHeight(child_height);
		setContentWidth(width);
	}

	void LineBox::postParentLayout(LayoutEngine& eng, const Dimensions& containing)
	{
	}

	void LineBox::handleRender(const KRE::SceneTreePtr& scene_tree, const point& offset) const 
	{
	}

	void LineBox::handleRenderBackground(const KRE::SceneTreePtr& scene_tree, const point& offset) const 
	{
	}

	void LineBox::handleRenderBorder(const KRE::SceneTreePtr& scene_tree, const point& offset) const 
	{
	}

	std::vector<LineBoxPtr> LineBox::reflowText(const BoxPtr& parent, const RootBoxPtr& root, const std::vector<TextHolder>& text_data, LayoutEngine& eng, const Dimensions& containing)
	{
		return TextBox::reflowText(text_data, parent, root, eng, containing);
	}


	LineBoxContainer::LineBoxContainer(const BoxPtr& parent, const StyleNodePtr& node, const RootBoxPtr& root)
		: Box(BoxId::LINE_CONTAINER, parent, node, root),
		  text_data_()
	{
	}

	void LineBoxContainer::transform(TextPtr txt, StyleNodePtr styles)
	{
		text_data_.emplace_back(txt, styles);
		ASSERT_LOG(txt != nullptr, "TextPtr was null.");
		txt->transformText(styles, true);
	}

	std::string LineBoxContainer::toString() const
	{
		std::ostringstream ss;
		ss << "LineBoxContainer: " << getDimensions().content_;
		return ss.str();
	}

	void LineBoxContainer::handlePreChildLayout(LayoutEngine& eng, const Dimensions& containing)
	{
		if(!text_data_.empty()) {
			std::vector<LineBoxPtr> line_boxes = LineBox::reflowText(getParent(), getRoot(), text_data_, eng, containing);

			for(const auto& line_box : line_boxes) {
				addChild(line_box);
			}
		}
		FixedPoint left = getMBPLeft();
		FixedPoint top = getMBPTop() + containing.content_.height;
		setContentX(left);
		setContentY(top);
	}

	void LineBoxContainer::handleLayout(LayoutEngine& eng, const Dimensions& containing)
	{
		calculateHorzMPB(containing.content_.width);
		calculateVertMPB(containing.content_.height);

        int child_height = 0;
        FixedPoint width = 0;
        for(auto& child : getChildren()) {
			if(!child->isFloat()) {
				child_height = std::max(child_height, child->getTop() + child->getMBPBottom() + child->getHeight());
				width = std::max(width, child->getLeft() + child->getWidth() + child->getMBPWidth());
			}
		}
		setContentHeight(child_height);
		setContentWidth(width);
	}

	void LineBoxContainer::postParentLayout(LayoutEngine& eng, const Dimensions& containing)
	{
	}

	void LineBoxContainer::handleRender(const KRE::SceneTreePtr& scene_tree, const point& offset) const
	{
	}

	void LineBoxContainer::handleRenderBackground(const KRE::SceneTreePtr& scene_tree, const point& offset) const
	{
	}

	void LineBoxContainer::handleRenderBorder(const KRE::SceneTreePtr& scene_tree, const point& offset) const
	{
	}

}
