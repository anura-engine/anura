#include "foreach.hpp"
#include "formula_callable_visitor.hpp"
#include "variant.hpp"

namespace game_logic
{

FormulaCallable_suspended::~FormulaCallable_suspended()
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

		foreach(const variant& key, v->getKeys().as_list()) {
			visit(v->get_attr_mutable(key));
		}
	} else if(v->is_callable()) {
		ptr_.push_back(FormulaCallable_suspended_ptr(new FormulaCallable_suspended_variant(v)));
		visit(*v->as_callable());
	} else if(v->is_function()) {
		std::vector<boost::intrusive_ptr<const FormulaCallable>*> items;
		v->get_mutable_closure_ref(items);
		foreach(boost::intrusive_ptr<const FormulaCallable>* ptr, items) {
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
