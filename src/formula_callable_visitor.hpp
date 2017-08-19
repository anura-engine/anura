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

#include <set>
#include <vector>

#include "formula_callable.hpp"

namespace game_logic
{
	class FormulaCallableSuspended
	{
	public:
		virtual ~FormulaCallableSuspended();
		virtual const FormulaCallable* value() const = 0;
		virtual void destroy_ref() = 0;
		virtual void restore_ref() = 0;
	private:
	};

	typedef std::shared_ptr<FormulaCallableSuspended> FormulaCallableSuspendedPtr;

	class FormulaCallableSuspendedVariant : public FormulaCallableSuspended
	{
	public:
		explicit FormulaCallableSuspendedVariant(variant* v)
		  : value_(v->as_callable()), v_(v)
		{
		}

		virtual const FormulaCallable* value() const override { return value_; }
		virtual void destroy_ref() override { *v_ = variant(); }
		virtual void restore_ref() override { *v_ = variant(value_); }
	private:
		const FormulaCallable* value_;
		variant* v_;
	};

	template<typename T>
	class FormulaCallableSuspendedImpl : public FormulaCallableSuspended
	{
	public:
		explicit FormulaCallableSuspendedImpl(ffl::IntrusivePtr<T>* ref)
		  : value_(ref->get()), ref_(ref)
		{
		}

		virtual const FormulaCallable* value() const override { return value_; }
		virtual void destroy_ref() override { if((*ref_)->refcount() == 1) { value_ = nullptr; } ref_->reset(); }
		virtual void restore_ref() override { if(!*ref_) { ref_->reset(dynamic_cast<T*>(const_cast<FormulaCallable*>(value_))); } }
	private:
		const FormulaCallable* value_;
		ffl::IntrusivePtr<T>* ref_;
	};

	class FormulaCallableVisitor
	{
	public:
		template<typename T>
		void visit(ffl::IntrusivePtr<T>* ref) {
			if(ref->get() == nullptr) {
				return;
			}

			ptr_.push_back(FormulaCallableSuspendedPtr(new FormulaCallableSuspendedImpl<T>(ref)));

			visit(**ref);
		}

		void visit(variant* v);
		void visit(const FormulaCallable& callable);
		void visit(FormulaCallable& callable);
		const std::vector<FormulaCallableSuspendedPtr>& pointers() const { return ptr_; }
	private:
		std::vector<FormulaCallableSuspendedPtr> ptr_;
		std::set<const void*> visited_;
	};
}
