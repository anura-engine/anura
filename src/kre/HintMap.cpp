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

#include "asserts.hpp"
#include "HintMap.hpp"

HintMapContainer::HintMapContainer()
{
}

const std::vector<std::string>& HintMapContainer::findHint(const std::string& name) const
{
	static std::vector<std::string> no_hint_list;
	auto it = hints_.find(name);
	if(it != hints_.end()) {
		return it->second;
	}
	LOG_WARN("No hint named '" << name << "' found.");
	return no_hint_list;
}

const std::string& HintMapContainer::findFirstHint(const std::string& name, const std::string& def) const
{
	auto it = hints_.find(name);
	if(it != hints_.end()) {
		return it->second.front();
	}
	return def;
}

void HintMapContainer::setHint(const std::string& hint_name, const std::string& hint)
{
	HintList hint_list(1,hint);
	hints_.insert(std::make_pair(hint_name, hint_list));
}

void HintMapContainer::setHint(const std::string& hint_name, const HintList& hint)
{
	hints_[hint_name] = hint;
}
