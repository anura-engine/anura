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

#include "css_properties.hpp"
#include "FontDriver.hpp"

namespace xhtml
{
	struct RenderContextManager
	{
		RenderContextManager();
		~RenderContextManager();
	};

	class RenderContext
	{
	public:
		// Returns the render context instance.
		static RenderContext& get();
		
		struct Manager
		{
			explicit Manager(const css::PropertyList& plist);
			~Manager();
			std::vector<int> update_list;
			bool pushed_font_change_;
		};

		int getDPI() const { return dpi_scale_; }
		void setDPI(int dpi) { dpi_scale_ = dpi; }

		const point& getViewport() const { return viewport_; }
		void setViewport(const point& p) { viewport_ = p; }

		const css::StylePtr& getComputedValue(css::Property p) const;

		std::vector<css::StylePtr> getCurrentStyles() const;

		// We need special case handling for the font.
		KRE::FontHandlePtr getFontHandle() const;
	private:
		RenderContext();
		int dpi_scale_;
		point viewport_;
	};
}
