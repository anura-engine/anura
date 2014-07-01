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

#include <functional>
#include <iostream>
#include <map>
#include <string>

#include "reference_counted_object.hpp"
#include "variant.hpp"

namespace game_logic
{
	enum FORMULA_ACCESS_TYPE { FORMULA_READ_ONLY, FORMULA_WRITE_ONLY, FORMULA_READ_WRITE };
	struct formula_input 
	{
		std::string name;
		FORMULA_ACCESS_TYPE access;
		formula_input(const std::string& name, FORMULA_ACCESS_TYPE access=FORMULA_READ_WRITE)
				: name(name), access(access)
		{}
	};

	class FormulaCallableVisitor;

	//interface for objects that can have formulae run on them
	class FormulaCallable : public reference_counted_object
	{
	public:
		explicit FormulaCallable(bool has_self=false) : has_self_(has_self)
		{}

		std::string query_id() const { return getObjectId(); }

		variant query_value(const std::string& key) const {
			if(has_self_ && key == "self") {
				return variant(this);
			}
			return getValue(key);
		}

		variant query_value_by_slot(int slot) const {
			return getValueBySlot(slot);
		}

		void mutate_value(const std::string& key, const variant& value) {
			setValue(key, value);
		}

		void mutate_value_by_slot(int slot, const variant& value) {
			setValueBySlot(slot, value);
		}

		std::vector<formula_input> inputs() const {
			std::vector<formula_input> res;
			getInputs(&res);
			return res;
		}

		bool equals(const FormulaCallable* other) const {
			return do_compare(other) == 0;
		}

		bool less(const FormulaCallable* other) const {
			return do_compare(other) < 0;
		}

		virtual void getInputs(std::vector<formula_input>* /*inputs*/) const {};

		void serialize(std::string& str) const {
			serialize_to_string(str);
		}

		bool has_key(const std::string& key) const 
			{ return !query_value(key).is_null(); }

		// In order to provide support for widgets to be able to have FFL handlers for events
		// The following two functions are provided for them to use to respectively execute
		// a command and create a new formula from a variant (which is expected to contain FFL 
		// commands).  If you're making some new that object that provides a custom symbol
		// table or supports different types of command_callable you should override these 
		// two functions to provide widget support.
		virtual bool executeCommand(const variant &v);
		virtual formula_ptr createFormula(const variant& v);

		//is some kind of command to the engine.
		virtual bool isCommand() const { return false; }
		virtual bool is_cairo_op() const { return false; }

		void performVisitValues(FormulaCallableVisitor& visitor) {
			visitValues(visitor);
		}

	protected:
		virtual ~FormulaCallable() {}

		virtual void setValue(const std::string& key, const variant& value);
		virtual void setValueBySlot(int slot, const variant& value);
		virtual int do_compare(const FormulaCallable* callable) const {
			return this < callable ? -1 : (this == callable ? 0 : 1);
		}

		virtual void serialize_to_string(std::string& str) const;

		virtual void visitValues(FormulaCallableVisitor& visitor) {}
	private:
		virtual variant getValue(const std::string& key) const = 0;
		virtual variant getValueBySlot(int slot) const;

		virtual std::string getObjectId() const { return "FormulaCallable"; }

		bool has_self_;
	};

	class FormulaCallable_no_ref_count : public FormulaCallable {
	public:
		FormulaCallable_no_ref_count() {
			turn_reference_counting_off();
		}
		virtual ~FormulaCallable_no_ref_count() {}
	};

	class FormulaCallable_with_backup : public FormulaCallable {
		const FormulaCallable& main_;
		const FormulaCallable& backup_;
		variant getValueBySlot(int slot) const {
			return backup_.query_value_by_slot(slot);
		}

		variant getValue(const std::string& key) const {
			variant var = main_.query_value(key);
			if(var.is_null()) {
				return backup_.query_value(key);
			}

			return var;
		}

		void getInputs(std::vector<formula_input>* inputs) const {
			main_.getInputs(inputs);
			backup_.getInputs(inputs);
		}
	public:
		FormulaCallable_with_backup(const FormulaCallable& main, const FormulaCallable& backup) : FormulaCallable(false), main_(main), backup_(backup)
		{}
	};

	class formula_variant_callable_with_backup : public FormulaCallable {
		variant var_;
		const FormulaCallable& backup_;
		variant getValue(const std::string& key) const {
			variant var = var_.get_member(key);
			if(var.is_null()) {
				return backup_.query_value(key);
			}

			return var;
		}

		variant getValueBySlot(int slot) const {
			return backup_.query_value_by_slot(slot);
		}

		void getInputs(std::vector<formula_input>* inputs) const {
			backup_.getInputs(inputs);
		}

	public:
		formula_variant_callable_with_backup(const variant& var, const FormulaCallable& backup) : FormulaCallable(false), var_(var), backup_(backup)
		{}
	};

	class MapFormulaCallable : public FormulaCallable {
	public:
		explicit MapFormulaCallable(variant node);
		explicit MapFormulaCallable(const FormulaCallable* fallback=NULL);
		explicit MapFormulaCallable(const std::map<std::string, variant>& m);
		variant write() const;
		MapFormulaCallable& add(const std::string& key, const variant& value);
		void set_fallback(const FormulaCallable* fallback) { fallback_ = fallback; }

		//adds an entry and gets direct access to the variant. Use with caution
		//and for cases where calling add() repeatedy isn't efficient enough.
		variant& add_direct_access(const std::string& key);

		bool empty() const { return values_.empty(); }
		void clear() { values_.clear(); }
		bool contains(const std::string& key) const { return values_.count(key) != 0; }

		const std::map<std::string, variant>& values() const { return values_; }

		typedef std::map<std::string,variant>::const_iterator const_iterator;

		const_iterator begin() const { return values_.begin(); }
		const_iterator end() const { return values_.end(); }

		variant& ref(const std::string& key) { return values_[key]; }

	private:
		//MapFormulaCallable(const MapFormulaCallable&);

		variant getValueBySlot(int slot) const {
			return fallback_->query_value_by_slot(slot);
		}

		virtual void visitValues(FormulaCallableVisitor& visitor);

		variant getValue(const std::string& key) const;
		void getInputs(std::vector<formula_input>* inputs) const;
		void setValue(const std::string& key, const variant& value);
		std::map<std::string,variant> values_;
		const FormulaCallable* fallback_;
	};

	typedef boost::intrusive_ptr<FormulaCallable> FormulaCallablePtr;
	typedef boost::intrusive_ptr<const FormulaCallable> ConstFormulaCallablePtr;

	typedef boost::intrusive_ptr<MapFormulaCallable> MapFormulaCallablePtr;
	typedef boost::intrusive_ptr<const MapFormulaCallable> ConstMapFormulaCallablePtr;

	class formula_expression;

	class command_callable : public FormulaCallable {
	public:
		command_callable();
		void runCommand(FormulaCallable& context) const;

		void setExpression(const formula_expression* expr);

		bool isCommand() const { return true; }
	private:
		virtual void execute(FormulaCallable& context) const = 0;
		variant getValue(const std::string& key) const { return variant(); }
		void getInputs(std::vector<game_logic::formula_input>* inputs) const {}

		//these two members are a more compiler-friendly version of a
		//intrusive_ptr<formula_expression>
		const formula_expression* expr_;
		boost::intrusive_ptr<const reference_counted_object> expr_holder_;
	};

	class fn_command_callable : public command_callable {
	public:
		explicit fn_command_callable(std::function<void()> fn);
	private:
		virtual void execute(FormulaCallable& context) const;
		std::function<void()> fn_;
	};

	class fn_command_callable_arg : public command_callable {
	public:
		explicit fn_command_callable_arg(std::function<void(FormulaCallable*)> fn);
	private:
		virtual void execute(FormulaCallable& context) const;
		std::function<void(FormulaCallable*)> fn_;
	};
}
