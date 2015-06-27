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

#include "geometry.hpp"
#include "Texture.hpp"

#include "xhtml_style_tree.hpp"

namespace xhtml
{
	struct BgBoxShadow
	{
		BgBoxShadow();
		explicit BgBoxShadow(FixedPoint x, FixedPoint y, FixedPoint blur, FixedPoint spread, bool ins, const KRE::ColorPtr& col); 
		FixedPoint x_offset;
		FixedPoint y_offset;
		FixedPoint blur_radius;
		FixedPoint spread_radius;
		bool inset;
		KRE::ColorPtr color;
	};

	class BackgroundInfo
	{
	public:
		explicit BackgroundInfo(const StyleNodePtr& styles);
		void render(DisplayListPtr display_list, const point& offset, const Dimensions& dims) const;
		void init(const Dimensions& dims);
	private:
		void renderBoxShadow(DisplayListPtr display_list, const point& offset, const Dimensions& dims, KRE::RenderablePtr clip_shape) const;

		StyleNodePtr styles_;
		KRE::TexturePtr texture_;

		std::vector<BgBoxShadow> box_shadows_;

		std::array<FixedPoint, 4> border_radius_horiz_;
		std::array<FixedPoint, 4> border_radius_vert_;

		bool has_border_radius_;
	};
}
