#include "foreach.hpp"
#include "formula_callable_visitor.hpp"
#include "variant.hpp"

namespace game_logic
{

formula_callable_suspended::~formula_callable_suspended()
{
}

void formula_callable_visitor::visit(variant* v)
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

		foreach(const variant& key, v->get_keys().as_list()) {
			visit(v->get_attr_mutable(key));
		}
	} else if(v->is_callable()) {
		ptr_.push_back(formula_callable_suspended_ptr(new formula_callable_suspended_variant(v)));
		visit(*v->as_callable());
	} else if(v->is_function()) {
		std::vector<boost::intrusive_ptr<const formula_callable>*> items;
		v->get_mutable_closure_ref(items);
		foreach(boost::intrusive_ptr<const formula_callable>* ptr, items) {
			visit(ptr);
		}
	}
}

void formula_callable_visitor::visit(const formula_callable& callable)
{
	visit(const_cast<formula_callable&>(callable));
}

void formula_callable_visitor::visit(formula_callable& callable)
{
	if(visited_.count(&callable)) {
		return;
	}

	visited_.insert(&callable);

	callable.perform_visit_values(*this);
}

}
