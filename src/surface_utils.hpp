/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include "kre/Surface.hpp"

namespace graphics
{
	enum class SpritesheetOptions {
		DEFAULT					= 0,
		NO_STRIP_ANNOTATIONS	= 1
	};

	inline bool operator&(SpritesheetOptions lhs, SpritesheetOptions rhs) {
		return (static_cast<int>(lhs) & static_cast<int>(rhs)) != 0;
	}

	void set_alpha_for_transparent_colors_in_rgba_surface(KRE::SurfacePtr s, SpritesheetOptions options=SpritesheetOptions::DEFAULT);
	const unsigned char* get_alpha_pixel_colors();
	unsigned long map_color_to_16bpp(unsigned long color);
}
