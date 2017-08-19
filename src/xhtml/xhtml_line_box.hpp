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

#pragma once

#include "xhtml_box.hpp"
#include "xhtml_text_box.hpp"

namespace xhtml
{
	struct TextHolder
	{
		TextHolder() : txt(nullptr), styles(nullptr) {}
		explicit TextHolder(const TextPtr& t, const StyleNodePtr& s) : txt(t), styles(s), box(nullptr) {}
		explicit TextHolder(const BoxPtr& b, const StyleNodePtr& s) : txt(nullptr), styles(s), box(b) {}
		TextPtr txt;
		StyleNodePtr styles;
		BoxPtr box;
	};

	// This class acts as a container for LineBox's and TextBox's so that we can generate them during layout, but
	// allocate during the LayoutEngine pass.
	class LineBoxContainer : public Box
	{
	public:
		LineBoxContainer(const BoxPtr& parent, const StyleNodePtr& node, const RootBoxPtr& root);
		std::string toString() const override;
		void transform(TextPtr txt, StyleNodePtr styles);
		void addBoxForLayout(const BoxPtr& box, const StyleNodePtr& s) { text_data_.emplace_back(box, s); }
	private:
		void handlePreChildLayout(LayoutEngine& eng, const Dimensions& containing) override;
		void handleLayout(LayoutEngine& eng, const Dimensions& containing) override;
		void postParentLayout(LayoutEngine& eng, const Dimensions& containing) override;
		void handleRender(const KRE::SceneTreePtr& scene_tree, const point& offset) const override;
		void handleRenderBackground(const KRE::SceneTreePtr& scene_tree, const point& offset) const override;
		void handleRenderBorder(const KRE::SceneTreePtr& scene_tree, const point& offset) const override;
		
		std::vector<TextHolder> text_data_;
	};

	class LineBox : public Box
	{
	public:
		LineBox(const BoxPtr& parent, const StyleNodePtr& node, const RootBoxPtr& root);
		std::string toString() const override;
		static std::vector<LineBoxPtr> reflowText(const BoxPtr& parent, const RootBoxPtr& root, const std::vector<TextHolder>& tex_data, LayoutEngine& eng, const Dimensions& containing);
	private:
		void handlePreChildLayout(LayoutEngine& eng, const Dimensions& containing) override;
		void handleLayout(LayoutEngine& eng, const Dimensions& containing) override;
		void postParentLayout(LayoutEngine& eng, const Dimensions& containing) override;
		void handleRender(const KRE::SceneTreePtr& scene_tree, const point& offset) const override;
		void handleRenderBackground(const KRE::SceneTreePtr& scene_tree, const point& offset) const override;
		void handleRenderBorder(const KRE::SceneTreePtr& scene_tree, const point& offset) const override;
	};
}
