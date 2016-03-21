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

#pragma once

#include <list>

#include "Color.hpp"
#include "Util.hpp"

namespace KRE
{
	class ColorScope;
	typedef std::unique_ptr<ColorScope> ColorScopePtr;

	class ColorScope
	{
	public:
		explicit ColorScope(const ColorPtr& color);
		explicit ColorScope(const Color& color);
		~ColorScope();
		static const Color& getCurrentColor();
	private:
		DISALLOW_COPY_ASSIGN_AND_DEFAULT(ColorScope);
		bool pop_stack_;
	};
}
