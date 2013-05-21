#ifndef FORMULA_CALLABLE_VISITOR_HPP_INCLUDED
#define FORMULA_CALLABLE_VISITOR_HPP_INCLUDED

#include <boost/shared_ptr.hpp>

#include <set>
#include <vector>

#include "formula_callable.hpp"

namespace game_logic
{

class formula_callable_suspended
{
public:
	virtual ~formula_callable_suspended();
	virtual const formula_callable* value() const = 0;
	virtual void destroy_ref() = 0;
	virtual void restore_ref() = 0;
private:
};

typedef boost::shared_ptr<formula_callable_suspended> formula_callable_suspended_ptr;

class formula_callable_suspended_variant : public formula_callable_suspended
{
public:
	explicit formula_callable_suspended_variant(variant* v)
	  : value_(v->as_callable()), v_(v)
	{
	}

	virtual const formula_callable* value() const { return value_; }
	virtual void destroy_ref() { *v_ = variant(); }
	virtual void restore_ref() { *v_ = variant(value_); }
private:
	const formula_callable* value_;
	variant* v_;
};

template<typename T>
class formula_callable_suspended_impl : public formula_callable_suspended
{
public:
	explicit formula_callable_suspended_impl(boost::intrusive_ptr<T>* ref)
	  : value_(ref->get()), ref_(ref)
	{
	}

	virtual const formula_callable* value() const { return value_; }
	virtual void destroy_ref() { if((*ref_)->refcount() == 1) { value_ = NULL; } ref_->reset(); }
	virtual void restore_ref() { if(!*ref_) { ref_->reset(dynamic_cast<T*>(const_cast<formula_callable*>(value_))); } }
private:
	const formula_callable* value_;
	boost::intrusive_ptr<T>* ref_;
};

class formula_callable_visitor
{
public:
	template<typename T>
	void visit(boost::intrusive_ptr<T>* ref) {
		ptr_.push_back(formula_callable_suspended_ptr(new formula_callable_suspended_impl<T>(ref)));

		visit(**ref);
	}

	void visit(variant* v);
	void visit(const formula_callable& callable);
	void visit(formula_callable& callable);
	const std::vector<formula_callable_suspended_ptr>& pointers() const { return ptr_; }
private:
	std::vector<formula_callable_suspended_ptr> ptr_;
	std::set<const void*> visited_;
};

}

#endif
