/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "BlendModeScope.hpp"

namespace KRE
{
	namespace 
	{
		BlendModeScope::color_stack_type& get_mode_stack()
		{
			static BlendModeScope::color_stack_type res;
			return res;
		}

		const BlendMode& get_default_mode()
		{
			static BlendMode res = BlendMode();
			return res;
		}
	}

	BlendModeScope::BlendModeScope(const BlendMode& bm)
	{
		it_ = get_mode_stack().emplace(get_mode_stack().end(), bm);
	}

	BlendModeScope::BlendModeScope(const BlendModeConstants& src, const BlendModeConstants& dst)
	{
		it_ = get_mode_stack().emplace(get_mode_stack().end(), BlendMode(src, dst));
	}

	BlendModeScope::~BlendModeScope()
	{
		get_mode_stack().erase(it_);
	}

	const BlendMode& BlendModeScope::getCurrentMode()
	{
		if(get_mode_stack().empty()) {
			return get_default_mode();
		}
		return get_mode_stack().back();
	}
}
