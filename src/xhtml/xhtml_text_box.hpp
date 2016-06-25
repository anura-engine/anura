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
	struct TextHolder;

	struct LineInfo 
	{
		LineInfo() : line_(nullptr), offset_(), justification_(0), width_(0), height_(0) {}
		explicit LineInfo(const LinePtr& line, const point& offset=point()) : line_(line), offset_(offset), justification_(0), width_(0), height_(0) {}
		LinePtr line_;
		point offset_;
		FixedPoint justification_;
		FixedPoint width_;
		FixedPoint height_;
	};

	class TextBox : public Box
	{
	public:
		TextBox(const BoxPtr& parent, const StyleNodePtr& node, const RootBoxPtr& root);
		std::string toString() const override;
		const LineInfo& getLine() const { return line_; }
		static std::vector<LineBoxPtr> reflowText(const std::vector<TextHolder>& th, const BoxPtr& parent, const RootBoxPtr& root, LayoutEngine& eng, const Dimensions& containing);
	private:
		void handleLayout(LayoutEngine& eng, const Dimensions& containing) override;
		void postParentLayout(LayoutEngine& eng, const Dimensions& containing) override;
		void handleRender(const KRE::SceneTreePtr& scene_tree, const point& offset) const override;
		void handleRenderBackground(const KRE::SceneTreePtr& scene_tree, const point& offset) const override;
		void handleRenderBorder(const KRE::SceneTreePtr& scene_tree, const point& offset) const override;
		void handleRenderShadow(const KRE::SceneTreePtr& scene_tree, KRE::FontRenderablePtr fontr, float w, float h) const;
		FixedPoint calculateWidth(const LineInfo& line) const;

		void setJustify(FixedPoint containing_width);
		void setRightAlign(FixedPoint containing_width);
		void setCenterAlign(FixedPoint containing_width);

		LineInfo line_;

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
