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
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include <vector>
#include <string>

#define DISALLOW_ASSIGN(TypeName) \
	void operator=(const TypeName&)

#define DISALLOW_ASSIGN_AND_DEFAULT(TypeName) \
    TypeName();                                     \
	void operator=(const TypeName&)

#define DISALLOW_COPY_AND_ASSIGN(TypeName)  \
    TypeName(const TypeName&);              \
    void operator=(const TypeName&)

#define DISALLOW_COPY_ASSIGN_AND_DEFAULT(TypeName)  \
    TypeName();                                     \
    TypeName(const TypeName&);                      \
    void operator=(const TypeName&)

namespace Util
{
	enum class SplitFlags {
		NONE					= 0,
		ALLOW_EMPTY_STRINGS		= 1,
	};

	std::vector<std::string> split(const std::string& s, const std::string& eol, SplitFlags flags=SplitFlags::NONE);
}
