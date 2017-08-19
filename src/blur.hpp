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

#pragma once

#include <deque>

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

class Frame;

class CustomObject;

class BlurObject : public game_logic::FormulaCallable
{
public:
	BlurObject(const std::map<std::string,variant>& starting_properties, const std::map<std::string,variant>& ending_properties, int duration, variant easing);
	~BlurObject();

	void setObject(CustomObject* obj);
	void draw(int x, int y);
	void process();
	bool expired() const;
private:
	ffl::IntrusivePtr<CustomObject> obj_;
	std::map<std::string,variant> start_properties_, end_properties_, cur_properties_;
	int duration_, age_;
	variant easing_;
	DECLARE_CALLABLE(BlurObject);
};
