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

#include "xhtml_root_box.hpp"
#include "xhtml_layout_engine.hpp"

namespace xhtml
{
	using namespace css;

	RootBox::RootBox(const BoxPtr& parent, const StyleNodePtr& node)
		: BlockBox(parent, node, nullptr),
		  fixed_boxes_()
	{
	}

	std::string RootBox::toString() const 
	{
		std::ostringstream ss;
		ss << "RootBox: " << getDimensions().content_ << (!fixed_boxes_.empty() ? "\n" : "");

		int nesting = 10;
		for(auto& f : fixed_boxes_) {
			ss << std::string((nesting+1) * 2, ' ') << " FixedBox: " << f->toString() << "\n";
		}
		return ss.str();
	}

	void RootBox::handleLayout(LayoutEngine& eng, const Dimensions& containing) 
	{
		//BlockBox::handleLayout(eng, containing);

		calculateHorzMPB(containing.content_.width);
		calculateVertMPB(containing.content_.height);

		setContentX(getMBPLeft());
		setContentY(getMBPTop());

		setContentWidth(containing.content_.width - getMBPWidth());
		setContentHeight(containing.content_.height - getMBPHeight());

		layoutFixed(eng, containing);
	}

	void RootBox::handleEndRender(const KRE::SceneTreePtr& scene_tree, const point& offset) const
	{
		// render fixed boxes.
		for(auto& fix : fixed_boxes_) {
			fix->render(point(0, 0));
		}
	}

	void RootBox::handleCreateSceneTree(KRE::SceneTreePtr scene_parent)
	{
		for(auto& fix : fixed_boxes_) {
			KRE::SceneTreePtr ptr = fix->createSceneTree(scene_parent);
			scene_parent->addChild(ptr);
		}
	}

	void RootBox::addFixed(BoxPtr fixed)
	{
		fixed_boxes_.emplace_back(fixed);
	}

	void RootBox::layoutFixed(LayoutEngine& eng, const Dimensions& containing)
	{
		for(auto& fix : fixed_boxes_) {
			fix->layout(eng, eng.getDimensions());
		}
	}
}
