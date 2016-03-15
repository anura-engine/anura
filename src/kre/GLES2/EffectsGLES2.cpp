/*
	Copyright (C) 2012-2016 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "SDL_opengles2.h"

#include "EffectsGLES2.hpp"

namespace KRE
{
	namespace GLESv2
	{
		StippleEffect::StippleEffect(const variant& node)
			: pattern_(0),
			factor_(1)
		{
			ASSERT_LOG(node.has_key("pattern"), "StippleEffect requires 'pattern' attribute: " << node.to_debug_string());
			pattern_ = static_cast<unsigned>(node["pattern"].as_int());
			factor_ = node["factor"].as_int32(1);
		}

		void StippleEffect::apply()
		{
			// XXX disabled for GLESv2 at the moment One possible solution is using a texture and applying it to the line
			// to create the stipple effect.
			// These are deprecated in OpenGL 3.1. We really should replace them with a shader solution.
			// technically should apply these in a stack.
			//glEnable(GL_LINE_STIPPLE);
			//glLineStipple(factor_, pattern_);
		}

		void StippleEffect::clear()
		{
			// XXX disabled for GLESv2 at the moment
			//glDisable(GL_LINE_STIPPLE);
			//glLineStipple(1,0);
		}
	}
}
