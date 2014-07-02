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

fn_command_callable::fn_command_callable(std::function<void()> fn) : fn_(fn)
{}

void fn_command_callable::execute(FormulaCallable& context) const
{
	fn_();
}

fn_command_callable_arg::fn_command_callable_arg(std::function<void(FormulaCallable*)> fn) : fn_(fn)
{}

void fn_command_callable_arg::execute(FormulaCallable& context) const
{
	fn_(&context);
}

}
