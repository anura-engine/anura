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

#include <stack>

#include "asserts.hpp"
#include "ColorScope.hpp"

namespace KRE
{
	namespace 
	{
		std::stack<Color>& get_color_stack()
		{
			static std::stack<Color> res;
			return res;
		}

		const Color& get_default_color()
		{
			static Color res = Color::colorWhite();
			return res;
		}
	}

	ColorScope::ColorScope(const ColorPtr& color)
		: pop_stack_(color == nullptr ? false : true)
	{
		if(color != nullptr) {
			get_color_stack().emplace(*color);
		}
	}

	ColorScope::ColorScope(const Color& color)
		: pop_stack_(true)
	{
		get_color_stack().emplace(color);
	}

	ColorScope::~ColorScope()
	{
		if(pop_stack_) {
			ASSERT_LOG(get_color_stack().empty() == false, "Color stack was empty in desctructor");
			get_color_stack().pop();
		}
	}

	const Color& ColorScope::getCurrentColor()
	{
		if(get_color_stack().empty()) {
			return get_default_color();
		}
		return get_color_stack().top();
	}
}
