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

#include "formula_callable_visitor.hpp"

namespace game_logic
{
	FormulaCallableSuspended::~FormulaCallableSuspended()
	{
	}

	void FormulaCallableVisitor::visit(variant* v)
	{
		if(!v) {
			return;
		}

		if(v->is_list()) {
			if(visited_.count(v->get_addr())) {
				return;
			}
			visited_.insert(v->get_addr());

			for(int n = 0; n != v->num_elements(); ++n) {
				visit(v->get_index_mutable(n));
			}
		} else if(v->is_map()) {
			if(visited_.count(v->get_addr())) {
				return;
			}
			visited_.insert(v->get_addr());

			for(const variant& key : v->getKeys().as_list()) {
				visit(v->get_attr_mutable(key));
			}
		} else if(v->is_callable()) {
			ptr_.push_back(FormulaCallableSuspendedPtr(new FormulaCallableSuspendedVariant(v)));
			visit(*v->as_callable());
		} else if(v->is_function()) {
			std::vector<ffl::IntrusivePtr<const FormulaCallable>*> items;
			v->get_mutable_closure_ref(items);
			for(ffl::IntrusivePtr<const FormulaCallable>* ptr : items) {
				visit(ptr);
			}
		}
	}

	void FormulaCallableVisitor::visit(const FormulaCallable& callable)
	{
		visit(const_cast<FormulaCallable&>(callable));
	}

	void FormulaCallableVisitor::visit(FormulaCallable& callable)
	{
		if(visited_.count(&callable)) {
			return;
		}

		visited_.insert(&callable);

		callable.performVisitValues(*this);
	}
}
