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

#include "Blend.hpp"
#include "Util.hpp"

namespace KRE
{
	class BlendModeScope
	{
	public:
		// We use a list type for the stack type of scopes since iterators
		// aren't invalidated by removing/adding elements (unless you remove the item
		// the iterator points to -- but this is the expected case).
		// Access to the back element and adding new elements is constant.
		// Only erasing is linear in complexity.
		typedef std::list<BlendMode> color_stack_type;
		typedef std::list<BlendMode>::iterator iterator;

		explicit BlendModeScope(const BlendMode& bm);
		explicit BlendModeScope(const BlendModeConstants& src, const BlendModeConstants& dst);
		~BlendModeScope();
		static const BlendMode& getCurrentMode();
	private:
		DISALLOW_COPY_ASSIGN_AND_DEFAULT(BlendModeScope);
		iterator it_;
	};
}
