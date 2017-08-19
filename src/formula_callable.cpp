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

#include "formula_callable.hpp"
#include "formula_callable_visitor.hpp"

namespace game_logic
{
	void MapFormulaCallable::visitValues(FormulaCallableVisitor& visitor)
	{
		for(std::map<std::string,variant>::iterator i = values_.begin();
			i != values_.end(); ++i) {
			visitor.visit(&i->second);
		}
	}

	FnCommandCallable::FnCommandCallable(const char* name, std::function<void()> fn) : name_(name), fn_(fn)
	{}

	void FnCommandCallable::execute(FormulaCallable& context) const
	{
		fn_();
	}

	std::string FnCommandCallable::debugObjectName() const
	{
		return std::string("FnCommandCallable: ") + name_;
	}

	FnCommandCallableArg::FnCommandCallableArg(const char* name, std::function<void(FormulaCallable*)> fn) : name_(name), fn_(fn)
	{}

	void FnCommandCallableArg::execute(FormulaCallable& context) const
	{
		fn_(&context);
	}

	std::string FnCommandCallableArg::debugObjectName() const
	{
		return std::string("FnCommandCallableArg: ") + name_;
	}
}
