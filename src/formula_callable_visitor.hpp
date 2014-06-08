#ifndef FormulaCallableVisitor_HPP_INCLUDED
#define FormulaCallableVisitor_HPP_INCLUDED

#include <boost/shared_ptr.hpp>

#include <set>
#include <vector>

#include "formula_callable.hpp"

namespace game_logic
{

class FormulaCallable_suspended
{
public:
	virtual ~FormulaCallable_suspended();
	virtual const FormulaCallable* value() const = 0;
	virtual void destroy_ref() = 0;
	virtual void restore_ref() = 0;
private:
};

typedef boost::shared_ptr<FormulaCallable_suspended> FormulaCallable_suspended_ptr;

class FormulaCallable_suspended_variant : public FormulaCallable_suspended
{
public:
	explicit FormulaCallable_suspended_variant(variant* v)
	  : value_(v->as_callable()), v_(v)
	{
	}

	virtual const FormulaCallable* value() const { return value_; }
	virtual void destroy_ref() { *v_ = variant(); }
	virtual void restore_ref() { *v_ = variant(value_); }
private:
	const FormulaCallable* value_;
	variant* v_;
};

template<typename T>
class FormulaCallable_suspended_impl : public FormulaCallable_suspended
{
public:
	explicit FormulaCallable_suspended_impl(boost::intrusive_ptr<T>* ref)
	  : value_(ref->get()), ref_(ref)
	{
	}

	virtual const FormulaCallable* value() const { return value_; }
	virtual void destroy_ref() { if((*ref_)->refcount() == 1) { value_ = NULL; } ref_->reset(); }
	virtual void restore_ref() { if(!*ref_) { ref_->reset(dynamic_cast<T*>(const_cast<FormulaCallable*>(value_))); } }
private:
	const FormulaCallable* value_;
	boost::intrusive_ptr<T>* ref_;
};

class FormulaCallableVisitor
{
public:
	template<typename T>
	void visit(boost::intrusive_ptr<T>* ref) {
		if(ref->get() == NULL) {
			return;
		}

		ptr_.push_back(FormulaCallable_suspended_ptr(new FormulaCallable_suspended_impl<T>(ref)));

		visit(**ref);
	}

	void visit(variant* v);
	void visit(const FormulaCallable& callable);
	void visit(FormulaCallable& callable);
	const std::vector<FormulaCallable_suspended_ptr>& pointers() const { return ptr_; }
private:
	std::vector<FormulaCallable_suspended_ptr> ptr_;
	std::set<const void*> visited_;
};

}

#endif
