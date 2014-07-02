/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
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

#include <boost/shared_ptr.hpp>

#include <string>

#include "variant.hpp"

class Achievement;

typedef std::shared_ptr<const Achievement> AchievementPtr;

class Achievement
{
public:
	static AchievementPtr get(const std::string& id);

	explicit Achievement(variant node);

	const std::string& id() const { return id_; }
	const std::string& name() const { return name_; }
	const std::string& description() const { return description_; }
	int points() const { return points_; }
	static bool attain(const std::string& id);
private:
	std::string id_, name_, description_;
	int points_;
};
