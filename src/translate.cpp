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

#include <map>

#include "logger.hpp"
#include "translate.hpp"

namespace i18n
{
	namespace
	{
		typedef std::map<std::string,std::string> translation_map;
		translation_map map;
	}

	void add_translation(const std::string& from, const std::string& to)
	{
		LOG_INFO("add translation: " << from << " -> " << to);
		map[from] = to;
	}

	const std::string& translate(const std::string& from)
	{
		const translation_map::const_iterator i = map.find(from);
		if(i != map.end()) {
			return i->second;
		} else {
			return from;
		}
	}
}
