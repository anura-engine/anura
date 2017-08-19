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
	class BorderInfo
	{
	public:
		explicit BorderInfo(const StyleNodePtr& styles);
		void init(const Dimensions& dims);
		bool render(const KRE::SceneTreePtr& scene_tree, const Dimensions& dims, const point& offset) const;
		void renderNormal(const KRE::SceneTreePtr& scene_tree, const Dimensions& dims, const point& offset) const;
		void setWidths(const std::array<float,4>& widths) { widths_ = widths; }
		void setOutset(const std::array<float,4>& outset) { outset_ = outset; }
		void setSlice(const std::array<float,4>& slice) { slice_ = slice; }
		bool isValid(css::Side side) const;
	private:
		StyleNodePtr styles_;
		KRE::TexturePtr image_;
		std::array<float,4> slice_;
		std::array<float,4> outset_;
		std::array<float,4> widths_;
	};

}
