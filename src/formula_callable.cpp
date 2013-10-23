#include "formula_callable.hpp"
#include "formula_callable_visitor.hpp"

namespace game_logic
{

void map_formula_callable::visit_values(formula_callable_visitor& visitor)
{
	for(std::map<std::string,variant>::iterator i = values_.begin();
	    i != values_.end(); ++i) {
		visitor.visit(&i->second);
	}
}

fn_command_callable::fn_command_callable(std::function<void()> fn) : fn_(fn)
{}

void fn_command_callable::execute(formula_callable& context) const
{
	fn_();
}

fn_command_callable_arg::fn_command_callable_arg(std::function<void(formula_callable*)> fn) : fn_(fn)
{}

void fn_command_callable_arg::execute(formula_callable& context) const
{
	fn_(&context);
}

}
