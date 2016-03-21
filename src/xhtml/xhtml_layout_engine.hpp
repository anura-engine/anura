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

#include <stack>

#include "xhtml_box.hpp"
#include "xhtml_render_ctx.hpp"

namespace xhtml
{
	class LayoutEngine
	{
	public:
		explicit LayoutEngine();

		void layoutRoot(StyleNodePtr node, BoxPtr parent, const point& container);
		
		std::vector<BoxPtr> layoutChildren(const std::vector<StyleNodePtr>& children, BoxPtr parent);

		FixedPoint getDescent() const;

		RootBoxPtr getRoot() const { return root_; }
		
		FixedPoint getXAtPosition(FixedPoint y1, FixedPoint y2) const;
		FixedPoint getX2AtPosition(FixedPoint y1, FixedPoint y2) const;

		FixedPoint getWidthAtPosition(FixedPoint y1, FixedPoint y2, FixedPoint width) const;

		bool hasFloatsAtPosition(FixedPoint y1, FixedPoint y2) const;

		void moveCursorToClearFloats(css::Clear float_clear, point& cursor);

		const Dimensions& getDimensions() const { return dims_; }

		static FixedPoint getFixedPointScale() { return 65536; }
		static float getFixedPointScaleFloat() { return 65536.0f; }

		const point& getOffset();

		struct FloatContextManager
		{
			FloatContextManager(LayoutEngine& eng, const FloatList& floats) : eng_(eng) { eng_.float_list_.emplace(floats); };
			~FloatContextManager() { eng_.float_list_.pop(); }
			LayoutEngine& eng_;
		};
		const FloatList& getFloatList() const;
		void addFloat(BoxPtr float_box);

		const point& getCursor() const { return cursor_; }
		void setCursor(const point& p) { cursor_ = p; }
		void resetCursor() { cursor_.x = cursor_.y = 0; }
	private:
		RootBoxPtr root_;
		Dimensions dims_;
		RenderContext& ctx_;
		
		std::stack<int> list_item_counter_;
		std::stack<point> offset_;

		std::stack<FloatList> float_list_;
		
		point cursor_;
	};

}
