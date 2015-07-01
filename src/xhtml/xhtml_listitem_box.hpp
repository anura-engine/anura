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

namespace xhtml
{
	class ListItemBox : public Box
	{
	public:
		explicit ListItemBox(BoxPtr parent, StyleNodePtr node, int count);
		std::string toString() const override;
	private:
		void handlePreChildLayout(LayoutEngine& eng, const Dimensions& containing) override;
		void handleLayout(LayoutEngine& eng, const Dimensions& containing) override;
		void handlePostChildLayout(LayoutEngine& eng, BoxPtr child) override;
		void handleRender(DisplayListPtr display_list, const point& offset) const override;
		int count_;
		std::string marker_;
	};

}
