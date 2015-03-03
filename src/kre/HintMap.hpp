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

#include <map>
#include <string>
#include <vector>

typedef std::vector<std::string> HintList;
typedef std::map<std::string,HintList> HintMap;
class HintMapContainer
{
public:
	HintMapContainer();
	void setHint(const std::string& hint_name, const std::string& hint);
	void setHint(const std::string& hint_name, const HintList& hint);
	const std::vector<std::string>& findHint(const std::string& name) const;
	const std::string& findFirstHint(const std::string& name, const std::string& def=std::string()) const;
	HintMap getHints() const { return hints_; }
private:
	HintMap hints_;
};
