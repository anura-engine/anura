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

#include "formula_garbage_collector.hpp"
#include "variant.hpp"

namespace game_logic
{
	enum class FORMULA_ACCESS_TYPE { READ_ONLY, WRITE_ONLY, READ_WRITE };
	struct FormulaInput 
	{
		std::string name;
		FORMULA_ACCESS_TYPE access;
		FormulaInput(const std::string& name, FORMULA_ACCESS_TYPE access=FORMULA_ACCESS_TYPE::READ_WRITE)
				: name(name), access(access)
		{}
	};

	class FormulaCallableVisitor;

	//interface for objects that can have formulae run on them
	class FormulaCallable : public GarbageCollectible
	{
	public:
		explicit FormulaCallable(bool has_self=false) : has_self_(has_self)
		{}

		explicit FormulaCallable(GARBAGE_COLLECTOR_EXCLUDE_OPTIONS options) : has_self_(false), GarbageCollectible(options)
		{}

		std::string queryId() const { return getObjectId(); }

		variant queryValue(const std::string& key) const {
			if(has_self_ && key == "self") {
				return variant(this);
			}
			return getValue(key);
		}

		variant queryValueBySlot(int slot) const {
			return getValueBySlot(slot);
		}

		bool queryConstantValue(const std::string& key, variant* value) const {
			return getConstantValue(key, value);
		}

		void mutateValue(const std::string& key, const variant& value) {
			setValue(key, value);
		}

		void mutateValueBySlot(int slot, const variant& value) {
			setValueBySlot(slot, value);
		}

		std::vector<FormulaInput> inputs() const {
			std::vector<FormulaInput> res;
			getInputs(&res);
			return res;
		}

		bool equals(const FormulaCallable* other) const {
			return doCompare(other) == 0;
		}

		bool less(const FormulaCallable* other) const {
			return doCompare(other) < 0;
		}

		virtual std::string toDebugString() const { return ""; }

		virtual void getInputs(std::vector<FormulaInput>* /*inputs*/) const {};

		void serialize(std::string& str) const {
			serializeToString(str);
		}

		bool has_key(const std::string& key) const 
			{ return !queryValue(key).is_null(); }

		// In order to provide support for widgets to be able to have FFL handlers for events
		// The following two functions are provided for them to use to respectively execute
		// a command and create a new formula from a variant (which is expected to contain FFL 
		// commands).  If you're making some new that object that provides a custom symbol
		// table or supports different types of CommandCallable you should override these 
		// two functions to provide widget support.
		virtual bool executeCommand(const variant &v);
		virtual FormulaPtr createFormula(const variant& v);

		//is some kind of command to the engine.
		virtual bool isCommand() const { return false; }
		virtual bool isCairoOp() const { return false; }

		void performVisitValues(FormulaCallableVisitor& visitor) {
			visitValues(visitor);
		}

	protected:
		virtual ~FormulaCallable() {}

		virtual variant getValueDefault(const std::string& key) const { return variant(); }
		virtual void setValueDefault(const std::string& key, const variant& value) {}

		virtual void setValue(const std::string& key, const variant& value);
		virtual void setValueBySlot(int slot, const variant& value);
		virtual int doCompare(const FormulaCallable* callable) const {
			return this < callable ? -1 : (this == callable ? 0 : 1);
		}

		virtual void serializeToString(std::string& str) const;

		virtual void visitValues(FormulaCallableVisitor& visitor) {}
	private:
		virtual variant getValue(const std::string& key) const = 0;
		virtual variant getValueBySlot(int slot) const;

		virtual bool getConstantValue(const std::string& key, variant* value) const {
			return false;
		}

		virtual std::string getObjectId() const { return "FormulaCallable"; }

		bool has_self_;
	};

	class FormulaCallableNoRefCount : public FormulaCallable {
	public:
		FormulaCallableNoRefCount() {
			turn_reference_counting_off();
		}
		virtual ~FormulaCallableNoRefCount() {}
	};

	class FormulaCallableWithBackup : public FormulaCallable {
		const FormulaCallable& main_;
		const FormulaCallable& backup_;

		void setValueBySlot(int slot, const variant& value) override {
			const_cast<FormulaCallable&>(backup_).mutateValueBySlot(slot, value);
		}

		variant getValueBySlot(int slot) const override {
			return backup_.queryValueBySlot(slot);
		}

		variant getValue(const std::string& key) const override {
			variant var = main_.queryValue(key);
			if(var.is_null()) {
				return backup_.queryValue(key);
			}

			return var;
		}

		void getInputs(std::vector<FormulaInput>* inputs) const override {
			main_.getInputs(inputs);
			backup_.getInputs(inputs);
		}
	public:
		FormulaCallableWithBackup(const FormulaCallable& main, const FormulaCallable& backup) : FormulaCallable(false), main_(main), backup_(backup)
		{}
	};

	class FormulaVariantCallableWithBackup : public FormulaCallable {
		variant var_;
		const FormulaCallable& backup_;
		variant getValue(const std::string& key) const override {
			variant var = var_.get_member(key);
			if(var.is_null()) {
				return backup_.queryValue(key);
			}

			return var;
		}

		void setValueBySlot(int slot, const variant& value) override {
			const_cast<FormulaCallable&>(backup_).mutateValueBySlot(slot, value);
		}

		variant getValueBySlot(int slot) const override {
			return backup_.queryValueBySlot(slot);
		}

		void getInputs(std::vector<FormulaInput>* inputs) const override {
			backup_.getInputs(inputs);
		}

	public:
		FormulaVariantCallableWithBackup(const variant& var, const FormulaCallable& backup) : FormulaCallable(false), var_(var), backup_(backup)
		{}

		void surrenderReferences(GarbageCollector* collector) override {
			collector->surrenderVariant(&var_);
		}
	};

	class MapFormulaCallable : public FormulaCallable {
	public:
		explicit MapFormulaCallable(variant node);
		explicit MapFormulaCallable(const FormulaCallable* fallback=nullptr);
		explicit MapFormulaCallable(const std::map<std::string, variant>& m);
		variant write() const;
		MapFormulaCallable& add(const std::string& key, const variant& value);
		void setFallback(const FormulaCallable* fallback) { fallback_ = fallback; }

		//adds an entry and gets direct access to the variant. Use with caution
		//and for cases where calling add() repeatedy isn't efficient enough.
		variant& addDirectAccess(const std::string& key);

		bool empty() const { return values_.empty(); }
		void clear() { values_.clear(); }
		bool contains(const std::string& key) const { return values_.count(key) != 0; }

		const std::map<std::string, variant>& values() const { return values_; }

		typedef std::map<std::string,variant>::const_iterator const_iterator;

		const_iterator begin() const { return values_.begin(); }
		const_iterator end() const { return values_.end(); }

		variant& ref(const std::string& key) { return values_[key]; }

		void surrenderReferences(GarbageCollector* collector) override;

	private:
		//MapFormulaCallable(const MapFormulaCallable&);

		variant getValueBySlot(int slot) const override {
			return fallback_->queryValueBySlot(slot);
		}

		void setValueBySlot(int slot, const variant& value) override {
			const_cast<FormulaCallable*>(fallback_)->mutateValueBySlot(slot, value);
		}

		virtual void visitValues(FormulaCallableVisitor& visitor) override;

		variant getValue(const std::string& key) const override;
		void getInputs(std::vector<FormulaInput>* inputs) const override;
		void setValue(const std::string& key, const variant& value) override;
		std::map<std::string,variant> values_;
		const FormulaCallable* fallback_;
	};

	typedef ffl::IntrusivePtr<FormulaCallable> FormulaCallablePtr;
	typedef ffl::IntrusivePtr<const FormulaCallable> ConstFormulaCallablePtr;

	typedef ffl::IntrusivePtr<MapFormulaCallable> MapFormulaCallablePtr;
	typedef ffl::IntrusivePtr<const MapFormulaCallable> ConstMapFormulaCallablePtr;

	class FormulaExpression;

	class CommandCallable : public FormulaCallable {
	public:
		CommandCallable();
		void runCommand(FormulaCallable& context) const;

		void setExpression(const FormulaExpression* expr);

		bool isCommand() const override { return true; }

		std::string toDebugString() const override { std::string s = typeid(*this).name(); return "(Command Object: " + s + ")"; }
	private:
		virtual void execute(FormulaCallable& context) const = 0;
		variant getValue(const std::string& key) const override { return variant(); }
		void getInputs(std::vector<game_logic::FormulaInput>* inputs) const override {}

		//these two members are a more compiler-friendly version of a
		//intrusive_ptr<FormulaExpression>
		const FormulaExpression* expr_;
		ffl::IntrusivePtr<const reference_counted_object> expr_holder_;
	};

	class FnCommandCallable : public CommandCallable {
	public:
		FnCommandCallable(const char* name, std::function<void()> fn);

		std::string debugObjectName() const override;
	private:
		virtual void execute(FormulaCallable& context) const override;
		const char* name_;
		std::function<void()> fn_;
	};

	class FnCommandCallableArg : public CommandCallable {
	public:
		FnCommandCallableArg(const char* name, std::function<void(FormulaCallable*)> fn);
		std::string debugObjectName() const override;
	private:
		virtual void execute(FormulaCallable& context) const override;
		const char* name_;
		std::function<void(FormulaCallable*)> fn_;
	};

	variant deferCurrentCommandSequence();
}
