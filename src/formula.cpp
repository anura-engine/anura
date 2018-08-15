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

#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <boost/optional/optional.hpp>
#include <cmath>
#include <future>
#include <stack>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <limits.h>


#include "asserts.hpp"
#include "formatter.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "formula_constants.hpp"
#include "formula_function.hpp"
#include "formula_interface.hpp"
#include "formula_object.hpp"
#include "formula_profiler.hpp"
#include "formula_tokenizer.hpp"
#include "formula_vm.hpp"
#include "formula_where.hpp"
#include "i18n.hpp"
#include "lua_iface.hpp"
#include "preferences.hpp"
#include "random.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"
#include "utf8_to_codepoint.hpp"
#include "variant_type.hpp"
#include "variant_type_check.hpp"
#include "variant_utils.hpp"

#define STRICT_ERROR(s) if(g_strict_formula_checking_warnings) { LOG_WARN(s); } else { ASSERT_LOG(false, s); }
#define STRICT_ASSERT(cond, s) if(!(cond)) { STRICT_ERROR(s); }

PREF_INT(max_ffl_recursion, 100, "Maximum depth of FFL recursion");

using namespace formula_vm;

namespace 
{
	PREF_BOOL(ffl_vm_opt_library_lookups, true, "Optimize library lookups in VM");
	PREF_BOOL(ffl_vm_opt_constant_lookups, true, "Optimize contant lookups in VM");
	PREF_BOOL(ffl_vm_opt_inline, true, "Try to inline FFL calls.");
	PREF_BOOL(ffl_vm_opt_replace_where, true, "Try to replace trivial where calls.");

	//the last formula that was executed; used for outputting debugging info.
	const game_logic::Formula* last_executed_formula;

	bool g_verbatim_string_expressions = false;

	bool g_strict_formula_checking = false;
	bool g_strict_formula_checking_warnings = false;

	std::set<game_logic::Formula*>& all_formulae() 
	{
		static std::set<game_logic::Formula*>* instance = new std::set<game_logic::Formula*>;
		return *instance;
	}
}

std::string output_formula_error_info() 
{
	if(last_executed_formula) {
		return last_executed_formula->outputDebugInfo();
	}
	return "";
}

namespace game_logic
{
	const std::set<Formula*>& Formula::getAll() {
		return all_formulae();
	}

	void set_verbatim_string_expressions(bool verbatim) {
		g_verbatim_string_expressions = verbatim;
	}

	WhereVariables::WhereVariables(const FormulaCallable &base, WhereVariablesInfoPtr info)
		: FormulaCallable(false), base_(&base), info_(info)
		{}

	void WhereVariables::surrenderReferences(GarbageCollector* collector) {
		collector->surrenderPtr(&base_, "base");
		for(CacheEntry& v : results_cache_) {
			collector->surrenderVariant(&v.result);
		}
	}

	void WhereVariables::setValueBySlot(int slot, const variant& value) {
		ASSERT_LOG(slot < info_->base_slot, "Illegal set on immutable where variables " << slot);
		const_cast<FormulaCallable*>(base_.get())->mutateValueBySlot(slot, value);
	}

	void WhereVariables::setValue(const std::string& key, const variant& value) {
		const_cast<FormulaCallable*>(base_.get())->mutateValue(key, value);
	}

	variant WhereVariables::getValueBySlot(int slot) const {
		if(slot >= info_->base_slot) {
			slot -= info_->base_slot;
			if(static_cast<unsigned>(slot) < results_cache_.size() && results_cache_[slot].have_result) {
				return results_cache_[slot].result;
			} else {
				variant result = info_->entries[slot]->evaluate(*this);
				if(results_cache_.size() <= static_cast<unsigned>(slot)) {
					results_cache_.resize(slot+1);
				}

				results_cache_[slot].result = result;
				results_cache_[slot].have_result = true;
				return result;
			}
		}

		return base_->queryValueBySlot(slot);
	}

	variant WhereVariables::getValue(const std::string& key) const {
		const variant result = base_->queryValue(key);
		if(result.is_null()) {
			std::vector<std::string>::const_iterator i = std::find(info_->names.begin(), info_->names.end(), key);
			if(i != info_->names.end()) {
				const int slot = static_cast<int>(i - info_->names.begin());
				return getValueBySlot(info_->base_slot + slot);
			}
		}
		return result;
	}
	
	void FormulaCallable::setValue(const std::string& key, const variant& /*value*/)
	{
		LOG_ERROR("cannot set key '" << key << "' on object");
	}

	void FormulaCallable::setValueBySlot(int slot, const variant& /*value*/)
	{
		LOG_ERROR("cannot set slot '" << slot << "' on object");
	}

	variant FormulaCallable::getValueBySlot(int slot) const
	{
		ASSERT_LOG(false, "Could not get value by slot from formula callable " << typeid(*this).name() << ": " << slot);
		return variant(0); //so VC++ doesn't complain
	}

	void FormulaCallable::serializeToString(std::string& str) const
	{
		if(preferences::serialize_bad_objects()) {
			//force serialization of this through so we can work out what's going on.
			str += "(UNSERIALIZABLE_OBJECT " + std::string(typeid(*this).name()) + ")";
			return;
		}

		throw type_error("Tried to serialize type which cannot be serialized");
	}

	bool FormulaCallable::executeCommand(const variant &v) 
	{
		if(v.is_null()) {
			return true;
		}

		if(v.is_function()) {
			std::vector<variant> args;
			variant cmd = v(args);
			executeCommand(cmd);
		} else if(v.is_list()) {
			for(int n = 0; n != v.num_elements(); ++n) {
				executeCommand(v[n]);
			}
		} else {
			CommandCallable* callable = v.try_convert<CommandCallable>();
			if(callable) {
				callable->runCommand(*this);
			} else if(variant_type::get_commands()->match(v)) {
				ASSERT_LOG(false, "RUNNING CUSTOM OBJECT COMMANDS IN A NON-CUSTOM OBJECT CONTEXT: " << v.to_debug_string() << "\nFORMULA INFO: " << output_formula_error_info() << "\n");
			} else {
				ASSERT_LOG(false, "EXPECTED EXECUTABLE COMMAND OBJECT, INSTEAD FOUND: " << v.to_debug_string() << "\nFORMULA INFO: " << output_formula_error_info() << "\n");
			}
		}

		return true;
	}

	variant_type_ptr VariantExpression::getVariantType() const {
		if(type_override_) {
			return type_override_;
		}

		return get_variant_type_from_value(v_);
	}

	CommandCallable::CommandCallable() : expr_(nullptr)
	{
	}

	void CommandCallable::runCommand(FormulaCallable& context) const
	{
		if(expr_) {
			try {
				fatal_assert_scope scope;
				execute(context);
			} catch(fatal_assert_failure_exception& e) {
				ASSERT_FATAL(e.msg << "\nERROR ENCOUNTERED WHILE RUNNING COMMAND GENERATED BY THIS EXPRESSION:\n" << expr_->debugPinpointLocation());
			}
		} else {
			execute(context);
		}
	}

	void CommandCallable::setExpression(const FormulaExpression* expr) {
		expr_ = expr;
		expr_holder_.reset(expr);
	}

	FormulaPtr FormulaCallable::createFormula(const variant& v) 
	{
		return FormulaPtr(new Formula(v, 0));
	}

	MapFormulaCallable::MapFormulaCallable(variant node)
	  : FormulaCallable(false), fallback_(nullptr)
	{
		for(const auto& value : node.as_map()) {
			values_[value.first.as_string()] = value.second;
		}
	}
	
	MapFormulaCallable::MapFormulaCallable(const FormulaCallable* fallback) 
		: FormulaCallable(false), 
		fallback_(fallback)
	{}
	
	MapFormulaCallable::MapFormulaCallable(const std::map<std::string, variant>& values) 
		: FormulaCallable(false), 
		fallback_(nullptr), 
		values_(values)
	{}

	void MapFormulaCallable::surrenderReferences(GarbageCollector* collector) 
	{
		for(std::pair<const std::string,variant>& p : values_) {
			collector->surrenderVariant(&p.second);
		}
	}
	
	MapFormulaCallable& MapFormulaCallable::add(const std::string& key, const variant& value)
	{
		values_[key] = value;
		return *this;
	}

	variant& MapFormulaCallable::addDirectAccess(const std::string& key)
	{
		return values_[key];
	}
	
	variant MapFormulaCallable::getValue(const std::string& key) const
	{
		std::map<std::string, variant>::const_iterator itor = values_.find(key);
		if(itor == values_.end()) {
			if(fallback_) {
				return fallback_->queryValue(key);
			} else {
				return variant();
			}
		} else {
			return itor->second;
		}
	}

	variant MapFormulaCallable::write() const
	{
		variant_builder result;
		for(std::map<std::string, variant>::const_iterator i = values_.begin();
		    i != values_.end(); ++i) {
			result.add(i->first, i->second);
		}
		return result.build();
	}
	
	void MapFormulaCallable::getInputs(std::vector<FormulaInput>* inputs) const
	{
		if(fallback_) {
			fallback_->getInputs(inputs);
		}
		for(std::map<std::string,variant>::const_iterator i = values_.begin(); i != values_.end(); ++i) {
			inputs->push_back(FormulaInput(i->first, FORMULA_ACCESS_TYPE::READ_WRITE));
		}
	}
	
	void MapFormulaCallable::setValue(const std::string& key, const variant& value)
	{
		values_[key] = value;
	}
	
	namespace 
	{
		class VMExpression : public FormulaExpression {
		public:
			VMExpression(VirtualMachine& vm, variant_type_ptr t, const FormulaExpression& o) : FormulaExpression("_vm"), vm_(vm), type_(t), can_reduce_to_variant_(false)
			{
				setDebugInfo(o);
				setVMDebugInfo(vm_);
				t->set_expr(this);
			}

			bool canCreateVM() const override {
				return true;
			}

			void emitVM(formula_vm::VirtualMachine& vm) const override {
				vm.append(vm_);
			}

			variant executeMember(const FormulaCallable& variables, std::string& id, variant* variant_id) const override {
				ASSERT_LOG(false, "executemember on VMExpression");
			}

			std::string debugOutput() const { return vm_.debugOutput(); }

			bool isVM() const override { return true; }

			bool canReduceToVariant(variant& v) const override {
				v = variant_;
				return can_reduce_to_variant_;
			}

			void setVariant(const variant& v) {
				variant_ = v;
				can_reduce_to_variant_ = true;
			}

			formula_vm::VirtualMachine& get_vm() { return vm_; }
			const formula_vm::VirtualMachine& get_vm() const { return vm_; }

		private:
			variant execute(const FormulaCallable& variables) const override {
//				Formula::failIfStaticContext();

				variant result = vm_.execute(variables);
				return result;
			}

			variant_type_ptr getVariantType() const override {
				return type_;
			}

			formula_vm::VirtualMachine vm_;
			variant_type_ptr type_;

			variant variant_;
			bool can_reduce_to_variant_;
		};

		#if defined(USE_LUA)
		class LuaFnExpression : public FormulaExpression {
		public:
			explicit LuaFnExpression(lua::LuaFunctionReference* fn_ref) 
				: fn_ref_(fn_ref)
			{
			}
			variant execute(const FormulaCallable& variables) const override
			{
				return fn_ref_->call();
			}
		private:
			variant_type_ptr getVariantType() const override {
				return variant_type::get_any();
			}
			lua::LuaFunctionReferencePtr fn_ref_;
		};
		#endif
		
		class FunctionListExpression : public FormulaExpression {
		public:
			explicit FunctionListExpression(FunctionSymbolTable *symbols)
			: FormulaExpression("_function_list"), symbols_(symbols)
			{}

		private:
			variant_type_ptr getVariantType() const override {
				return variant_type::get_list(variant_type::get_type(variant::VARIANT_TYPE_STRING));
			}
			variant execute(const FormulaCallable& /*variables*/) const override {
				std::vector<variant> res;
				std::vector<std::string> function_names = builtin_function_names();
				std::vector<std::string> more_function_names = symbols_->getFunctionNames();
				function_names.insert(function_names.end(), more_function_names.begin(), more_function_names.end());
				for(size_t i = 0; i < function_names.size(); i++) {
					res.push_back(variant(function_names[i]));
				}
				return variant(&res);
			}
	
			FunctionSymbolTable* symbols_;
		};

		class ListExpression : public FormulaExpression {
		public:
			explicit ListExpression(const std::vector<ExpressionPtr>& items)
			: FormulaExpression("_list"), items_(items)
			{}

		private:
			variant_type_ptr getVariantType() const override {
				std::vector<variant_type_ptr> types;
				for(const ExpressionPtr& item : items_) {
					variant_type_ptr new_type = item->queryVariantType();
					types.push_back(new_type);
				}

				return variant_type::get_specific_list(types);
			}

			//a special version of static evaluation that doesn't save a
			//reference to the list, so that we can allow static evaluation
			//not to be fooled.
			variant staticEvaluate(const FormulaCallable& variables) const override {
				std::vector<variant> res;
				res.reserve(items_.size());
				for(std::vector<ExpressionPtr>::const_iterator i = items_.begin(); i != items_.end(); ++i) {
					res.push_back((*i)->evaluate(variables));
				}

				return variant(&res);
			}

			variant execute(const FormulaCallable& variables) const override {
				return staticEvaluate(variables);
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				return std::vector<ConstExpressionPtr>(items_.begin(), items_.end());
			}

			bool canCreateVM() const override { return canChildrenVM(); }

			ExpressionPtr optimizeToVM() override {
				bool can_vm = true;
				for(ExpressionPtr& e : items_) {
					optimizeChildToVM(e);
					if(e->canCreateVM() == false) {
						can_vm = false;
					}
				}

				if(can_vm) {
					formula_vm::VirtualMachine vm;
					for(ExpressionPtr& e : items_) {
						e->emitVM(vm);
					}

					vm.addLoadConstantInstruction(variant(static_cast<int>(items_.size())));

					vm.addInstruction(OP_LIST);
					return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
				}

				return ExpressionPtr();
			}
	
			std::vector<ExpressionPtr> items_;
		};

		class ListComprehensionExpression : public FormulaExpression {
		public:
			ListComprehensionExpression(ExpressionPtr expr, const std::map<std::string, ExpressionPtr>& generators, const std::vector<ExpressionPtr>& filters, int base_slot)
			  : FormulaExpression("_list_compr"), expr_(expr), generators_(generators), filters_(filters), base_slot_(base_slot)
			{
				for(std::map<std::string,ExpressionPtr>::const_iterator i = generators.begin(); i != generators.end(); ++i) {
					generator_names_.push_back(i->first);
				}
			}
	
		private:
			variant_type_ptr getVariantType() const override {
				return variant_type::get_list(expr_->queryVariantType());
			}

			variant execute(const FormulaCallable& variables) const override {
				std::vector<int> nelements;
				std::vector<variant> lists;
				for(std::map<std::string, ExpressionPtr>::const_iterator i = generators_.begin(); i != generators_.end(); ++i) {
					lists.push_back(i->second->evaluate(variables));
					nelements.push_back(lists.back().num_elements());
					if(nelements.back() == 0) {
						std::vector<variant> items;
						return variant(&items);
					}
				}

				std::vector<variant> result;

				std::vector<variant*> args;

				ffl::IntrusivePtr<SlotFormulaCallable> callable;

				std::vector<int> indexes(lists.size());
				for(;;) {
					if(callable.get() == nullptr || callable->refcount() > 1) {
						args.clear();

						callable.reset(new SlotFormulaCallable);
						callable->setFallback(&variables);
						callable->setBaseSlot(base_slot_);
						callable->reserve(generator_names_.size());
						for(const std::string& arg : generator_names_) {
							callable->add(variant());
							args.push_back(&callable->backDirectAccess());
						}
					}

					for(int n = 0; n != indexes.size(); ++n) {
						*args[n] = lists[n][indexes[n]];
					}

					bool passes = true;
					for(const ExpressionPtr& filter : filters_) {
						if(filter->evaluate(*callable).as_bool() == false) {
							passes = false;
							break;
						}
					}

					if(passes) {
						result.push_back(expr_->evaluate(*callable));
					}

					if(!incrementVec(indexes, nelements)) {
						break;
					}
				}
		
				return variant(&result);
			}

			static bool incrementVec(std::vector<int>& v, const std::vector<int>& max_values) {
				int index = 0;
				while(index != v.size()) {
					if(++v[index] < max_values[index]) {
						return true;
					}

					v[index] = 0;
					++index;
				}

				return false;
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(expr_);
				for(std::map<std::string, ExpressionPtr>::const_iterator i = generators_.begin(); i != generators_.end(); ++i) {
					result.push_back(i->second);
				}

				result.insert(result.end(), filters_.begin(), filters_.end());
				return result;
			}

			bool canCreateVM() const override { return canChildrenVM(); }

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(expr_);
				bool can_vm = expr_->canCreateVM();
				for(std::map<std::string, ExpressionPtr>::iterator i = generators_.begin(); i != generators_.end(); ++i) {
					optimizeChildToVM(i->second);
					can_vm = can_vm && i->second->canCreateVM();
				}

				for(ExpressionPtr& f : filters_) {
					optimizeChildToVM(f);
					can_vm = can_vm && f->canCreateVM();
				}

				if(!can_vm) {
					return ExpressionPtr();
				}

				formula_vm::VirtualMachine vm;

				for(std::map<std::string, ExpressionPtr>::const_iterator i = generators_.begin(); i != generators_.end(); ++i) {
					i->second->emitVM(vm);
				}

				vm.addInstruction(formula_vm::OP_PUSH_INT);
				vm.addInt(static_cast<int>(generators_.size()));

				vm.addInstruction(formula_vm::OP_PUSH_INT);
				vm.addInt(base_slot_);

				const int jump_source = vm.addJumpSource(OP_ALGO_COMPREHENSION);


				for(ExpressionPtr& f : filters_) {
					f->emitVM(vm);
					vm.addInstruction(formula_vm::OP_UNARY_NOT);
					vm.addInstruction(formula_vm::OP_BREAK_IF);
				}

				expr_->emitVM(vm);

				vm.jumpToEnd(jump_source);

				return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
			}

			ExpressionPtr expr_;
			std::map<std::string, ExpressionPtr> generators_;
			std::vector<std::string> generator_names_;
			std::vector<ExpressionPtr> filters_;
			int base_slot_;
		};

		class MapExpression : public FormulaExpression {
		public:
			explicit MapExpression(const std::vector<ExpressionPtr>& items)
			: FormulaExpression("_map"), items_(items)
			{}
	
		private:
			variant_type_ptr getVariantType() const override {
				std::map<variant, variant_type_ptr> types;

				std::vector<variant_type_ptr> key_types, value_types;

				bool is_specific_map = true;

				for(std::vector<ExpressionPtr>::const_iterator i = items_.begin(); ( i != items_.end() ) && ( i+1 != items_.end() ) ; i+=2) {

					variant key_value;
					if(!(*i)->canReduceToVariant(key_value) || !key_value.is_string()) {
						is_specific_map = false;
					}

					variant_type_ptr new_key_type = (*i)->queryVariantType();
					variant_type_ptr new_value_type = (*(i+1))->queryVariantType();

					types[key_value] = new_value_type;

					for(const variant_type_ptr& existing : key_types) {
						if(existing->is_equal(*new_key_type)) {
							new_key_type.reset();
							break;
						}
					}

					if(new_key_type) {
						key_types.push_back(new_key_type);
					}

					for(const variant_type_ptr& existing : value_types) {
						if(existing->is_equal(*new_value_type)) {
							new_value_type.reset();
							break;
						}
					}

					if(new_value_type) {
						value_types.push_back(new_value_type);
					}
				}

				if(is_specific_map && !types.empty()) {
					return variant_type::get_specific_map(types);
				}

				variant_type_ptr key_type, value_type;

				if(key_types.size() == 1) {
					key_type = key_types[0];
				} else {
					key_type = variant_type::get_union(key_types);
				}

				if(value_types.size() == 1) {
					value_type = value_types[0];
				} else {
					value_type = variant_type::get_union(value_types);
				}

				return variant_type::get_map(key_type, value_type);
			}

			variant execute(const FormulaCallable& variables) const override {
				//since maps can be modified we want any map construction to return
				//a brand new map.
				Formula::failIfStaticContext();

				std::map<variant,variant> res;
				for(std::vector<ExpressionPtr>::const_iterator i = items_.begin(); ( i != items_.end() ) && ( i+1 != items_.end() ) ; i+=2) {
					variant key = (*i)->evaluate(variables);
					variant value = (*(i+1))->evaluate(variables);
					res[ key ] = value;
				}
		
				variant result(&res);
				result.set_source_expression(this);
				return result;
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result(items_.begin(), items_.end());
				return result;
			}

			bool canCreateVM() const override { return canChildrenVM(); }

			ExpressionPtr optimizeToVM() override {
				bool can_vm = true;
				for(ExpressionPtr& i : items_) {
					optimizeChildToVM(i);
					if(i->canCreateVM() == false) {
						can_vm = false;
					}
				}

				if(can_vm) {
					formula_vm::VirtualMachine vm;
					for(ExpressionPtr& e : items_) {
						e->emitVM(vm);
					}

					vm.addLoadConstantInstruction(variant(static_cast<int>(items_.size())));

					vm.addInstruction(OP_MAP);
					return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
				}
				return ExpressionPtr();
			}
	
			std::vector<ExpressionPtr> items_;
		};

		class UnaryOperatorExpression : public FormulaExpression {
		public:
			UnaryOperatorExpression(const std::string& op, ExpressionPtr arg)
			: FormulaExpression("_unary"), operand_(arg)
			{
				if(op == "not") {
					op_ = OP::NOT;
				} else if(op == "-") {
					op_ = OP::SUB;
				} else {
					ASSERT_LOG(false, "illegal unary operator: '" << op << "'\n" << arg->debugPinpointLocation());
				}
			}
		private:
			variant_type_ptr getVariantType() const override {
				switch(op_) {
				case OP::NOT: return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
				case OP::SUB:
				default:
					if(operand_->queryVariantType()->is_type(variant::VARIANT_TYPE_INT)) {
						return variant_type::get_type(variant::VARIANT_TYPE_INT);
					} else {
						return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
					}
				}
			}

			variant execute(const FormulaCallable& variables) const override {
				const variant res = operand_->evaluate(variables);
				switch(op_) {
					case OP::NOT: 
						return res.as_bool() ? variant::from_bool(false) : variant::from_bool(true);
					case OP::SUB: 
					default: 
						return -res;
				}
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(operand_);
				return result;
			}

			bool canCreateVM() const override { return canChildrenVM(); }

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(operand_);
				if(operand_->canCreateVM()) {
					formula_vm::VirtualMachine vm;
					operand_->emitVM(vm);
					if(op_ == OP::NOT) {
						vm.addInstruction(OP_UNARY_NOT);
					} else {
						vm.addInstruction(OP_UNARY_SUB);
					}

					return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
				}
				return ExpressionPtr();
			}

			enum class OP { NOT, SUB };
			OP op_;
			ExpressionPtr operand_;
		};

		class ConstIdentifierExpression : public FormulaExpression {
		public:
			explicit ConstIdentifierExpression(const std::string& id)
			: FormulaExpression("_const_id"), v_(get_constant(id))
			{
			}
	
		private:
			variant execute(const FormulaCallable& variables) const override {
				return v_;
			}

			variant_type_ptr getVariantType() const override {
				return variant_type::get_type(v_.type());
			}
	
			variant v_;
		};

		class SlotIdentifierExpression : public FormulaExpression {
		public:
			SlotIdentifierExpression(const std::string& id, int slot, ConstFormulaCallableDefinitionPtr callable_def)
			: FormulaExpression("_slot"), slot_(slot), id_(id), callable_def_(callable_def)
			{
				const FormulaCallableDefinition::Entry* entry = callable_def_->getEntry(slot_);
				ASSERT_LOG(entry != nullptr, "COULD NOT FIND DEFINITION IN SLOT CALLABLE: " << id);
				entry->access_count++;
			}
	
			const std::string& id() const { return id_; }

			bool isIdentifier(std::string* ident) const override {
				if(ident) {
					*ident = id_;
				}

				return true;
			}

			ConstFormulaCallableDefinitionPtr getTypeDefinition() const override {
				const FormulaCallableDefinition::Entry* def = callable_def_->getEntry(slot_);
				ASSERT_LOG(def, "DID NOT FIND EXPECTED DEFINITION");
				if(def->type_definition) {
					return def->type_definition;
				} else {
					return FormulaExpression::getTypeDefinition();
				}
			}

			int getSlot() const { return slot_; }
			const FormulaCallableDefinition& getDefinition() const { return *callable_def_; }

			variant_type_ptr variant_type() const { return callable_def_->getEntry(slot_)->variant_type; }

			bool canCreateVM() const override { return true; }
			void emitVM(formula_vm::VirtualMachine& vm) const override {
				const FormulaCallableDefinition::Entry* def = callable_def_->getEntry(slot_);

				variant v;
				if(def != nullptr && def->constant_fn && def->constant_fn(&v)) {
					vm.addLoadConstantInstruction(v);
					return;
				}

				int index = -1;
				if(false && callable_def_->getSymbolIndexForSlot(slot_, &index)) {
					vm.addInstruction(formula_vm::OP_LOOKUP_SYMBOL_STACK);
					vm.addInt(index);
				} else {
					vm.addInstruction(formula_vm::OP_LOOKUP);
					vm.addInt(slot_);
				}
			}

			ExpressionPtr optimizeToVM() override {
				formula_vm::VirtualMachine vm;
				emitVM(vm);
				return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
			}

		private:
			variant executeMember(const FormulaCallable& variables, std::string& id, variant* variant_id) const override {
				id = id_;
				return variables.queryValue("self");
			}
	
			variant execute(const FormulaCallable& variables) const override {
				Formula::failIfStaticContext();
				return variables.queryValueBySlot(slot_);
			}

			variant_type_ptr getVariantType() const override {
				return callable_def_->getEntry(slot_)->variant_type;
			}

			variant_type_ptr getMutableType() const override {
				return callable_def_->getEntry(slot_)->getWriteType();
			}

			ConstFormulaCallableDefinitionPtr getModifiedDefinitionBasedOnResult(bool result, ConstFormulaCallableDefinitionPtr current_def, variant_type_ptr expression_is_this_type) const override {
				variant_type_ptr current_type = getVariantType();
				if(result && current_type) {
					variant_type_ptr new_type;
					if(expression_is_this_type) {
						new_type = expression_is_this_type;
					} else {
						new_type = variant_type::get_null_excluded(current_type);
					}

					if(new_type != current_type) {
						FormulaCallableDefinitionPtr new_def = modify_formula_callable_definition(current_def, slot_, new_type);
						return new_def;
					}
				}

				if(!result && current_type && expression_is_this_type) {
					variant_type_ptr new_type = variant_type::get_with_exclusion(current_type, expression_is_this_type);
					if(new_type != current_type) {
						FormulaCallableDefinitionPtr new_def = modify_formula_callable_definition(current_def, slot_, new_type);
						return new_def;
					}
				}

				return nullptr;
			}

			void staticErrorAnalysis() const override {
				const FormulaCallableDefinition::Entry* entry = callable_def_->getEntry(slot_);
				ASSERT_LOG(entry != nullptr, "COULD NOT FIND DEFINITION IN SLOT CALLABLE: " << id_ << " " << debugPinpointLocation());
				ASSERT_LOG(entry->isPrivate() == false, "Identifier " << id_ << " is private " << debugPinpointLocation());
			}

			int slot_;
			std::string id_;
			ConstFormulaCallableDefinitionPtr callable_def_;
		};

namespace {
	// Calculates the edit distance between two strings.
	class edit_distance_calculator {
	private:
		const std::string & a_;
		const std::string & b_;

		std::vector< std::vector<size_t> > cache_;

	public:
		edit_distance_calculator(const std::string & a, const std::string & b)
			: a_(a)
			, b_(b)
			, cache_(a.size() + 1, std::vector<size_t>(b.size() + 1))
		{
			// cache_ stores the calculated edit distance between initial segments of a and b

			for (size_t j = 0; j <= b.size(); ++j) {
				cache_[0][j] = j;
			}
			for (size_t i = 1; i <= a.size(); ++i) {
				cache_[i][0] = i;
				for (size_t j = 1; j <= b.size(); ++j) {
					size_t replaced = cache_[i-1][j-1] + ((a[i-1] == b[j-1]) ? 0 : 1);
					size_t inserted = cache_[i-1][j] + 1;
					size_t deleted  = cache_[i][j-1] + 1;
					size_t min = std::min(replaced, std::min(inserted, deleted));
					// transposition
					if (i > 1 && j > 1 && a[i-1] == b[j-2] && a[i-2] == b[j-1]) {
						min = std::min(min, cache_[i-2][j-2] + 1);
					}
					cache_[i][j] = min;
				}
			}
		}
		size_t operator()() {
			return cache_[a_.size()][b_.size()];
		}
	};
}

		class IdentifierExpression : public FormulaExpression {
		public:
			IdentifierExpression(const std::string& id, ConstFormulaCallableDefinitionPtr callable_def)
			: FormulaExpression("_id"), id_(id), callable_def_(callable_def)
			{
			}
	
			const std::string& id() const { return id_; }

			bool isIdentifier(std::string* ident) const override {
				if(ident) {
					*ident = id_;
				}

				return true;
			}

			void set_function(ExpressionPtr fn) { function_ = fn; }

			ExpressionPtr optimize() const override {
				if(callable_def_) {
					const int index = callable_def_->getSlot(id_);
					if(index != -1) {
						if(callable_def_->supportsSlotLookups()) {
							auto entry = callable_def_->getEntry(index);
							variant v;
							if(entry != nullptr && entry->constant_fn && entry->constant_fn(&v)) {
								return ExpressionPtr(new VariantExpression(v));
							}

							return ExpressionPtr(new SlotIdentifierExpression(id_, index, callable_def_.get()));
						}
					} else if(callable_def_->isStrict() || g_strict_formula_checking) {

						std::vector<std::string> known_v;
						for(int n = 0; n != callable_def_->getNumSlots(); ++n) {
							known_v.push_back(callable_def_->getEntry(n)->id);
						}

						std::sort(known_v.begin(), known_v.end());
						std::string known;

						// Suggest a correction
						boost::optional<std::string> candidate_match;
						size_t candidate_value = std::min(static_cast<size_t>(4), id_.size());
						for(const std::string& k : known_v) {
							known += k + " \n";

							size_t d = edit_distance_calculator(id_, k)();
							if (candidate_value > d) {
								candidate_match = k;
								candidate_value = d;
							} else if (candidate_value == d) {
								// best match so far is not unique so blank it out
								candidate_match = boost::none;
							}
						}
						std::string suggested_match = "";
						if (candidate_match) {
							suggested_match = "\nMaybe you meant '" + *candidate_match + "'?\n";
						}
						if(callable_def_->getTypeName() != nullptr) {
							STRICT_ERROR("Unknown symbol '" << id_ << "' in " << *callable_def_->getTypeName() << " " << debugPinpointLocation() << suggested_match << "\nKnown symbols: (excluding built-in functions)\n" << known << "\n");
						} else {
							STRICT_ERROR("Unknown identifier '" << id_ << "' " << debugPinpointLocation() << suggested_match << "\nIdentifiers that are valid in this scope:\n" << known << "\n");
						}
					} else if(callable_def_) {
						std::string type_name = "unk";
						if(callable_def_->getTypeName()) {
							type_name = *callable_def_->getTypeName();
						}
					}
				}

				return ExpressionPtr();
			}

			ConstFormulaCallableDefinitionPtr getTypeDefinition() const override {
				if(callable_def_) {
					const FormulaCallableDefinition::Entry* e = callable_def_->getEntry(callable_def_->getSlot(id_));
					if(e && e->type_definition) {
						return e->type_definition;
					} else {
						return FormulaExpression::getTypeDefinition();
					}
				}

				return nullptr;
			}

		private:
			ConstFormulaCallableDefinitionPtr getModifiedDefinitionBasedOnResult(bool result, ConstFormulaCallableDefinitionPtr current_def, variant_type_ptr expression_is_this_type) const override {
				if(!callable_def_) {
					return ConstFormulaCallableDefinitionPtr();
				}

				variant_type_ptr current_type = getVariantType();
				const int slot = callable_def_->getSlot(id_);
				if(result && current_type && slot != -1) {
					variant_type_ptr new_type;
					if(expression_is_this_type) {
						new_type = expression_is_this_type;
					} else {
						new_type = variant_type::get_null_excluded(current_type);
					}

					if(new_type != current_type) {
						FormulaCallableDefinitionPtr new_def = modify_formula_callable_definition(current_def, slot, new_type);
						return new_def;
					}
				}

				if(!result && current_type && expression_is_this_type) {
					variant_type_ptr new_type = variant_type::get_with_exclusion(current_type, expression_is_this_type);
					if(new_type != current_type) {
						FormulaCallableDefinitionPtr new_def = modify_formula_callable_definition(current_def, slot, new_type);
						return new_def;
					}
				}

				return nullptr;
			}

			variant executeMember(const FormulaCallable& variables, std::string& id, variant* variant_id) const override {
				id = id_;
				return variables.queryValue("self");
			}
	
			variant execute(const FormulaCallable& variables) const override {
				variant result = variables.queryValue(id_);
				if(result.is_null() && function_) {
					return function_->evaluate(variables);
				}

				return result;
			}

			bool canCreateVM() const override { return !function_; }

			ExpressionPtr optimizeToVM() override {
			//	optimizeChildToVM(left_);
				if(!function_) {
					formula_vm::VirtualMachine vm;
					vm.addLoadConstantInstruction(variant(id_));
					vm.addInstruction(formula_vm::OP_LOOKUP_STR);
					return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
				}
				return ExpressionPtr();
			}

			variant_type_ptr getVariantType() const override {

				if(callable_def_) {
					const FormulaCallableDefinition::Entry* e = callable_def_->getEntry(callable_def_->getSlot(id_));
					if(!e) {
						e = callable_def_->getDefaultEntry();
					}

					if(e) {
						return e->variant_type;
					}
				}

				return variant_type::get_any();
			}
			variant_type_ptr getMutableType() const override {

				if(callable_def_) {
					const FormulaCallableDefinition::Entry* e = callable_def_->getEntry(callable_def_->getSlot(id_));
					if(!e) {
						e = callable_def_->getDefaultEntry();
					}

					if(e) {
						return e->getWriteType();
					}
				}
				return variant_type::get_any();
			}

			std::string id_;
			ConstFormulaCallableDefinitionPtr callable_def_;

			//If this symbol is a function, this is the value we can return for it.
			ExpressionPtr function_;
		};

		class InstantiateGenericExpression : public FormulaExpression {
			ExpressionPtr left_;
			std::vector<variant_type_ptr> types_;
		public:
			InstantiateGenericExpression(variant formula_str, ExpressionPtr left, const formula_tokenizer::Token* i1, const formula_tokenizer::Token* i2)
			  : left_(left)
			{
				while(i1 != i2) {
					variant_type_ptr type = parse_variant_type(formula_str, i1, i2);
					types_.push_back(type);
					ASSERT_LOG(i1 == i2 || i1->type == formula_tokenizer::FFL_TOKEN_TYPE::COMMA, "Unexpected token while parsing generic parameters\n" << pinpoint_location(formula_str, i1->begin, i1->end));
					if(i1->type == formula_tokenizer::FFL_TOKEN_TYPE::COMMA) {
						++i1;
					}
				}
			}

		private:
			variant execute(const FormulaCallable& variables) const override {
				const variant left = left_->evaluate(variables);
				return left.instantiate_generic_function(types_);
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(left_);
				return result;
			}

			bool canCreateVM() const override { return false; }

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(left_);
				return ExpressionPtr();
			}
		};

		class GenericLambdaFunctionExpression : public FormulaExpression {
		public:
			GenericLambdaFunctionExpression(const std::vector<std::string>& args, variant fml, int base_slot, const std::vector<variant>& default_args, const std::vector<variant_type_ptr>& variant_types, const variant_type_ptr& return_type, std::shared_ptr<RecursiveFunctionSymbolTable> symbol_table, const std::vector<std::string>& generic_types, std::function<ConstFormulaPtr(const std::vector<variant_type_ptr>&)> factory) :    fml_(fml), base_slot_(base_slot), type_info_(new VariantFunctionTypeInfo), symbol_table_(symbol_table), generic_types_(generic_types), factory_(factory)
			{
				type_info_->arg_names = args;
				type_info_->default_args = default_args;
				type_info_->variant_types = variant_types;
				type_info_->return_type = return_type;

				if(!type_info_->return_type) {
					type_info_->return_type = variant_type::get_any();
				}

				type_info_->variant_types.resize(args.size());
				for(variant_type_ptr& t : type_info_->variant_types) {
					if(!t) {
						t = variant_type::get_any();
					}
				}
			}
	
		private:
			variant execute(const FormulaCallable& variables) const override {
				variant v(fml_, variables, base_slot_, type_info_, generic_types_, factory_);
				return v;
			}

			variant_type_ptr getVariantType() const override {
				return variant_type::get_function_type(type_info_->variant_types, type_info_->return_type, static_cast<int>(type_info_->variant_types.size() - type_info_->default_args.size()));
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				return result;
			}

			bool canCreateVM() const override { return false; }

			ExpressionPtr optimizeToVM() override {
				return ExpressionPtr();
			}

			variant fml_;
			int base_slot_;

			VariantFunctionTypeInfoPtr type_info_;

			std::shared_ptr<RecursiveFunctionSymbolTable> symbol_table_;
			std::vector<std::string> generic_types_;
			std::function<ConstFormulaPtr(const std::vector<variant_type_ptr>&)> factory_;
		};


		class LambdaFunctionExpression : public FormulaExpression {
		public:
			LambdaFunctionExpression(const std::vector<std::string>& args, ConstFormulaPtr fml, int base_slot, const std::vector<variant>& default_args, const std::vector<variant_type_ptr>& variant_types, const variant_type_ptr& return_type) : FormulaExpression("_lambda"), fml_(fml), base_slot_(base_slot), type_info_(new VariantFunctionTypeInfo), requires_closure_(true)
			{
				type_info_->arg_names = args;
				type_info_->default_args = default_args;
				type_info_->variant_types = variant_types;
				type_info_->return_type = return_type;

				if(!type_info_->return_type) {
					type_info_->return_type = variant_type::get_any();
				}

				type_info_->variant_types.resize(args.size());
				for(variant_type_ptr& t : type_info_->variant_types) {
					if(!t) {
						t = variant_type::get_any();
					}
				}

				static ffl::IntrusivePtr<SlotFormulaCallable> callable(new SlotFormulaCallable);
				fn_ = variant(fml_, *callable, base_slot_, type_info_);
			}

			void setNoClosure() { requires_closure_ = false; }
	
		private:
			variant execute(const FormulaCallable& variables) const override {
				if(requires_closure_) {

					return fn_.change_function_callable(variables);
				} else {
					return fn_;
				}
			}

			variant_type_ptr getVariantType() const override {
				return variant_type::get_function_type(type_info_->variant_types, type_info_->return_type, static_cast<int>(type_info_->variant_types.size() - type_info_->default_args.size()));
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(fml_->expr());
				return result;
			}

			bool canCreateVM() const override { return true; }
			
			ExpressionPtr optimizeToVM() override {
				formula_vm::VirtualMachine vm;
				vm.addLoadConstantInstruction(fn_);
				if(requires_closure_) {
					vm.addInstruction(OP_LAMBDA_WITH_CLOSURE);
				}
				return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
			}

			game_logic::ConstFormulaPtr fml_;
			int base_slot_;

			VariantFunctionTypeInfoPtr type_info_;

			bool requires_closure_;

			variant fn_;
	
		};


		namespace 
		{
			int function_recursion_depth = 0;

			#define DEBUG_FULL_EXPRESSION_STACKS
			#ifdef DEBUG_FULL_EXPRESSION_STACKS
			std::vector<ExpressionPtr> g_expr_stack;
			#endif // DEBUG_FULL_EXPRESSION_STACKS

			std::string get_expression_stack() {
				std::ostringstream s;
			#ifdef DEBUG_FULL_EXPRESSION_STACKS
				LOG_INFO("NUMBER OF FRAMES: " << g_expr_stack.size());
				for(ExpressionPtr e : g_expr_stack) {
					s << "  " << e->str() << " " << e->debugPinpointLocation() << "\n";
				}

				LOG_INFO("OUTPUT FRAMES: " << g_expr_stack.size());
			#endif // DEBUG_FULL_EXPRESSION_STACKS
				return s.str();
			}

			struct InfiniteRecursionProtector {
				explicit InfiniteRecursionProtector(const ExpressionPtr& expr) {
			#ifdef DEBUG_FULL_EXPRESSION_STACKS
					g_expr_stack.push_back(expr);
			#endif
					++function_recursion_depth;
		
					ASSERT_LOG(function_recursion_depth < g_max_ffl_recursion, "Recursion too deep. Exceeded limit of " << g_max_ffl_recursion << ". Use --max_ffl_recursion to increase this limit, though the most likely cause of this is infinite recursion. Function: " << expr->str() << "\n\ncall Stack: " << get_call_stack() << "\n\n" << get_expression_stack());
				}
				~InfiniteRecursionProtector() {
			#ifdef DEBUG_FULL_EXPRESSION_STACKS
					g_expr_stack.pop_back();
			#endif
					--function_recursion_depth;
				}
			};
		}

		class FunctionCallExpression : public FormulaExpression {
		public:
			FunctionCallExpression(ExpressionPtr left, const std::vector<ExpressionPtr>& args)
			: FormulaExpression("_fn"), left_(left), args_(args)
			{
				variant left_var;
				if(left_->canReduceToVariant(left_var)) {
					if(left_var.is_generic_function()) {
						std::map<std::string, variant_type_ptr> types;
						std::vector<variant_type_ptr> arg_types = left_var.function_arg_types();
						for(int n = 0; n != arg_types.size() && n != args_.size(); ++n) {
							std::string id;
							if(arg_types[n]->is_generic(&id) == false) {
								continue;
							}

							variant_type_ptr type = args_[n]->queryVariantType();
							variant_type_ptr current = types[id];
							if(current) {
								if(type->is_equal(*current) || variant_types_compatible(type, current)) {
									//type = type
								} else if(variant_types_compatible(current, type)) {
									type = current;
								} else {
									std::vector<variant_type_ptr> v;
									v.push_back(type);
									v.push_back(current);
									type = variant_type::get_union(v);
								}
							}

							types[id] = type;
						}

						std::vector<variant_type_ptr> args;
						std::vector<std::string> generic_args = left_var.generic_function_type_args();
						for(const std::string& id : generic_args) {
							variant_type_ptr type = types[id];
							ASSERT_LOG(type, "Cannot find type in generic function for type " << id);
							args.push_back(type);
						}

						variant fn = left_var.instantiate_generic_function(args);
						left_.reset(new VariantExpression(fn));
					}
				}

				variant_type_ptr fn_type = left_->queryVariantType();
				std::vector<variant_type_ptr> arg_types;
				if(fn_type->is_function(&arg_types, nullptr, nullptr)) {
					for(unsigned n = 0; n < arg_types.size() && n < args.size(); ++n) {
						const FormulaInterface* formula_interface = arg_types[n]->is_interface();

						ffl::IntrusivePtr<FormulaInterfaceInstanceFactory> interface_factory;
						if(formula_interface) {
							try {
								interface_factory.reset(formula_interface->createFactory(args[n]->queryVariantType()));
							} catch(FormulaInterface::interface_mismatch_error& e) {
								error_msg_ = "Could not create interface: " + e.msg;
							}
						}

						interfaces_.push_back(interface_factory);
					}
				}
			}
		private:
			variant execute(const FormulaCallable& variables) const override {
				const InfiniteRecursionProtector recurse_scope(left_);
				const variant left = left_->evaluate(variables);
				std::vector<variant> args;
				args.reserve(args_.size());
				unsigned nindex = 0;
				for(const ExpressionPtr& e : args_) {
					if(nindex < interfaces_.size() && interfaces_[nindex]) {
						args.push_back(interfaces_[nindex]->create(e->evaluate(variables)));
					} else {
						args.push_back(e->evaluate(variables));
					}
					++nindex;
				}

				if(!left.is_function()) {
					//TODO: Nasty hack to make null() still work -- deprecated in
					//favor of null.
					if(left_->str() == "null" && args_.empty()) {
						return variant();
					}
				}
		
				return left(&args);
			}

			variant_type_ptr getVariantType() const override {
				std::vector<variant_type_ptr> arg_types;
				for(const ExpressionPtr& expr : args_) {
					arg_types.push_back(expr->queryVariantType());
				}
				variant_type_ptr return_type = left_->queryVariantType()->function_return_type_with_args(arg_types);
				if(return_type) {
					return return_type;
				}

				return variant_type::get_any();
			}

			void staticErrorAnalysis() const override {
				if(error_msg_.empty() == false) {
					ASSERT_LOG(false, error_msg_ << " " << debugPinpointLocation());
				}

				variant_type_ptr fn_type = left_->queryVariantType();
				std::vector<variant_type_ptr> arg_types;
				int min_args = 0;
				const bool is_function = fn_type->is_function(&arg_types, nullptr, &min_args);

				ASSERT_LOG(!fn_type->is_type(variant::VARIANT_TYPE_FUNCTION), "Function call on object of type 'function'. Must have a type with a full type signature to call a function on it in strict mode." << debugPinpointLocation());
				ASSERT_LOG(is_function, "Function call on expression which isn't guaranteed to be a function: " << fn_type->to_string() << " " << debugPinpointLocation());

				if(is_function) {
					for(unsigned n = 0; n != args_.size() && n != arg_types.size(); ++n) {
						variant_type_ptr t = args_[n]->queryVariantType();
						if(!variant_types_compatible(arg_types[n], t) && (n >= interfaces_.size() || !interfaces_[n])) {
							std::string msg = " DOES NOT MATCH ";
							if(variant_types_compatible(arg_types[n], variant_type::get_null_excluded(t))) {
								msg = " MIGHT BE nullptr ";
							}

							ASSERT_LOG(false,
									   "FUNCTION CALL DOES NOT MATCH: " << debugPinpointLocation() << " ARGUMENT " << (n+1) << " TYPE " << t->to_string() << msg << arg_types[n]->to_string() << "\n");
						}
					}

					ASSERT_LOG(min_args < 0 || args_.size() >= static_cast<unsigned>(min_args), "Too few arguments to function. Provided " << args_.size() << ", expected at least " << min_args << ": " << debugPinpointLocation() << "\n");
					ASSERT_LOG(args_.size() <= arg_types.size(), "Too many arguments to function. Provided " << args_.size() << ", expected at most " << arg_types.size() << ": " << debugPinpointLocation() << "\n");
				}
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(left_);
				result.insert(result.end(), args_.begin(), args_.end());
				return result;
			}

			bool canCreateVM() const override { return canChildrenVM(); }

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(left_);
				bool can_vm = left_->canCreateVM();

				for(ExpressionPtr& e : args_) {
					optimizeChildToVM(e);
					if(!e->canCreateVM()) {
						can_vm = false;
					}
				}

				if(can_vm) {

					formula_vm::VirtualMachine vm;

					variant fn_var;
					if(g_ffl_vm_opt_inline && left_->canReduceToVariant(fn_var) && fn_var.is_regular_function() && fn_var.get_function_formula() && fn_var.get_function_formula()->hasGuards() == false && fn_var.get_function_formula()->expr()->canCreateVM()) {
						auto info = fn_var.get_function_info();

						const int base_slot = fn_var.get_function_base_slot();
						const int num_args = static_cast<int>(info->arg_names.size());

						formula_vm::VirtualMachine fn_vm;
						fn_var.get_function_formula()->expr()->emitVM(fn_vm);

						//see if the function never uses its closure and we can fully inline it.
						bool can_optimize = true;

						std::map<int, VirtualMachine::Iterator> lookups;
						std::vector<VirtualMachine::Iterator> ordered_lookups;

						std::vector<bool> vm_trivial;
						for(int n = 0; n < num_args; ++n) {
							if(n < args_.size()) {
								formula_vm::VirtualMachine vm;
								args_[n]->emitVM(vm);
								auto itor(vm.begin_itor());
								if(!itor.at_end()) {
									itor.next();
								}

								vm_trivial.push_back(itor.at_end());
							} else {
								vm_trivial.push_back(true);
							}
						}


						std::vector<bool> unrelated_scope_stack;
						int loop_end = -1;

						for(VirtualMachine::Iterator itor(fn_vm.begin_itor()); !itor.at_end(); itor.next()) {
							if(formula_vm::VirtualMachine::isInstructionLoop(itor.get())) {
								const int end = static_cast<int>(itor.get_index()) + itor.arg();
								if(end > loop_end) {
									loop_end = end;
								}
							} else if(itor.get() == formula_vm::OP_PUSH_SCOPE) {
								unrelated_scope_stack.push_back(true);
							} else if(itor.get() == formula_vm::OP_INLINE_FUNCTION) {
								unrelated_scope_stack.push_back(false);
							} else if(itor.get() == formula_vm::OP_WHERE && itor.arg() >= 0) {
								unrelated_scope_stack.push_back(false);
							} else if(itor.get() == formula_vm::OP_POP_SCOPE) {
								assert(unrelated_scope_stack.empty() == false);
								unrelated_scope_stack.pop_back();
							} else if((itor.get() == formula_vm::OP_LOOKUP_STR && std::find(unrelated_scope_stack.begin(), unrelated_scope_stack.end(), true) == unrelated_scope_stack.end()) || itor.get() == formula_vm::OP_CALL_BUILTIN_DYNAMIC || itor.get() == formula_vm::OP_LAMBDA_WITH_CLOSURE) {
								can_optimize = false;
								break;
							} else if(itor.get() == formula_vm::OP_LOOKUP && std::find(unrelated_scope_stack.begin(), unrelated_scope_stack.end(), true) == unrelated_scope_stack.end() && itor.arg() < base_slot) {
								can_optimize = false;
								break;
							} else if(itor.get() == formula_vm::OP_LOOKUP && std::find(unrelated_scope_stack.begin(), unrelated_scope_stack.end(), true) == unrelated_scope_stack.end() && itor.arg() >= base_slot + num_args) {
								//TODO: remap lookups of symbols created within the function. For now just don't allow inlining.
								can_optimize = false;
								break;
							} else if(itor.get() == formula_vm::OP_LOOKUP && std::find(unrelated_scope_stack.begin(), unrelated_scope_stack.end(), true) == unrelated_scope_stack.end() && itor.arg() >= base_slot && itor.arg() < base_slot + num_args) {

								const int index = itor.arg() - base_slot;
								assert(index >= 0 && index < vm_trivial.size());

								if((static_cast<int>(itor.get_index()) < loop_end || lookups.count(itor.arg()) > 0) && vm_trivial[index] == false) {
									can_optimize = false;
									break;
								}

								lookups.insert(std::pair<int, VirtualMachine::Iterator>(itor.arg(), itor));
	
								ordered_lookups.emplace_back(itor);
							}
						}

						if(can_optimize) {

							std::reverse(ordered_lookups.begin(), ordered_lookups.end());

							for(auto lookup : ordered_lookups) {
								auto next_itor = lookup;
								next_itor.next();
	
								const int index = lookup.arg() - base_slot;
								assert(index >= 0 && index < num_args);

								VirtualMachine arg_vm;

								if(index < args_.size()) {
									args_[index]->emitVM(arg_vm);
								} else {
									//a default argument
									const int start_default = num_args - static_cast<int>(info->default_args.size());
									const int default_index = index - start_default;
									assert(default_index >= 0 && default_index < info->default_args.size());

									arg_vm.addLoadConstantInstruction(info->default_args[default_index]);
								}

								fn_vm.append(lookup, next_itor, arg_vm);
							}

							vm.append(fn_vm);
						} else {

							for(ExpressionPtr& e : args_) {
								e->emitVM(vm);
							}

							if(args_.size() < info->arg_names.size()) {
								ASSERT_LOG(args_.size() + info->default_args.size() >= info->arg_names.size(), "Wrong number of function args");

								auto i = info->default_args.end() - (info->arg_names.size() - args_.size());
								while(i != info->default_args.end()) {
									vm.addLoadConstantInstruction(*i);
									++i;
								}
	
							}

							vm.addLoadConstantInstruction(variant(fn_var.get_function_closure()));

							vm.addInstruction(OP_PUSH_INT);
							vm.addInt(info->arg_names.size());

							vm.addInstruction(formula_vm::OP_INLINE_FUNCTION);
							vm.addInt(base_slot);

							vm.append(fn_vm);

							vm.addInstruction(formula_vm::OP_POP_SCOPE);
						}
					} else {

						left_->emitVM(vm);
						size_t index = 0;
						for(ExpressionPtr& e : args_) {
							e->emitVM(vm);
							if(index < interfaces_.size() && interfaces_[index]) {
								vm.addLoadConstantInstruction(variant(interfaces_[index].get()));
								vm.addInstruction(OP_CREATE_INTERFACE);
							}
							++index;
						}

						vm.addInstruction(OP_CALL);
						vm.addInt(static_cast<VirtualMachine::InstructionType>(args_.size()));
					}

					return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
				}

				return ExpressionPtr();
			}
	
			ExpressionPtr left_;
			std::vector<ExpressionPtr> args_;
			std::vector<ffl::IntrusivePtr<FormulaInterfaceInstanceFactory> > interfaces_;
			std::string error_msg_;
		};

		class DotExpression : public FormulaExpression {
		public:
			DotExpression(ExpressionPtr left, ExpressionPtr right, ConstFormulaCallableDefinitionPtr right_def)
			: FormulaExpression("_dot"), left_(left), right_(right), right_def_(right_def)
			{}
			ConstFormulaCallableDefinitionPtr getTypeDefinition() const override {
				return right_->getTypeDefinition();
			}
		private:
			variant execute(const FormulaCallable& variables) const override {
				const variant left = left_->evaluate(variables);
				if(!left.is_callable()) {
					if(left.is_map()) {
						return left[variant(right_->str())];
					} else if(left.is_list()) {
						const std::string& s = right_->str();
						if(s == "x" || s == "r") {
							return left[0];
						} else if(s == "y" || s == "g") {
							return left[1];
						} else if(s == "z" || s == "b") {
							return left[2];
						} else if(s == "a") {
							return left[3];
						} else {
							return variant();
						}
					}

					ASSERT_LOG(!left.is_null(), "CALL OF DOT OPERATOR ON nullptr VALUE: '" << left_->str() << "': " << debugPinpointLocation());
					ASSERT_LOG(false, "CALL OF DOT OPERATOR ON ILLEGAL VALUE: " << left.write_json() << " PRODUCED BY '" << left_->str() << "': " << debugPinpointLocation());
			
					return left;
				}
		
				return right_->evaluate(*left.as_callable());
			}
	
			variant executeMember(const FormulaCallable& variables, std::string& id, variant* variant_id) const override {
				variant left = left_->evaluate(variables);
		
				if(!right_->isIdentifier(&id)) {
					return right_->evaluateWithMember(*left.as_callable(), id);
				}
		
				return left;
			}

			variant_type_ptr getVariantType() const override {
				variant_type_ptr type = left_->queryVariantType();
				if(type && variant_type::get_type(variant::VARIANT_TYPE_LIST)->is_compatible(type)) {
					variant_type_ptr list_of = type->is_list_of();
					if(list_of) {
						return list_of;
					} else {
						return variant_type::get_any();
					}
				}

				return right_->queryVariantType();
			}

			variant_type_ptr getMutableType() const override {
				variant_type_ptr type = left_->queryMutableType();
				if(type && variant_type::get_type(variant::VARIANT_TYPE_LIST)->is_compatible(type)) {
					variant_type_ptr list_of = type->is_list_of();
					if(list_of) {
						return list_of;
					} else {
						return variant_type::get_any();
					}
				}

				return right_->queryMutableType();
			}

			static bool is_type_valid_left_side(variant_type_ptr type) {
				const std::vector<variant_type_ptr>* u = type->is_union();
				if(u) {
					for(variant_type_ptr t : *u) {
						if(!is_type_valid_left_side(t)) {
							return false;
						}
					}

					return u->empty() == false;
				}

				return variant_types_compatible(variant_type::get_type(variant::VARIANT_TYPE_CALLABLE), type) || variant_types_compatible(variant_type::get_type(variant::VARIANT_TYPE_MAP), type);
			}

			void staticErrorAnalysis() const override {
				variant_type_ptr type = left_->queryVariantType();
				ASSERT_LOG(type, "Could not find type for left side of '.' operator: " << left_->str() << ": " << debugPinpointLocation());

				if(variant_type::get_type(variant::VARIANT_TYPE_LIST)->is_compatible(type)) {
					const std::string& s = right_->str();
					static const std::string ListMembers[] = { "x", "y", "z", "r", "g", "b", "a" };
					for(const std::string& item : ListMembers) {
						if(s == item) {
							return;
						}
					}

					ASSERT_LOG(false, "No such member " << s << " in list: " << debugPinpointLocation());
				}

				ASSERT_LOG(variant_type::may_be_null(type) == false, "Left side of '.' operator may be null: " << left_->str() << " is " << type->to_string() << " " << debugPinpointLocation());
				ASSERT_LOG(is_type_valid_left_side(type), "Left side of '.' is of invalid type: " << left_->str() << " is " << type->to_string() << " " << debugPinpointLocation());
			}

			ConstFormulaCallableDefinitionPtr getModifiedDefinitionBasedOnResult(bool result, ConstFormulaCallableDefinitionPtr current_def, variant_type_ptr expression_is_this_type) const override {

				std::vector<const DotExpression*> expr;
				if(isIdentifierChain(&expr) == false) {
					return ConstFormulaCallableDefinitionPtr();
				}

				//This expression is the top of an identifier chain -- i.e. expression of the form a.b.c.d
				//where a, b, c, and d are all plain identifiers. They are stored with right-associativity
				//meaning this expression is the last expression in the chain.

				ConstFormulaCallableDefinitionPtr def;
				while(expr.empty() == false) {
					if(!expr.back()->right_def_) {
						return ConstFormulaCallableDefinitionPtr();
					}

					ConstFormulaCallableDefinitionPtr new_right_def = def;
					if(!new_right_def) {
						new_right_def = expr.back()->right_->queryModifiedDefinitionBasedOnResult(result, expr.back()->right_def_, expression_is_this_type);
					}

					auto last_expr = expr.back();

					expr.pop_back();

					std::string key_name;

					ConstFormulaCallableDefinitionPtr context_def = current_def;
					if(expr.empty() == false) {
						context_def = expr.back()->right_def_;
						if(!context_def) {
							return ConstFormulaCallableDefinitionPtr();
						}

						if(!expr.back()->right_->isIdentifier(&key_name)) {
							return ConstFormulaCallableDefinitionPtr();
						}
					} else {
						if(!last_expr->left_->isIdentifier(&key_name)) {
							return ConstFormulaCallableDefinitionPtr();
						}
					}

					const int slot = context_def->getSlot(key_name);

					def = modify_formula_callable_definition(context_def, slot, variant_type_ptr(), new_right_def.get());
				}

				return def;
			}

			//function which tells you if this is the top of an identifier chain -- i.e. an expression in
			//the form a.b.c.d which is held using right-associativity. Gives you the list
			//of individual expressions.
			bool isIdentifierChain(std::vector<const DotExpression*>* expressions) const {

				std::string id;
				if(!right_->isIdentifier(&id)) {
					return false;
				}

				if(left_->isIdentifier(&id)) {
					expressions->push_back(this);
					return true;
				}

				const DotExpression* left_dot = dynamic_cast<const DotExpression*>(left_.get());
				if(!left_dot) {
					return false;
				}

				if(left_dot->isIdentifierChain(expressions)) {
					expressions->push_back(this);
					return true;
				}

				return false;
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(left_);
				result.push_back(right_);
				return result;
			}

			ExpressionPtr optimize() const override {

				auto left_type = left_->queryVariantType();

				//Optimization so that an expression such as lib.gui would boil down directly into
				//the actual class instance.
				if(g_ffl_vm_opt_library_lookups && left_type->is_builtin() && *left_type->is_builtin() == "library") {
					const std::string& s = right_->str();

					if(can_load_library_instance(s)) {
						FormulaCallablePtr res = get_library_instance(s);
						ASSERT_LOG(res.get() != nullptr, "Could not get library: " << s);
						return ExpressionPtr(new VariantExpression(variant(res.get())));
					}
				}

				variant left_var;
				if(g_ffl_vm_opt_constant_lookups && left_->canReduceToVariant(left_var) && left_var.is_callable()) {
					auto p = left_var.as_callable();
					variant value;
					if(p->queryConstantValue(right_->str(), &value)) {
						return ExpressionPtr(new VariantExpression(value));
					}
				}

				return ExpressionPtr();
			}

			bool canCreateVM() const override { return canChildrenVM(); }

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(left_);
				optimizeChildToVM(right_);

				auto left_type = left_->queryVariantType();

				if(left_->canCreateVM() && right_->canCreateVM()) {
					formula_vm::VirtualMachine vm;

					//Optimization so that an expression such as lib.gui would boil down directly into
					//the actual class instance.
					if(g_ffl_vm_opt_library_lookups && left_type->is_builtin() && *left_type->is_builtin() == "library") {
						const std::string& s = right_->str();

						if(can_load_library_instance(s)) {
							FormulaCallablePtr res = get_library_instance(s);
							ASSERT_LOG(res.get() != nullptr, "Could not get library: " << s);
							vm.addLoadConstantInstruction(variant(res.get()));
							return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
						}
					}

					if(variant_type::get_type(variant::VARIANT_TYPE_LIST)->is_compatible(left_type)) {
						left_->emitVM(vm);

						const std::string& s = right_->str();
						if(s == "x" || s == "r") {
							vm.addInstruction(OP_INDEX_0);
						} else if(s == "y" || s == "g") {
							vm.addInstruction(OP_INDEX_1);
						} else if(s == "z" || s == "b") {
							vm.addInstruction(OP_INDEX_2);
						} else if(s == "a") {
							vm.addInstruction(OP_PUSH_INT);
							vm.addInt(3);
							vm.addInstruction(OP_INDEX);
						}
						return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
					} else if(variant_type::get_type(variant::VARIANT_TYPE_CALLABLE)->is_compatible(left_type)) {
						left_->emitVM(vm);
						vm.addInstruction(OP_PUSH_SCOPE);
						right_->emitVM(vm);
						vm.addInstruction(OP_POP_SCOPE);
					} else if(variant_type::get_type(variant::VARIANT_TYPE_MAP)->is_compatible(left_type) && left_->str() != "arg" /*HORRIBLE HACK to exclude arg, TODO: fix arg to not mismatch object and map types*/) {
						left_->emitVM(vm);
						vm.addLoadConstantInstruction(variant(right_->str()));
						vm.addInstruction(formula_vm::OP_INDEX);
					} else {
						left_->emitVM(vm);
						vm.addLoadConstantInstruction(variant(right_->str()));
						vm.addInstruction(formula_vm::OP_INDEX_STR);
					}

					return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
				}
				return ExpressionPtr();
			}
	
			ExpressionPtr left_, right_;

			//the definition used to evaluate right_. i.e. the type of the value
			//returned from left_.
			ConstFormulaCallableDefinitionPtr right_def_;
		};

		class SquareBracketExpression : public FormulaExpression { //TODO
		public:
			SquareBracketExpression(ExpressionPtr left, ExpressionPtr key)
			: FormulaExpression("_sqbr"), left_(left), key_(key)
			{
			}
		private:
			variant execute(const FormulaCallable& variables) const override {
				const variant left = left_->evaluate(variables);
				const variant key = key_->evaluate(variables);
				if(left.is_list() || left.is_map()) {
					return left[ key ];
				} else if(left.is_string()) {
					unsigned index = key.as_int();
					if(left.is_str_utf8()) {
						ASSERT_LOG(index < left.num_elements(), "index outside bounds: " << left.as_string() << "[" << index << "]'\n'" << debugPinpointLocation());

						return variant(utils::str_substr_utf8(left.as_string(), index, index+1));

					} else {
						const std::string& s = left.as_string();
						ASSERT_LOG(index < s.length(), "index outside bounds: " << s << "[" << index << "]'\n'"  << debugPinpointLocation());
						return variant(s.substr(index, 1));
					}
				} else if(left.is_callable()) {
					return left.as_callable()->queryValue(key.as_string());
				} else {
					LOG_INFO("STACK TRACE FOR ERROR:" << get_call_stack());
					LOG_INFO(output_formula_error_info());
					ASSERT_LOG(false, "illegal usage of operator []: called on " << left.to_debug_string() << " value: " << left_->str() << "'\n" << debugPinpointLocation());
				}
			}
	
			variant executeMember(const FormulaCallable& variables, std::string& id, variant* variant_id) const override {
				const variant left = left_->evaluate(variables);
				const variant key = key_->evaluate(variables);

				if(key.is_string()) {
					id = key.as_string();
				} else if(variant_id) {
					*variant_id = key;
				}
				return left;
			}

			variant_type_ptr getVariantType() const override {
				variant_type_ptr left_type = left_->queryVariantType();
				if(left_type->is_type(variant::VARIANT_TYPE_STRING)) {
					return variant_type::get_type(variant::VARIANT_TYPE_STRING);
				}

				variant_type_ptr list_element_type = left_type->is_list_of();
				if(list_element_type) {
					return list_element_type;
				}

				std::pair<variant_type_ptr, variant_type_ptr> p = left_type->is_map_of();
				if(p.second) {
					return p.second;
				}

				return variant_type::get_any();
			}

			variant_type_ptr getMutableType() const override {
				return queryVariantType();
			}

			void staticErrorAnalysis() const override {
				variant_type_ptr type = left_->queryVariantType();

				ASSERT_LOG(variant_type::get_null_excluded(type) == type, "Left side of '[]' operator may be null: " << left_->str() << " is " << type->to_string() << " " << debugPinpointLocation());
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(left_);
				result.push_back(key_);
				return result;
			}

			bool canCreateVM() const override { return canChildrenVM(); }

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(left_);
				optimizeChildToVM(key_);

				auto left_type = left_->queryVariantType();

				if(left_->canCreateVM() && key_->canCreateVM()) {
					formula_vm::VirtualMachine vm;
					left_->emitVM(vm);

					variant key_const;
					if(left_type->is_list_of() && key_->canReduceToVariant(key_const) && key_const.is_int() && key_const.as_int() >= 0 && key_const.as_int() <= 2) {
						switch(key_const.as_int()) {
							case 0: vm.addInstruction(formula_vm::OP_INDEX_0); break;
							case 1: vm.addInstruction(formula_vm::OP_INDEX_1); break;
							case 2: vm.addInstruction(formula_vm::OP_INDEX_2); break;
							default: assert(false);
						}
						
					} else {
						key_->emitVM(vm);
						if(left_type->is_list_of() || left_type->is_map_of().first) {
							vm.addInstruction(formula_vm::OP_INDEX);
						} else {
							vm.addInstruction(formula_vm::OP_INDEX_STR);
						}
					}

					return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
				}
				return ExpressionPtr();
			}
	
			ExpressionPtr left_, key_;
		};

		class SliceSquareBracketExpression : public FormulaExpression {
		public:
			SliceSquareBracketExpression(ExpressionPtr left, ExpressionPtr start, ExpressionPtr end)
			: FormulaExpression("_slice_sqbr"), left_(left), start_(start), end_(end)
			{}
		private:
			variant execute(const FormulaCallable& variables) const override {
				const variant left = left_->evaluate(variables);
				int begin_index = start_ ? start_->evaluate(variables).as_int() : 0;
				int end_index = end_ ? end_->evaluate(variables).as_int() : left.num_elements();

				if(left.is_string()) {
					const std::string& s = left.as_string();
					int s_len = static_cast<int>(left.num_elements());
					if(begin_index > s_len) {
						begin_index = s_len;
					}
					if(end_index > static_cast<int>(s.length())) {
						end_index = s_len;
					}
					if(s.length() == 0) {
						return left;
					}

					ASSERT_LOG(begin_index >= 0, "Illegal negative index when slicing a string: " << begin_index << " at " << debugPinpointLocation());
					ASSERT_LOG(end_index >= 0, "Illegal negative index when slicing a string: " << end_index << " at " << debugPinpointLocation());

					if(end_index >= begin_index) {
						if(s_len != s.size()) {
							//utf8 string.
							return variant(utils::str_substr_utf8(s, begin_index, end_index));
						} else {
							return variant(s.substr(begin_index, end_index-begin_index));
						}
					} else {
						return variant("");
					}
				}

				if(begin_index > left.num_elements()) {
					begin_index = left.num_elements();
				}

				if(end_index > left.num_elements()) {
					end_index = left.num_elements();
				}
		
				if(left.is_list()) {
					if(left.num_elements() == 0) {
						std::vector<variant> empty;
						return variant(&empty);
					}
					if(end_index >= begin_index) {
						return left.get_list_slice(begin_index, end_index);
					} else {
						std::vector<variant> empty;
						return variant(&empty);
					}
			
				} else {
					ASSERT_LOG(false, "illegal usage of operator [:]'\n" << debugPinpointLocation() << " called on object of type " << variant::variant_type_to_string(left.type()));
				}
			}

			variant_type_ptr getVariantType() const override {
				return left_->queryVariantType();
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(left_);
				result.push_back(start_);
				result.push_back(end_);
				return result;
			}

			bool canCreateVM() const override { return canChildrenVM(); }

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(left_);
				optimizeChildToVM(start_);
				optimizeChildToVM(end_);
				if(left_->canCreateVM() && (!start_ || start_->canCreateVM()) && (!end_ || end_->canCreateVM()) && (start_ || end_)) {
					formula_vm::VirtualMachine vm;
					left_->emitVM(vm);
					if(start_) {
						start_->emitVM(vm);
					} else {
						vm.addLoadConstantInstruction(variant(0));
					}

					if(end_) {
						end_->emitVM(vm);
					} else {
						vm.addLoadConstantInstruction(variant());
					}
					vm.addInstruction(OP_ARRAY_SLICE);
					return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
				}
				return ExpressionPtr();
			}
	
			ExpressionPtr left_, start_, end_;
		};

		variant_type_ptr get_variant_type_and_or(ExpressionPtr left, ExpressionPtr right, bool is_or=false) {
			variant_type_ptr left_type = left->queryVariantType();
			variant_type_ptr right_type = right->queryVariantType();
			if(left_type->is_equal(*right_type)) {
				return left_type;
			}

			std::vector<variant_type_ptr> types;
			if(is_or) {
				//Make it so e.g. (int|null or int) evaluates to int rather than int|null
				left_type = variant_type::get_null_excluded(left_type);
			}
			types.push_back(left_type);
			types.push_back(right_type);
			return variant_type::get_union(types);
		}

		class AndOperatorExpression : public FormulaExpression {
		public:
			AndOperatorExpression(ExpressionPtr left, ExpressionPtr right)
			  : FormulaExpression("_and"), left_(left), right_(right)
			{
			}

		private:
			variant execute(const FormulaCallable& variables) const override {
				variant v = left_->evaluate(variables);
				if(!v.as_bool()) {
					return v;
				}

				return right_->evaluate(variables);
			}

			variant_type_ptr getVariantType() const override {
				return get_variant_type_and_or(left_, right_);
			}

			ConstFormulaCallableDefinitionPtr
			getModifiedDefinitionBasedOnResult(bool result, ConstFormulaCallableDefinitionPtr current_def, variant_type_ptr expression_is_this_type) const override {
				if(expression_is_this_type) {
					return ConstFormulaCallableDefinitionPtr();
				}

				if(result) {
					ConstFormulaCallableDefinitionPtr original_def = current_def;
					ConstFormulaCallableDefinitionPtr def = left_->queryModifiedDefinitionBasedOnResult(result, current_def);
					if(def) {
						current_def = def;
					}

					def = right_->queryModifiedDefinitionBasedOnResult(result, current_def);
					if(def) {
						current_def = def;
					}

					if(current_def != original_def) {
						return current_def;
					}
				}

				return ConstFormulaCallableDefinitionPtr();
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(left_);
				result.push_back(right_);
				return result;
			}

			bool canCreateVM() const override { return canChildrenVM(); }

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(left_);
				optimizeChildToVM(right_);

				if(left_->canCreateVM() && right_->canCreateVM()) {
					formula_vm::VirtualMachine vm;
					left_->emitVM(vm);
					const int jump_source = vm.addJumpSource(OP_JMP_UNLESS);
					vm.addInstruction(OP_POP);
					right_->emitVM(vm);
					vm.jumpToEnd(jump_source);
					return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
				}

				return ExpressionPtr();
			}

			ExpressionPtr left_, right_;
		};

		class OrOperatorExpression : public FormulaExpression {
		public:
			OrOperatorExpression(ExpressionPtr left, ExpressionPtr right)
			  : FormulaExpression("_or"), left_(left), right_(right)
			{
			}

		private:
			variant execute(const FormulaCallable& variables) const override {
				variant v = left_->evaluate(variables);
				if(v.as_bool()) {
					return v;
				}

				return right_->evaluate(variables);
			}

			variant_type_ptr getVariantType() const override {
				return get_variant_type_and_or(left_, right_, true);
			}

			ConstFormulaCallableDefinitionPtr
			getModifiedDefinitionBasedOnResult(bool result, ConstFormulaCallableDefinitionPtr current_def, variant_type_ptr expression_is_this_type) const override {
				if(expression_is_this_type) {
					return ConstFormulaCallableDefinitionPtr();
				}

				if(result == false) {
					ConstFormulaCallableDefinitionPtr def = right_->queryModifiedDefinitionBasedOnResult(result, current_def);
					if(def) {
						return def;
					} else {
						return left_->queryModifiedDefinitionBasedOnResult(result, current_def);
					}
				}

				return ConstFormulaCallableDefinitionPtr();
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(left_);
				result.push_back(right_);
				return result;
			}

			bool canCreateVM() const override { return canChildrenVM(); }

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(left_);
				optimizeChildToVM(right_);

				if(left_->canCreateVM() && right_->canCreateVM()) {
					formula_vm::VirtualMachine vm;
					left_->emitVM(vm);
					const int jump_source = vm.addJumpSource(OP_JMP_IF);
					vm.addInstruction(OP_POP);
					right_->emitVM(vm);
					vm.jumpToEnd(jump_source);
					return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
				}

				return ExpressionPtr();
			}

			ExpressionPtr left_, right_;
		};

		class OperatorExpression : public FormulaExpression {
		public:
			OperatorExpression(const std::string& op, ExpressionPtr left,
								ExpressionPtr right)
			: FormulaExpression("_op"), op_(OP(op[0])), left_(left), right_(right)
			{
				if(op == ">=") {
					op_ = OP_GTE;
				} else if(op == "<=") {
					op_ = OP_LTE;
				} else if(op == "!=") {
					op_ = OP_NEQ;
				} else if(op == "and") {
					op_ = OP_AND;
				} else if(op == "or") {
					op_ = OP_OR;
				} else if(op == "in") {
					op_ = OP_IN;
				} else if(op == "not in") {
					op_ = OP_NOT_IN;
				}
			}

			ExpressionPtr optimize() const override {
				if(op_ == OP_AND) {
					return ExpressionPtr(new AndOperatorExpression(left_, right_));
				} else if(op_ == OP_OR) {
					return ExpressionPtr(new OrOperatorExpression(left_, right_));
				}

				return ExpressionPtr();
			}

			ExpressionPtr get_left() const { return left_; }
			ExpressionPtr get_right() const { return right_; }


			void emitVM(formula_vm::VirtualMachine& vm) const override {
				left_->emitVM(vm);
				right_->emitVM(vm);
				vm.addInstruction(op_);
			}
	
		private:
			variant execute(const FormulaCallable& variables) const override {
				const variant left = left_->evaluate(variables);
				variant right = right_->evaluate(variables);
				switch(op_) {
					case OP_IN:
					case OP_NOT_IN: {
						bool result = op_ == OP_IN;
						if(right.is_list()) {
							for(int n = 0; n != right.num_elements(); ++n) {
								if(left == right[n]) {
									return variant::from_bool(result);
								}
							}
					
							return variant::from_bool(!result);
						} else if(right.is_map()) {
							return variant::from_bool(right.has_key(left) ? result : !result);
						} else {
							ASSERT_LOG(false, "ILLEGAL OPERAND TO 'in': " << right.write_json() << " AT " << debugPinpointLocation());
							return variant();
						}
					}
					case OP_AND: 
						return left.as_bool() == false ? left : right;
					case OP_OR: 
						return left.as_bool() ? left : right;
					case OP_ADD: 
						return left + right;
					case OP_SUB: 
						return left - right;
					case OP_MUL: 
						return left * right;
					case OP_DIV: 

						//this is a very unorthodox hack to guard against divide-by-zero errors.  It returns positive or negative infinity instead of asserting, which (hopefully!) works out for most of the physical calculations that are using this.  We tentatively view this behavior as much more preferable to the game apparently crashing for a user.  This is of course not rigorous outside of a videogame setting.
						if(right == variant(0)) { 
							right = variant(decimal::epsilon());
						}

						return left / right;
					case OP_POW: 
						return left ^ right;
					case OP_EQ:  
						return left == right ? variant::from_bool(true) : variant::from_bool(false);
					case OP_NEQ: 
						return left != right ? variant::from_bool(true) : variant::from_bool(false);
					case OP_LTE: 
						return left <= right ? variant::from_bool(true) : variant::from_bool(false);
					case OP_GTE: 
						return left >= right ? variant::from_bool(true) : variant::from_bool(false);
					case OP_LT:  
						return left < right ? variant::from_bool(true) : variant::from_bool(false);
					case OP_GT:  
						return left > right ? variant::from_bool(true) : variant::from_bool(false);
					case OP_MOD: 
						return left % right;
					case OP_DICE:
					default:
						return variant(dice_roll(left.as_int(), right.as_int()));
				}
			}
	
			static int dice_roll(int num_rolls, int faces) {
				int res = 0;
				while(faces > 0 && num_rolls-- > 0) {
					res += (rng::generate()%faces)+1;
				}
				return res;
			}

			void staticErrorAnalysis() const override {
				variant_type_ptr left_type = left_->queryVariantType();
				variant_type_ptr right_type = right_->queryVariantType();

				if(left_type->is_numeric() && right_type->is_numeric()) {
					return;
				}

				switch(op_) {
				case OP_EQ:
				case OP_NEQ:
					ASSERT_LOG(variant_types_might_match(left_type, right_type) || left_type->is_type(variant::VARIANT_TYPE_NULL) || right_type->is_type(variant::VARIANT_TYPE_NULL), "Equality expression on incompatible types: " << left_type->to_string() << " compared to " << right_type->to_string() << " " << debugPinpointLocation());
				case OP_IN:
				case OP_NOT_IN:
				case OP_LTE:
				case OP_GTE:
				case OP_GT:
				case OP_LT:
				case OP_AND:
				case OP_OR:
					return;

				case OP_ADD: {
					if(left_type->is_numeric() && right_type->is_numeric()) {
						return;
					}

					if(left_type->is_type(variant::VARIANT_TYPE_STRING) && variant_type::may_be_null(right_type) == false) {
						return;
					}

					if(left_type->is_list_of() && right_type->is_list_of()) {
						return;
					}

					if((left_type->is_map_of().first || left_type->is_class()) && right_type->is_map_of().first) {
						return;
					}

					ASSERT_LOG(false, "Illegal types to + operator: " << left_type->to_string() << " + " << right_type->to_string() << " At " << debugPinpointLocation());

					return;
				}

				case OP_MUL: {
					if(left_type->is_numeric() && right_type->is_numeric()) {
						return;
					}

					if(right_type->is_type(variant::VARIANT_TYPE_INT)) {
						if(left_type->is_type(variant::VARIANT_TYPE_STRING) || left_type->is_list_of()) {
							return;
						}
					}

					ASSERT_LOG(false, "Illegal types to * operator: " << left_type->to_string() << " + " << right_type->to_string() << " At " << debugPinpointLocation());

					return;
				}

				case OP_POW:
				case OP_DIV:
				case OP_SUB: {
					ASSERT_LOG(left_type->is_numeric() && right_type->is_numeric(),
							   "Illegal types to " << static_cast<char>(op_) << " operator: " << left_type->to_string() << " " << static_cast<char>(op_) << " " << right_type->to_string() << " " << debugPinpointLocation());
					return;

				}

				case OP_MOD:
				case OP_DICE:
					return;
				default:
					ASSERT_LOG(false, "unknown op type: " << op_);
				}
			}

			variant_type_ptr getVariantType() const override {

				switch(op_) {
				case OP_IN:
				case OP_NOT_IN:
				case OP_NEQ:
				case OP_LTE:
				case OP_GTE:
				case OP_GT:
				case OP_LT:
				case OP_EQ:
					return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
				case OP_AND:
				case OP_OR: {
					variant_type_ptr left_type = left_->queryVariantType()->base_type_no_enum();
					variant_type_ptr right_type = left_->queryVariantType()->base_type_no_enum();
					if(left_type->is_equal(*right_type)) {
						return left_type;
					}

					std::vector<variant_type_ptr> v;
					v.push_back(variant_type::get_null_excluded(left_type)); //if the left type is null it can't possibly be returned. e.g. make it so null|int or int will evaluate to int
					v.push_back(right_type);
					return variant_type::get_union(v);
				}

				case OP_ADD: {
					variant_type_ptr left_type = left_->queryVariantType()->base_type_no_enum();
					variant_type_ptr right_type = right_->queryVariantType()->base_type_no_enum();
					if(left_type->is_equal(*right_type)) {
						return left_type;
					}

					if(left_type->is_type(variant::VARIANT_TYPE_STRING)) {
						return left_type;
					}

					if(left_type->is_type(variant::VARIANT_TYPE_DECIMAL) || right_type->is_type(variant::VARIANT_TYPE_DECIMAL)) {
						return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
					}

					std::string class_name;
					if(left_type->is_class(&class_name) && right_type->is_map_of().first) {
						return left_type;
					}

					if(left_type->is_specific_list() && right_type->is_specific_list()) {
						std::vector<variant_type_ptr> items = *left_type->is_specific_list();
						auto other = right_type->is_specific_list();
						items.insert(items.end(), other->begin(), other->end());
						return variant_type::get_specific_list(items);
					}

					variant_type_ptr left_list = left_type->is_list_of();
					variant_type_ptr right_list = right_type->is_list_of();
					if(left_list && right_list) {
						std::vector<variant_type_ptr> v;
						v.push_back(left_list);
						v.push_back(right_list);
						return variant_type::get_list(variant_type::get_union(v));
					}

					const std::map<variant, variant_type_ptr>* left_specific = left_type->is_specific_map();
					const std::map<variant, variant_type_ptr>* right_specific = right_type->is_specific_map();
					if(left_specific && right_specific) {
						std::map<variant, variant_type_ptr> m = *left_specific;
						for(auto p : *right_specific) {
							if(m.count(p.first)) {
								std::vector<variant_type_ptr> v;
								v.push_back(m[p.first]);
								v.push_back(p.second);
								m[p.first] = variant_type::get_union(v);
							} else {
								m[p.first] = p.second;
							}
						}

						return variant_type::get_specific_map(m);
					}

					std::pair<variant_type_ptr,variant_type_ptr> left_map = left_type->is_map_of();
					std::pair<variant_type_ptr,variant_type_ptr> right_map = right_type->is_map_of();
					if(left_map.first && right_map.first) {
						std::vector<variant_type_ptr> k, v;
						k.push_back(left_map.first);
						k.push_back(right_map.first);
						v.push_back(left_map.second);
						v.push_back(right_map.second);
						return variant_type::get_map(variant_type::get_union(k), variant_type::get_union(v));
					}

					//TODO: improve this, handle remaining cases!
					return variant_type::get_any();
				}

				case OP_MUL: {
					variant_type_ptr left_type = left_->queryVariantType()->base_type_no_enum();
					variant_type_ptr right_type = right_->queryVariantType()->base_type_no_enum();
					if(left_type->is_type(variant::VARIANT_TYPE_INT) && right_type->is_type(variant::VARIANT_TYPE_INT)) {
						return variant_type::get_type(variant::VARIANT_TYPE_INT);
					}

					if((left_type->is_type(variant::VARIANT_TYPE_INT) ||
						left_type->is_type(variant::VARIANT_TYPE_DECIMAL)) &&
					   (right_type->is_type(variant::VARIANT_TYPE_INT) ||
						right_type->is_type(variant::VARIANT_TYPE_DECIMAL))) {
						return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
					}

					if(left_type->is_list_of()) {
						return variant_type::get_list(left_type->is_list_of());
					}

					return variant_type::get_any();
				}

				case OP_POW:
				case OP_DIV:
				case OP_SUB: {
					variant_type_ptr left_type = left_->queryVariantType()->base_type_no_enum();
					variant_type_ptr right_type = right_->queryVariantType()->base_type_no_enum();
					if(left_type->is_type(variant::VARIANT_TYPE_INT) && right_type->is_type(variant::VARIANT_TYPE_INT)) {
						return variant_type::get_type(variant::VARIANT_TYPE_INT);
					}

					return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
				}

				case OP_MOD:
				case OP_DICE:
					return variant_type::get_type(variant::VARIANT_TYPE_INT);
				default:
					ASSERT_LOG(false, "unknown op type: " << op_);
			
				}

			}

			ConstFormulaCallableDefinitionPtr getModifiedDefinitionBasedOnResult(bool result, ConstFormulaCallableDefinitionPtr current_def, variant_type_ptr expression_is_this_type) const override {
				if(expression_is_this_type) {
					return ConstFormulaCallableDefinitionPtr();
				}

				if(op_ == OP_EQ || op_ == OP_NEQ) {
					variant value;
					if(right_->isLiteral(value) && value.is_null()) {
						return left_->queryModifiedDefinitionBasedOnResult(op_ == OP_NEQ ? result : !result, current_def);
					} else if(left_->isLiteral(value) && value.is_null()) {
						return right_->queryModifiedDefinitionBasedOnResult(op_ == OP_NEQ ? result : !result, current_def);
					}
				}

				return ConstFormulaCallableDefinitionPtr();
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(left_);
				result.push_back(right_);
				return result;
			}

			bool canCreateVM() const override { return canChildrenVM(); }

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(left_);
				optimizeChildToVM(right_);

				if(left_->canCreateVM() && right_->canCreateVM()) {
					formula_vm::VirtualMachine vm;
					left_->emitVM(vm);
					right_->emitVM(vm);
					vm.addInstruction(op_);
					return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
				}

				return ExpressionPtr();
			}
	
			OP op_;
			ExpressionPtr left_, right_;
		};

		typedef std::map<std::string,ExpressionPtr> expr_table;
		typedef std::shared_ptr<expr_table> expr_table_ptr;

		ConstFormulaCallableDefinitionPtr create_where_definition(expr_table_ptr table, ConstFormulaCallableDefinitionPtr def)
		{
			std::vector<std::string> items;
			std::vector<variant_type_ptr> types;
			for(std::map<std::string,ExpressionPtr>::const_iterator i = table->begin(); i != table->end(); ++i) {
				items.push_back(i->first);
				types.push_back(i->second->queryVariantType());
			}

			ASSERT_LOG(items.empty() == false, "EMPTY WHERE CLAUSE");

			FormulaCallableDefinitionPtr result = execute_command_callable_definition(&items[0], &items[0] + items.size(), def, &types[0]);
			result->setStrict(def && def->isStrict());
			return result;
		}

		class WhereExpression : public FormulaExpression {
		public:
			WhereExpression(ExpressionPtr body, WhereVariablesInfoPtr info)
			: FormulaExpression("_where"), body_(body), info_(info)
			{
			}
	
		private:
			ExpressionPtr optimize() const override {

				WhereExpression* base_where = dynamic_cast<WhereExpression*>(body_.get());
				if(base_where == NULL) {
					return ExpressionPtr();
				}

				WhereVariablesInfo& base_info = *base_where->info_;

				WhereVariablesInfoPtr res(new WhereVariablesInfo(*info_));
				res->callable_where_def = base_info.callable_where_def;

				res->names.insert(res->names.end(), base_info.names.begin(), base_info.names.end());
				res->entries.insert(res->entries.end(), base_info.entries.begin(), base_info.entries.end());

				return ExpressionPtr(new WhereExpression(base_where->body_, res));
			}

			variant_type_ptr getVariantType() const override {
				return body_->queryVariantType();
			}

			ExpressionPtr body_;
			WhereVariablesInfoPtr info_;
	
			variant execute(const FormulaCallable& variables) const override {
				FormulaCallablePtr wrapped_variables(new WhereVariables(variables, info_));
				return body_->evaluate(*wrapped_variables);
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(body_);
				result.insert(result.end(), info_->entries.begin(), info_->entries.end());
				return result;
			}

			bool canCreateVM() const override { return canChildrenVM(); }

			ExpressionPtr optimizeToVM() override {

				bool can_vm = canCreateVM();
				if(can_vm && g_strict_formula_checking && info_->callable_where_def) {
		//			const_cast<FormulaCallableDefinition*>(info_->callable_where_def.get())->setHasSymbolIndexes();
				}

				optimizeChildToVM(body_);
				for(ExpressionPtr& e : info_->entries) {
					optimizeChildToVM(e);
				}

				if(!can_vm) {
					return ExpressionPtr();
				}

				VMExpression* vm_body = dynamic_cast<VMExpression*>(body_.get());
				std::vector<VMExpression*> vm_entries;
				for(ExpressionPtr& e : info_->entries) {
					vm_entries.push_back(dynamic_cast<VMExpression*>(e.get()));
				}

				static int num_where = 0;
				static int num_opt_where = 0;

				++num_where;

				if(g_ffl_vm_opt_replace_where && vm_body != nullptr && std::count(vm_entries.begin(), vm_entries.end(), nullptr) == 0) {

					std::map<int, VirtualMachine::Iterator> lookups;
					std::vector<VirtualMachine::Iterator> ordered_lookups;

					int loop_end = -1;

					bool can_optimize = true;
					
					std::vector<formula_vm::VirtualMachine> all_vm;
					all_vm.reserve(vm_entries.size() + 1);
					all_vm.emplace_back(vm_body->get_vm());
					std::reverse(vm_entries.begin(), vm_entries.end());
					for(auto e : vm_entries) {
						all_vm.emplace_back(e->get_vm());
					}

					std::vector<bool> vm_trivial;
					for(int n = 0; n < vm_entries.size(); ++n) {
						auto i = all_vm[all_vm.size() - n - 1].begin_itor();
						if(!i.at_end()) {
							i.next();
						}

						vm_trivial.push_back(i.at_end());
					}

					for(auto& vm : all_vm) {

						std::vector<bool> unrelated_scope_stack;

						for(VirtualMachine::Iterator itor(vm.begin_itor()); !itor.at_end(); itor.next()) {
							if(formula_vm::VirtualMachine::isInstructionLoop(itor.get())) {
								const int end = static_cast<int>(itor.get_index()) + itor.arg();
								if(end > loop_end) {
									loop_end = end;
								}
							} else if(itor.get() == formula_vm::OP_PUSH_SCOPE) {
								unrelated_scope_stack.push_back(true);
							} else if(itor.get() == formula_vm::OP_INLINE_FUNCTION) {
								unrelated_scope_stack.push_back(false);
							} else if(itor.get() == formula_vm::OP_WHERE && itor.arg() >= 0) {
								unrelated_scope_stack.push_back(false);
							} else if(itor.get() == formula_vm::OP_POP_SCOPE) {
								assert(unrelated_scope_stack.empty() == false);
								unrelated_scope_stack.pop_back();
							} else if((itor.get() == formula_vm::OP_LOOKUP_STR && std::find(unrelated_scope_stack.begin(), unrelated_scope_stack.end(), true) == unrelated_scope_stack.end()) || itor.get() == formula_vm::OP_CALL_BUILTIN_DYNAMIC || itor.get() == formula_vm::OP_LAMBDA_WITH_CLOSURE) {
								can_optimize = false;
								break;
							} else if(itor.get() == formula_vm::OP_LOOKUP && std::find(unrelated_scope_stack.begin(), unrelated_scope_stack.end(), true) == unrelated_scope_stack.end() && itor.arg() >= info_->base_slot && itor.arg() < info_->base_slot + info_->entries.size()) {

								const int index = itor.arg() - info_->base_slot;
								assert(index >= 0 && index < vm_trivial.size());

								if((static_cast<int>(itor.get_index()) < loop_end || lookups.count(itor.arg()) > 0) && vm_trivial[index] == false) {
									can_optimize = false;
									break;
								}

								lookups.insert(std::pair<int, VirtualMachine::Iterator>(itor.arg(), itor));
	
								ordered_lookups.emplace_back(itor);
							}
						}

						if(!can_optimize) {
							break;
						}
					}

					if(can_optimize) {

						std::reverse(ordered_lookups.begin(), ordered_lookups.end());

						for(auto lookup : ordered_lookups) {
							VirtualMachine* vm = const_cast<VirtualMachine*>(lookup.get_vm());
							auto next_itor = lookup;
							next_itor.next();

							const int index = lookup.arg() - info_->base_slot;
							assert(index >= 0 && index < static_cast<int>(info_->entries.size()));

							vm->append(lookup, next_itor, all_vm[all_vm.size() - index - 1]);
						}

						++num_opt_where;

						return ExpressionPtr(new VMExpression(all_vm.front(), queryVariantType(), *this));
					}
				}

				formula_vm::VirtualMachine vm;

				bool first = true;
				for(ExpressionPtr& e : info_->entries) {
					e->emitVM(vm);
//					vm.addInstruction(formula_vm::OP_PUSH_SYMBOL_STACK);

					vm.addInstruction(formula_vm::OP_WHERE);
					if(first) {
						vm.addInt(info_->base_slot);
						first = false;
					} else {
						vm.addInt(-1);
					}
				}

				body_->emitVM(vm);
				vm.addInstruction(formula_vm::OP_POP_SCOPE);

				for(ExpressionPtr& e : info_->entries) {
//					vm.addInstruction(formula_vm::OP_POP_SYMBOL_STACK);
				}

				return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
			}
		};

		struct CommandSequenceEntry {
			const class CommandSequence* first;
			bool* second;
			ffl::IntrusivePtr<class CommandSequence> deferred;
			CommandSequenceEntry(const class CommandSequence* seq, bool* flag) : first(seq), second(flag)
			{}

			CommandSequenceEntry() : first(nullptr), second(nullptr) {}

		};

		std::vector<CommandSequenceEntry> g_command_sequence_stack;

		struct CommandSequenceStackScope {
			bool deferred = false;
			explicit CommandSequenceStackScope(const class CommandSequence* seq) {
				g_command_sequence_stack.push_back(CommandSequenceEntry(seq, &deferred));
			}

			~CommandSequenceStackScope() {
				g_command_sequence_stack.pop_back();
			}
		};


		class CommandSequence : public CommandCallable {
			variant cmd_;
			ExpressionPtr right_;
			ConstFormulaCallablePtr variables_;
			mutable int nbarrier_;

			void surrenderReferences(GarbageCollector* collector) override {
				collector->surrenderVariant(&cmd_, "cmd");
				collector->surrenderPtr(&variables_, "variables");
			}
		public:
			CommandSequence(const variant& cmd, ExpressionPtr right_expr, ConstFormulaCallablePtr variables)
			  : cmd_(cmd), right_(right_expr), variables_(variables), nbarrier_(0)
			{}

			void createBarrier() { ++nbarrier_; }

			void execute(game_logic::FormulaCallable& ob) const override {
				if(nbarrier_ > 0) {
					--nbarrier_;
					return;
				}

				{
					CommandSequenceStackScope scope(this);
					ob.executeCommand(cmd_);
					if(scope.deferred) {
						return;
					}
				}

				formula_profiler::Instrument instrument("CMD_EVAL");
				const variant right_cmd = right_->evaluate(*variables_);
				formula_profiler::Instrument instrument2("CMD_EXEC");
				ob.executeCommand(right_cmd);
			}

			ffl::IntrusivePtr<CommandSequence> createDeferred() const
			{
				return ffl::IntrusivePtr<CommandSequence>(new CommandSequence(variant(), right_, variables_));
			}
		};

		struct MultiCommandSequenceStackScope {
			std::vector<ffl::IntrusivePtr<CommandSequence> >* stack_;
			bool deferred_;
			explicit MultiCommandSequenceStackScope(std::vector<ffl::IntrusivePtr<CommandSequence> >* stack) : stack_(stack), deferred_(false)
			{
				for(auto p : *stack_) {
					g_command_sequence_stack.push_back(CommandSequenceEntry(p.get(), &deferred_));
				}
			}

			~MultiCommandSequenceStackScope() {
				g_command_sequence_stack.resize(g_command_sequence_stack.size() - stack_->size());
			}
		};

		class DeferredCommandSequence : public CommandCallable {
			mutable std::vector<ffl::IntrusivePtr<CommandSequence> > stack_;
		public:
			DeferredCommandSequence() {
				stack_.reserve(g_command_sequence_stack.size());
				for(auto& seq : g_command_sequence_stack) {
					*seq.second = true;
					if(!seq.deferred) {
						seq.deferred = seq.first->createDeferred();
					} else {
						seq.deferred->createBarrier();
					}
					stack_.push_back(seq.deferred);
				}
			}

			void execute(game_logic::FormulaCallable& ob) const override {
				MultiCommandSequenceStackScope scope(&stack_);
				while(scope.deferred_ == false && stack_.empty() == false) {
					auto seq = stack_.back();
					stack_.pop_back();
					g_command_sequence_stack.pop_back();
					seq->execute(ob);
				}
			}

			void surrenderReferences(GarbageCollector* collector) override {
				for(ffl::IntrusivePtr<CommandSequence>& p : stack_) {
					collector->surrenderPtr(&p);
				}
			}
		};

	} //namespace

	variant deferCurrentCommandSequence()
	{
		if(g_command_sequence_stack.empty()) {
			return variant();
		} else {
			return variant(new DeferredCommandSequence);
		}
	}

	namespace {

		class CommandSequenceExpression : public FormulaExpression {
			ExpressionPtr left_, right_;
		public:
			CommandSequenceExpression(ExpressionPtr left, ExpressionPtr right)
			  : left_(left), right_(right)
			{}

			variant_type_ptr getVariantType() const override {
				return variant_type::get_commands();
			}

			void staticErrorAnalysis() const override {
				if(left_) {
					variant_type_ptr left_type = left_->queryVariantType();
					ASSERT_LOG(variant_types_compatible(variant_type::get_commands(), left_type), "Expression to the left of ; must be of commands type, is of type " << left_type->to_string() << " " << debugPinpointLocation());
				}

				variant_type_ptr right_type = right_->queryVariantType();
				ASSERT_LOG(variant_types_compatible(variant_type::get_commands(), right_type), "Expression to the right of ; must be of commands type, is of type " << right_type->to_string() << " " << debugPinpointLocation());
			}

			variant execute(const FormulaCallable& variables) const override {

				Formula::failIfStaticContext();

				variant cmd;
				if(left_) {
					cmd = left_->evaluate(variables);
				}
				auto res = (new CommandSequence(cmd, right_, ConstFormulaCallablePtr(&variables)));
				return variant(res);
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				if(left_) {
					result.push_back(left_);
				}
				result.push_back(right_);
				return result;
			}

			bool canCreateVM() const override { return false; }

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(left_);
				optimizeChildToVM(right_);
				return ExpressionPtr();
			}
		};

		class LetExpression : public FormulaExpression {
			std::vector<std::string> names_;
			std::string identifier_;
			int slot_;

			ExpressionPtr let_expr_;
			ExpressionPtr right_expr_;

		public:

			LetExpression(const std::string& identifier, int slot, ExpressionPtr let_expr, ExpressionPtr right_expr)
			   : identifier_(identifier), slot_(slot), let_expr_(let_expr), right_expr_(right_expr)
			{
				names_.push_back(identifier);
			}

			variant_type_ptr getVariantType() const override {
				return right_expr_->queryVariantType();
			}

			variant execute(const FormulaCallable& variables) const override {
				const variant value = let_expr_->evaluate(variables);

				ffl::IntrusivePtr<MutableSlotFormulaCallable> callable(new MutableSlotFormulaCallable);
				callable->setFallback(&variables);
				callable->setBaseSlot(slot_);
				callable->setNames(&names_);
				callable->add(value);

				return right_expr_->evaluate(*callable);
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(let_expr_);
				result.push_back(right_expr_);
				return result;
			}

			bool canCreateVM() const override { return false; }

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(let_expr_);
				optimizeChildToVM(right_expr_);
				return ExpressionPtr();
			}
		};

		class IsExpression : public FormulaExpression {
		public:
			IsExpression(
					variant_type_ptr type, ExpressionPtr expr,
					bool negative = false)
				: FormulaExpression("_is"), type_(type)
				, expression_(expr), negative_(negative)
			{
			}

		private:
			variant_type_ptr getVariantType() const override {
				return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
			}

			variant execute(const FormulaCallable& variables) const override {
				const variant value = expression_->evaluate(variables);
				bool matching = type_->match(value);
				return variant::from_bool(
						negative_ ? !matching : matching);
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(expression_);
				return result;
			}

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(expression_);
				if(expression_->canCreateVM()) {
					formula_vm::VirtualMachine vm;
					expression_->emitVM(vm);
					vm.addLoadConstantInstruction(variant(type_.get()));
					vm.addInstruction(negative_ ? OP_IS_NOT : OP_IS);
					return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
				}
				return ExpressionPtr();
			}

			ConstFormulaCallableDefinitionPtr getModifiedDefinitionBasedOnResult(bool result, ConstFormulaCallableDefinitionPtr current_def, variant_type_ptr expression_is_this_type) const override {
				if(expression_is_this_type) {
					return ConstFormulaCallableDefinitionPtr();
				}

				return expression_->queryModifiedDefinitionBasedOnResult(result, current_def, type_);
			}

			variant_type_ptr type_;
			ExpressionPtr expression_;
			bool negative_;
		};

		class StaticTypeExpression : public FormulaExpression {
		public:
			StaticTypeExpression(variant_type_ptr type, ExpressionPtr expr)
			: FormulaExpression("_static_type"), type_(type), expression_(expr)
			{
				const FormulaInterface* formula_interface = type->is_interface();
				if(formula_interface) {
					ffl::IntrusivePtr<FormulaInterfaceInstanceFactory> interface_factory;
					try {
						interface_factory.reset(formula_interface->createFactory(expr->queryVariantType()));
					} catch(FormulaInterface::interface_mismatch_error& e) {
						ASSERT_LOG(false, "Could not create interface: " << e.msg << " " << debugPinpointLocation());
					}

					interface_ = interface_factory;
				}
			}
	
		private:
			variant_type_ptr getVariantType() const override {
				return type_;
			}

			variant_type_ptr type_;
			ExpressionPtr expression_;
	
			variant execute(const FormulaCallable& variables) const override {
				if(interface_) {
					return interface_->create(expression_->evaluate(variables));
				} else {
					return expression_->evaluate(variables);
				}
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(expression_);
				return result;
			}

			bool canCreateVM() const override { return false; }

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(expression_);
				return ExpressionPtr();
			}

			ExpressionPtr optimize() const override {
				if(!interface_) {
					return expression_;
				} else {
					return ExpressionPtr();
				}
			}

			void staticErrorAnalysis() const override {
				if(variant_types_compatible(type_, expression_->queryVariantType()) == false) {
					std::ostringstream reason;
					ASSERT_LOG(variant_types_compatible(type_, expression_->queryVariantType(), &reason), "Expression is not declared type. Of type " << expression_->queryVariantType()->to_string() << " when type " << type_->to_string() << " expected (" << reason.str() << ") " << debugPinpointLocation());
				}
			}

			ffl::IntrusivePtr<FormulaInterfaceInstanceFactory> interface_;
		};

		class TypeExpression : public FormulaExpression {
		public:
			TypeExpression(variant_type_ptr type, ExpressionPtr expr)
			: FormulaExpression("_type"), type_(type), expression_(expr)
			{
			}
	
		private:
			variant_type_ptr getVariantType() const override {
				return type_;
			}

			variant_type_ptr type_;
			ExpressionPtr expression_;
	
			variant execute(const FormulaCallable& variables) const override {
				const variant result = expression_->evaluate(variables);
				ASSERT_LOG(type_->match(result), "TYPE MIS-MATCH: EXPECTED " << type_->to_string() << " BUT FOUND " << result.write_json() << " OF TYPE '" << get_variant_type_from_value(result)->to_string() << "' " << type_->mismatch_reason(result) << " AT " << debugPinpointLocation());
				return result;
			}

			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(expression_);
				return result;
			}

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(expression_);
				if(expression_->canCreateVM()) {
					formula_vm::VirtualMachine vm;
					expression_->emitVM(vm);

					vm.addInstruction(OP_DUP);
					vm.addLoadConstantInstruction(variant(type_.get()));
					vm.addInstruction(OP_IS);
					const int jump_source = vm.addJumpSource(OP_POP_JMP_IF);

					vm.addLoadConstantInstruction(variant(formatter() << "Type mis-match. Expected " << type_->to_string() << " found "));
					vm.addInstruction(OP_SWAP);
					vm.addInstruction(OP_ADD);
					vm.addInstruction(OP_PUSH_NULL);

					vm.addInstruction(OP_ASSERT);
					vm.jumpToEnd(jump_source);

					return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
				}
				return ExpressionPtr();
			}
		};

		class AssertExpression : public FormulaExpression {
		public:
			AssertExpression(ExpressionPtr body, const std::vector<ExpressionPtr>& asserts, ExpressionPtr debug_expr)
			  : FormulaExpression("_assert"), body_(body), asserts_(asserts), debug_(debug_expr)
			{
			}

		private:
			ExpressionPtr body_, debug_;
			std::vector<ExpressionPtr> asserts_;

			variant execute(const FormulaCallable& variables) const override {
				for(const ExpressionPtr& a : asserts_) {
					if(!a->evaluate(variables).as_bool()) {
						OperatorExpression* op_expr = dynamic_cast<OperatorExpression*>(a.get());

						std::ostringstream expr_info;
						if(op_expr) {
							expr_info << "  " << op_expr->get_left()->str() << ": " << op_expr->get_left()->evaluate(variables).to_debug_string() << "\n";
							expr_info << "  " << op_expr->get_right()->str() << ": " << op_expr->get_right()->evaluate(variables).to_debug_string() << "\n";
						}

						if(debug_) {
							expr_info << "DEBUG EXPRESSION: " << debug_->str() << " -> " << debug_->evaluate(variables).to_debug_string() << "\n";
						}

						ASSERT_LOG(false,
								   "FORMULA ASSERTION FAILED: " << a->str() << " -- " << a->debugPinpointLocation() << "\n" << expr_info.str());
					}
				}

				return body_->evaluate(variables);
			}

			variant_type_ptr getVariantType() const override {
				return body_->queryVariantType();
			}
	
			std::vector<ConstExpressionPtr> getChildren() const override {
				std::vector<ConstExpressionPtr> result;
				result.push_back(body_);
				result.push_back(debug_);
				return result;
			}

			ExpressionPtr optimizeToVM() override {
				optimizeChildToVM(body_);
				optimizeChildToVM(debug_);
				bool can_vm = body_->canCreateVM() && (!debug_ || debug_->canCreateVM());
				for(ExpressionPtr& a : asserts_) {
					optimizeChildToVM(a);
					can_vm = can_vm && a->canCreateVM();
				}

				if(can_vm) {
					formula_vm::VirtualMachine vm;
					for(const ExpressionPtr& a : asserts_) {
						a->emitVM(vm);
						const int jump_source = vm.addJumpSource(OP_JMP_IF);
						vm.addLoadConstantInstruction(variant(a->str()));
						if(debug_) {
							debug_->emitVM(vm);
						} else {
							vm.addInstruction(OP_PUSH_NULL);
						}
						vm.addInstruction(OP_ASSERT);
						vm.jumpToEnd(jump_source);
						vm.addInstruction(OP_POP);
					}

					body_->emitVM(vm);

					return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
				}

				return ExpressionPtr();
			}
		};


		class IntegerExpression : public FormulaExpression {
		public:
			explicit IntegerExpression(int i) : FormulaExpression("_int"), i_(i)
			{}

			bool canCreateVM() const override { return true; }

			ExpressionPtr optimizeToVM() override {
				formula_vm::VirtualMachine vm;
				vm.addLoadConstantInstruction(i_);
				return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
			}
		private:
			variant execute(const FormulaCallable& /*variables*/) const override {
				return i_;
			}

			variant_type_ptr getVariantType() const override {
				return variant_type::get_type(variant::VARIANT_TYPE_INT);
			}
	
			variant i_;
		};

		class decimal_expression : public FormulaExpression {
		public:
			explicit decimal_expression(const decimal& d) : FormulaExpression("_decimal"), v_(d)
			{}

			bool canCreateVM() const override { return true; }
			ExpressionPtr optimizeToVM() override {
				formula_vm::VirtualMachine vm;
				vm.addLoadConstantInstruction(v_);
				return ExpressionPtr(new VMExpression(vm, queryVariantType(), *this));
			}
		private:
			variant execute(const FormulaCallable& /*variables*/) const override {
				return v_;
			}

			variant_type_ptr getVariantType() const override {
				return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
			}
	
			variant v_;
		};

		class StringExpression : public FormulaExpression {
		public:
			explicit StringExpression(std::string str, bool translate = false, FunctionSymbolTable* symbols = 0) : FormulaExpression("_string")
			{
				if (!g_verbatim_string_expressions) {
					const Formula::StrictCheckScope strict_checking(false);

					const std::string original = str;

					size_t pos = 0;
					//replace \\n sequences with newlines
					while((pos = str.find("\\n", pos)) != std::string::npos) {
						str = str.replace(pos, 2, "\n");
					}

					str.erase(std::remove(str.begin(), str.end(), '\t'), str.end());

					if (translate) {
						str = i18n::tr(str);
					}

					static const std::string BeginSub = "${";
					std::string::iterator i;
					while((i = std::search(str.begin(), str.end(), BeginSub.begin(), BeginSub.end())) != str.end()) {
						std::string::iterator j = std::find(i, str.end(), '}');
						if(j == str.end()) {
							break;
						}
			
						const std::string formula_str(i+BeginSub.size(), j);
						const auto pos = i - str.begin();
						str.erase(i, j+1);
			
						substitution sub;
						sub.pos = pos;
						sub.calculation.reset(new Formula(variant(formula_str), symbols));
						subs_.push_back(sub);
					}
		
					std::reverse(subs_.begin(), subs_.end());

					if(translate) {
						str_ = variant::create_translated_string(original, str);
						return;
					}
				} else if (translate) {
					str = std::string("~") + str + std::string("~");
				}
		
				str_ = variant(str);
			}

			bool isLiteral(variant& result) const override {
				if(subs_.empty()) {
					result = str_;
					return true;
				} else {
					return false;
				}
			}

			bool canReduceToVariant(variant& v) const override {
				if(subs_.empty()) {
					v = variant(str_);
					return true;
				} else {
					return false;
				}
			}

			bool canCreateVM() const override { return subs_.empty(); }

			ExpressionPtr optimizeToVM() override {
				if(subs_.empty()) {
					formula_vm::VirtualMachine vm;
					vm.addLoadConstantInstruction(str_);
					VMExpression* result = new VMExpression(vm, queryVariantType(), *this);
					result->setVariant(variant(str_));
					return ExpressionPtr(result);
				} else {
					//TODO: VM code for string subs.
					return ExpressionPtr();
				}
			}
			
		private:
			variant execute(const FormulaCallable& variables) const override {
				if(subs_.empty()) {
					return str_;
				} else {
					std::string res = str_.as_string();
					for(size_t i=0; i < subs_.size(); ++i) {
						const substitution& sub = subs_[i];
						const std::string str = sub.calculation->execute(variables).string_cast();
						res.insert(sub.pos, str);
					}
			
					return variant(res);
				}
			}

			variant_type_ptr getVariantType() const override {
				return variant_type::get_type(variant::VARIANT_TYPE_STRING);
			}
	
			struct substitution {
				std::ptrdiff_t pos;
				ConstFormulaPtr calculation;
			};
	
			variant str_;
			std::vector<substitution> subs_;
		};

		using namespace formula_tokenizer;
		int operator_precedence(const Token& t, const variant& formula_str)
		{
			static std::map<std::string,int> precedence_map;
			if(precedence_map.empty()) {
				int n = 0;
				precedence_map[";"] = ++n;
				precedence_map["->"] = ++n;
				precedence_map["where"] = ++n;
				precedence_map["asserting"] = ++n;
				precedence_map["::"] = ++n;
				precedence_map["<-"] = ++n;
				precedence_map["or"]    = ++n;
				precedence_map["and"]   = ++n;
				precedence_map["not"] = ++n;
				precedence_map["in"] = ++n;
				precedence_map["is"] = ++n;
				precedence_map["="]     = ++n;
				precedence_map["!="]    = n;
				precedence_map["<"]     = n;
				precedence_map[">"]     = n;
				precedence_map["<="]    = n;
				precedence_map[">="]    = n;
				precedence_map["+"]     = ++n;
				precedence_map["-"]     = n;
				precedence_map["*"]     = ++n;
				precedence_map["/"]     = ++n;
				precedence_map["%"]     = ++n;
				precedence_map["^"]     = ++n;
				precedence_map["d"]     = ++n;
				precedence_map["<<"]     = ++n;

				//these operators are equal precedence, and left
				//associative. Thus, x.y[4].z = ((x.y)[4]).z
				precedence_map["["]     = ++n;
				precedence_map["("]     = n;
				precedence_map["."]     = n;
			}
	
			ASSERT_LOG(precedence_map.count(std::string(t.begin,t.end)), "Unknown precedence for '" << std::string(t.begin,t.end) << "': " << pinpoint_location(formula_str, t.begin, t.end));
			return precedence_map[std::string(t.begin,t.end)];
		}

		ExpressionPtr parse_expression(const variant& formula_str, const Token* i1, const Token* i2, FunctionSymbolTable* symbols, ConstFormulaCallableDefinitionPtr callable_def, bool* can_optimize=nullptr);

		void parse_function_args(variant formula_str, const Token* &i1, const Token* i2,
								 std::vector<std::string>* res,
								 std::vector<std::string>* types,
								 std::vector<variant_type_ptr>* variant_types,
								 std::vector<variant>* default_values,
								 variant_type_ptr* result_type)
		{
			if(i1->type == FFL_TOKEN_TYPE::LPARENS) {
				++i1;
			} else {
				ASSERT_LOG(false, "Invalid function definition\n" << pinpoint_location(formula_str, i1->begin, (i2-1)->end));
			}
	
			while((i1->type != FFL_TOKEN_TYPE::RPARENS) && (i1 != i2)) {
				variant_type_ptr variant_type_info;
				if(i1+1 != i2 && i1->type != FFL_TOKEN_TYPE::COMMA && (i1+1)->type != FFL_TOKEN_TYPE::COMMA && (i1+1)->type != FFL_TOKEN_TYPE::RPARENS && std::string((i1+1)->begin, (i1+1)->end) != "=") {
					variant_type_info = parse_variant_type(formula_str, i1, i2);
				}

				ASSERT_LOG(i1->type != FFL_TOKEN_TYPE::RPARENS && i1 != i2, "UNEXPECTED END OF FUNCTION DEF: " << pinpoint_location(formula_str, (i1-1)->begin, (i1-1)->end));

				if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER) {
					if(i1+1 != i2 && std::string((i1+1)->begin, (i1+1)->end) == "=") {
						types->push_back("");
						res->push_back(std::string(i1->begin, i1->end));
						variant_types->push_back(variant_type_info);

						i1 += 2;
						ASSERT_LOG(i1 != i2, "Invalid function definition\n" << pinpoint_location(formula_str, i1->begin, (i2-1)->end));

						const Token* begin = i1;
						if(!TokenMatcher().add(FFL_TOKEN_TYPE::COMMA).add(FFL_TOKEN_TYPE::RPARENS)
							.find_match(i1, i2)) {
							ASSERT_LOG(false, "Invalid function definition\n" << pinpoint_location(formula_str, i1->begin, (i2-1)->end));
						}

						const ExpressionPtr expr = parse_expression(
							formula_str, begin, i1, nullptr, nullptr);

						ffl::IntrusivePtr<MapFormulaCallable> callable(new MapFormulaCallable);
						default_values->push_back(expr->evaluate(*callable));
						if(variant_type_info && !variant_type_info->match(default_values->back())) {
							ASSERT_LOG(false, "Default argument to function doesn't match type for argument " << (types->size()+1) << " arg: " << default_values->back().write_json() << " AT: " << pinpoint_location(formula_str, i1->begin, (i2-1)->end));
						}

						continue;

					} else if(default_values->empty() == false) {
						ASSERT_LOG(i1 != i2, "Invalid function definition: some args do not have a default value after some args do\n" << pinpoint_location(formula_str, i1->begin, (i2-1)->end));
					} else if(i1+1 != i2 && std::string((i1+1)->begin, (i1+1)->end) == "*") {
						types->push_back("");
						res->push_back(std::string(i1->begin, i1->end) + std::string("*"));
						variant_types->push_back(variant_type_info);
						++i1;
					} else if(i1+1 != i2 && (i1+1)->type == FFL_TOKEN_TYPE::IDENTIFIER) {
						types->push_back(std::string(i1->begin, i1->end));
						res->push_back(std::string((i1+1)->begin, (i1+1)->end));
						variant_types->push_back(variant_type_info);
						++i1;
					} else {
						types->push_back("");
						res->push_back(std::string(i1->begin, i1->end));
						variant_types->push_back(variant_type_info);
					}
				} else if (i1->type == FFL_TOKEN_TYPE::COMMA) {
					//do nothing
				} else {
					ASSERT_LOG(false, "Invalid function definition\n" << pinpoint_location(formula_str, i1->begin, (i2-1)->end));
				}
				++i1;
			}
	
			if(i1->type != FFL_TOKEN_TYPE::RPARENS) {
				ASSERT_LOG(false, "Invalid function definition\n" << pinpoint_location(formula_str, i1->begin, (i2-1)->end));
			}
			++i1;
			ASSERT_LOG(i1 != i2, "Unexpected end of function definition (missing return type definition): " << pinpoint_location(formula_str, (i1-1)->begin, (i1-1)->end));

			if(i1 != i2 && i1->type == FFL_TOKEN_TYPE::POINTER) {
				++i1;
				ASSERT_LOG(i1 != i2, "Unexpected end of function definition: " << pinpoint_location(formula_str, (i1-1)->begin, (i1-1)->end));

				variant_type_ptr res = parse_variant_type(formula_str, i1, i2);
				if(result_type) {
					*result_type = res;
				}
			}
		}

		void parse_args(const variant& formula_str, const std::string* function_name,
						const Token* i1, const Token* i2,
						std::vector<ExpressionPtr>* res,
						FunctionSymbolTable* symbols,
						ConstFormulaCallableDefinitionPtr definition,
						bool* can_optimize)
		{
			std::vector<std::pair<const Token*, const Token*> > args;

			ASSERT_LE(i1, i2);
			int parens = 0;
			const Token* beg = i1;
			while(i1 != i2) {
				if(i1->type == FFL_TOKEN_TYPE::LPARENS || i1->type == FFL_TOKEN_TYPE::LSQUARE || i1->type == FFL_TOKEN_TYPE::LBRACKET ) {
					++parens;
				} else if(i1->type == FFL_TOKEN_TYPE::RPARENS || i1->type == FFL_TOKEN_TYPE::RSQUARE || i1->type == FFL_TOKEN_TYPE::RBRACKET) {
					--parens;
				} else if(i1->type == FFL_TOKEN_TYPE::COMMA && !parens) {
					args.push_back(std::pair<const Token*, const Token*>(beg, i1));
					beg = i1+1;
				}
		
				++i1;
			}
	
			if(beg != i1) {
				args.push_back(std::pair<const Token*, const Token*>(beg, i1));
			}

			for(int n = 0; n != args.size(); ++n) {
				ConstFormulaCallableDefinitionPtr callable_def(definition);

				if(n+1 == args.size()) {
					//Certain special functions take a special callable definition
					//to evaluate their last argument. Discover what that is here.
					static const std::string MapCallableFuncs[] = { "count", "filter", "find", "find_or_die", "find_index", "find_index_or_die", "choose", "map" };
					if(args.size() >= 2 && function_name != nullptr && std::count(MapCallableFuncs, MapCallableFuncs + sizeof(MapCallableFuncs)/sizeof(*MapCallableFuncs), *function_name)) {
						std::string value_name = "value";

						static const std::string CustomIdMapCallableFuncs[] = { "filter", "find", "map", "find_index", "find_index_or_die" };
						if(args.size() == 3 && std::count(CustomIdMapCallableFuncs, CustomIdMapCallableFuncs + sizeof(CustomIdMapCallableFuncs)/sizeof(*CustomIdMapCallableFuncs), *function_name)) {
							//invocation like map(range(5), n, n*n) -- need to discover
							//the string for the second argument to set that in our
							//callable definition
							variant literal;
							res->back()->isLiteral(literal);
							if(literal.is_string()) {
								value_name = literal.as_string();
							} else if(res->back()->isIdentifier(&value_name) == false) {
								ASSERT_LOG(false, "Function " << *function_name << " requires a literal as its second argument: " << pinpoint_location(formula_str, args[1].first->begin, (args[1].second-1)->end));
							}
						}
						ASSERT_LOG(args.size() == 2 || args.size() == 3, "WRONG NUMBER OF ARGS TO " << *function_name << " AT " << pinpoint_location(formula_str, args[0].first->begin, (args[0].second-1)->end));

						variant_type_ptr key_type, value_type;

						variant_type_ptr sequence_type = (*res)[0]->queryVariantType();
						if(sequence_type->is_type(variant::VARIANT_TYPE_STRING)) {
							value_type = variant_type::get_type(variant::VARIANT_TYPE_STRING);
						} else {
							value_type = sequence_type->is_list_of();
						}

						if(!value_type) {
							key_type = sequence_type->is_map_of().first;
							value_type = sequence_type->is_map_of().second;
						}

						callable_def = get_map_callable_definition(callable_def, key_type, value_type, value_name);
					}
				}

				if(function_name != nullptr &&
				   ((n == 1 && (*function_name == "sort" || *function_name == "fold")) ||
					(n == 2 &&  *function_name == "zip"))) {
					variant_type_ptr sequence_type = (*res)[0]->queryVariantType();
					variant_type_ptr value_type = sequence_type->is_list_of();
					if(!value_type && *function_name == "zip") {
						value_type = sequence_type->is_map_of().second;
					}

					callable_def = get_variant_comparator_definition(callable_def, value_type);
				}

				if(function_name != nullptr && (n == 4 || (args.size() == 3 && n == 2)) &&
				   (*function_name == "spawn" || *function_name == "spawn_player")) {
					//The spawn custom_object_functions take a special child
					//argument as their last parameter.
					static std::string Items[] = { "child" };
					variant_type_ptr types[1];
					variant literal;
					if((*res)[0]->isLiteral(literal) && literal.is_string()) {
						types[0] = variant_type::get_custom_object(literal.as_string());
					} else {
						types[0] = variant_type::get_custom_object();
					}

					callable_def = game_logic::execute_command_callable_definition(&Items[0], &Items[0] + sizeof(Items)/sizeof(*Items), callable_def, types);
				}

				if(function_name != nullptr && *function_name == "if" && n >= 1) {
					ConstFormulaCallableDefinitionPtr new_def = callable_def;

					for(int m = 0; m < n; m += 2) {
						if(!new_def) {
							new_def = callable_def;
						}
						new_def = (*res)[m]->queryModifiedDefinitionBasedOnResult(m+1 == n, new_def);
					}

					if(new_def) {
						callable_def = new_def;
					}
				}

				res->push_back(parse_expression(formula_str, args[n].first, args[n].second, symbols, callable_def, can_optimize));
				res->back()->setDefinitionUsedByExpression(callable_def);
			}
		}

		void parse_set_args(const variant& formula_str, const Token* i1, const Token* i2,
							std::vector<ExpressionPtr>* res,
							FunctionSymbolTable* symbols,
							ConstFormulaCallableDefinitionPtr callable_def)
		{
			const size_t begin_size = res->size();
			int parens = 0;
			bool check_pointer = false;
			const Token* beg = i1;
			while(i1 != i2) {
				if(i1->type == FFL_TOKEN_TYPE::LPARENS || i1->type == FFL_TOKEN_TYPE::LSQUARE || i1->type == FFL_TOKEN_TYPE::LBRACKET) {
					++parens;
				} else if(i1->type == FFL_TOKEN_TYPE::RPARENS || i1->type == FFL_TOKEN_TYPE::RSQUARE || i1->type == FFL_TOKEN_TYPE::RBRACKET) {
					--parens;
				} else if(i1->type == FFL_TOKEN_TYPE::COLON && !parens ) {
					if (!check_pointer) {
						check_pointer = true;

						if(i1 - beg == 1 && beg->type == FFL_TOKEN_TYPE::IDENTIFIER) {
							//make it so that {a: 4} is the same as {'a': 4}
							res->push_back(ExpressionPtr(new VariantExpression(variant(std::string(beg->begin, beg->end)))));
						} else {
							res->push_back(parse_expression(formula_str, beg,i1, symbols, callable_def));
						}
						beg = i1+1;
					} else {
						if((i1-1)->type == FFL_TOKEN_TYPE::IDENTIFIER || (i1-1)->type == FFL_TOKEN_TYPE::STRING_LITERAL) {
							ASSERT_LOG(false, "Missing comma\n" << pinpoint_location(formula_str, (i1-2)->end, (i1-2)->end));
						} else {
							ASSERT_LOG(false, "Too many ':' operators.\n" << pinpoint_location(formula_str, i1->begin, (i2-1)->end));
						}
					}
				} else if( i1->type == FFL_TOKEN_TYPE::COMMA && !parens ) {
					ASSERT_LOG(check_pointer, "Expected ':' and found ',' instead\n" << pinpoint_location(formula_str, i1->begin, (i2-1)->end));
					check_pointer = false;
					res->push_back(parse_expression(formula_str, beg,i1, symbols, callable_def));
					beg = i1+1;
				}
		
				++i1;
			}

			if(beg != i1) {
				res->push_back(parse_expression(formula_str, beg,i1, symbols, callable_def));
			}

			ASSERT_LOG((res->size() - begin_size)%2 == 0, "Expected : before end of map expression.\n" << pinpoint_location(formula_str, (i2-1)->end, (i2-1)->end));
		}

		void parse_where_clauses(const variant& formula_str,
								 const Token* i1, const Token * i2,
								 expr_table_ptr res, FunctionSymbolTable* symbols,
								 ConstFormulaCallableDefinitionPtr callable_def) {
			int parens = 0;
			const Token *original_i1_cached = i1;
			const Token *beg = i1;
			std::string var_name;
			while(i1 != i2) {
				if(i1->type == FFL_TOKEN_TYPE::LPARENS || i1->type == FFL_TOKEN_TYPE::LBRACKET || i1->type == FFL_TOKEN_TYPE::LSQUARE) {
					++parens;
				} else if(i1->type == FFL_TOKEN_TYPE::RPARENS || i1->type == FFL_TOKEN_TYPE::RBRACKET || i1->type == FFL_TOKEN_TYPE::RSQUARE) {
					--parens;
				} else if(!parens) {
					if(i1->type == FFL_TOKEN_TYPE::COMMA) {
						if(var_name.empty()) {
							ASSERT_LOG(false, "There is 'where <expression>,; "
							<< "'where name=<expression>,' was needed.\n" <<
							pinpoint_location(formula_str, i1->begin));
						}
						(*res)[var_name] = parse_expression(formula_str, beg,i1, symbols, callable_def);
						beg = i1+1;
						var_name = "";
					} else if(i1->type == FFL_TOKEN_TYPE::OPERATOR) {
						std::string op_name(i1->begin, i1->end);
						if(op_name == "=") {
							if(beg->type != FFL_TOKEN_TYPE::IDENTIFIER || beg+1 != i1 || !var_name.empty()) {
								ASSERT_LOG(false, "Unexpected tokens after where\n"
								  << pinpoint_location(formula_str, i1->begin));
							}
							var_name.insert(var_name.end(), beg->begin, beg->end);
							beg = i1+1;
						}
					}
				}
				++i1;
			}
			if(beg != i1) {
				if(var_name.empty()) {
					ASSERT_LOG(false, "Unexpected tokens after where\n" <<
								pinpoint_location(formula_str, beg->begin));
				}
				(*res)[var_name] = parse_expression(formula_str, beg,i1, symbols, callable_def);
			}
		}

		ExpressionPtr parse_expression_internal(const variant& formula_str, const Token* i1, const Token* i2, FunctionSymbolTable* symbols, ConstFormulaCallableDefinitionPtr callable_def, bool* can_optimize=nullptr);

		namespace 
		{
			//only allow one static_FormulaCallable to be active at a time.
			bool static_FormulaCallable_active = false;
	
			//a special callable which will throw an exception if it's actually called.
			//we use this to determine if an expression is static -- i.e. doesn't
			//depend on input, and can be reduced to its result.
			struct non_static_expression_exception {};
			class static_FormulaCallable : public FormulaCallable {
				static_FormulaCallable(const static_FormulaCallable&);
			public:
				static_FormulaCallable() : FormulaCallable(false) {
				}
		
				~static_FormulaCallable() {
				}
		
				variant getValue(const std::string& key) const override {
					if(key == "lib") {
						return variant(get_library_object().get());
					}

					throw non_static_expression_exception();
				}

				variant getValueBySlot(int slot) const override {
					throw non_static_expression_exception();
				}
			};

			class StaticFormulaCallableGuard {
				ffl::IntrusivePtr<static_FormulaCallable> callable_;
			public:
				StaticFormulaCallableGuard() : callable_(new static_FormulaCallable) {
					if(static_FormulaCallable_active) {
						throw non_static_expression_exception();
					}
			
					static_FormulaCallable_active = true;
				}

				~StaticFormulaCallableGuard() {
					static_FormulaCallable_active = false;
				}

				ffl::IntrusivePtr<static_FormulaCallable> callable() const { return callable_; }
				bool callableNotCopied() const { return callable_->refcount() == 1; }
			};

			//A helper function which queries an expression and finds all the occurrences where it
			//looks up a symbol in its enclosing scope.
			void query_formula_expression_lookups(ConstExpressionPtr expr, std::vector<const SlotIdentifierExpression*>* slot_expr, std::vector<const IdentifierExpression*>* id_expr, std::vector<const VMExpression*>* vm_expr) {

				std::vector<ConstExpressionPtr> children = expr->queryChildren();

				if(dynamic_cast<const DotExpression*>(expr.get())) {
					if(children.empty() == false) {
						query_formula_expression_lookups(children.front(), slot_expr, id_expr, vm_expr);
					}

					return;
				} else if(dynamic_cast<const SlotIdentifierExpression*>(expr.get())) {
					slot_expr->push_back(dynamic_cast<const SlotIdentifierExpression*>(expr.get()));
				} else if(dynamic_cast<const IdentifierExpression*>(expr.get())) {
					id_expr->push_back(dynamic_cast<const IdentifierExpression*>(expr.get()));
				} else if(dynamic_cast<const VMExpression*>(expr.get())) {
					vm_expr->push_back(dynamic_cast<const VMExpression*>(expr.get()));
				} else {
					for(auto c : children) {
						query_formula_expression_lookups(c, slot_expr, id_expr, vm_expr);
					}
				}
			}
		}

		int in_static_context = 0;
		struct static_context {
			static_context() { ++in_static_context; }
			~static_context() { --in_static_context; }
		};

		ExpressionPtr optimize_expression(ExpressionPtr result, FunctionSymbolTable* symbols, ConstFormulaCallableDefinitionPtr callable_def, bool reduce_to_static)
		{
			ExpressionPtr original = result;

			if(g_strict_formula_checking) {
				if(g_strict_formula_checking_warnings) {
					assert_recover_scope scope;
					try {
						original->performStaticErrorAnalysis();
					} catch(validation_failure_exception&) {
						LOG_ERROR("(assert treated as warning)");
					}
				} else {
					original->performStaticErrorAnalysis();
				}
			}

			if(result) {
				ExpressionPtr optimized = result->optimize();
				while(optimized) {
					result = optimized;
					optimized = result->optimize();
				}
			}

			if(reduce_to_static) {
				//we want to try to evaluate this expression, and see if it is static.
				//it is static if it never reads its input, if it doesn't call the rng,
				//and if a reference to the input itself is not stored.
				const rng::Seed rng_seed = rng::get_seed();
				StaticFormulaCallableGuard static_callable;
				try {
					variant res;
			
					{
						const static_context ctx;
						res = result->staticEvaluate(*static_callable.callable());
					}

					if(rng_seed == rng::get_seed() && static_callable.callableNotCopied()) {
						//this expression is static. Reduce it to its result.
						VariantExpression* expr = new VariantExpression(res);
						if(result) {
							expr->setTypeOverride(result->queryVariantType());
						}

						result.reset(expr);
					}
				} catch(non_static_expression_exception&) {
					//the expression isn't static. Not an error.
				} catch(fatal_assert_failure_exception& e) {
					ASSERT_LOG(false, "Error parsing formula: " << e.msg << "\n" << original->debugPinpointLocation());
				}
			}
	
			if(result) {
				result->copyDebugInfoFrom(*original);

				if(g_strict_formula_checking) {
					if(g_strict_formula_checking_warnings) {
						assert_recover_scope scope;
						try {
							original->performStaticErrorAnalysis();
						} catch(validation_failure_exception&) {
							LOG_ERROR("(assert treated as warning)");
						}
					} else {
						original->performStaticErrorAnalysis();
					}
				}
			}

			return result;
		}

		ExpressionPtr parse_expression(const variant& formula_str, const Token* i1, const Token* i2, FunctionSymbolTable* symbols, ConstFormulaCallableDefinitionPtr callable_def, bool* can_optimize)
		{
			bool optimize = true;
			ExpressionPtr result(parse_expression_internal(formula_str, i1, i2, symbols, callable_def, &optimize));
			result->setDebugInfo(formula_str, i1->begin, (i2-1)->end);

			result = optimize_expression(result, symbols, callable_def, optimize);

			if(can_optimize && !optimize) {
				*can_optimize = false;
			}

			return result;
		}

static std::string debugSubexpressionTypes(ConstFormulaPtr & fml)
{
	std::stringstream ss;
	for(auto & child : fml->expr()->queryChildrenRecursive()) {
		ss << "Type " << child->queryVariantType()->to_string() << "\n";
		ss << child->debugPinpointLocation(nullptr) << "\n\n";
	}
	return ss.str();
}
		//only returns a value in the case of a lambda function, otherwise
		//returns nullptr.
		ExpressionPtr parse_function_def(const variant& formula_str, const Token*& i1, const Token* i2, FunctionSymbolTable* symbols, ConstFormulaCallableDefinitionPtr callable_def)
		{
			assert(i1->type == FFL_TOKEN_TYPE::KEYWORD && std::string(i1->begin, i1->end) == "def");

			++i1;

			std::string formula_name;
			if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER) {
				formula_name = std::string(i1->begin, i1->end);
				++i1;

				ASSERT_LOG(i1 != i2, "Unexpected end of input\n" << pinpoint_location(formula_str, (i1-1)->begin, (i1-1)->end));
			}

			generic_variant_type_scope generic_scope;

			std::vector<std::string> generic_types;

			if(i1->type == FFL_TOKEN_TYPE::LDUBANGLE) {
				++i1;
				while(i1 != i2 && i1->type != FFL_TOKEN_TYPE::RDUBANGLE) {
					ASSERT_LOG(i1->type != FFL_TOKEN_TYPE::IDENTIFIER, "Generic type names must be Capitalized\n" << pinpoint_location(formula_str, i1->begin, i1->end));
					ASSERT_LOG(i1->type == FFL_TOKEN_TYPE::CONST_IDENTIFIER, "Unexpected token when looking for generic type name\n" << pinpoint_location(formula_str, i1->begin, i1->end));
					std::string id(i1->begin, i1->end);
					ASSERT_LOG(std::count(generic_types.begin(), generic_types.end(), id) == 0, "Repeated type name " << id << "\n" << pinpoint_location(formula_str, i1->begin, i1->end));

					generic_types.push_back(id);
					generic_scope.register_type(id);

					++i1;
					if(i1 != i2 && i1->type == FFL_TOKEN_TYPE::COMMA) {
						++i1;
					}
				}

				ASSERT_LOG(i1 != i2 && i1 + 1 != i2, "Unexpected end of input\n" << pinpoint_location(formula_str, (i1-1)->begin, (i1-1)->end));
				ASSERT_LOG(i1->type == FFL_TOKEN_TYPE::RDUBANGLE, "Unexpected token while looking for > to end generic function\n" << pinpoint_location(formula_str, i1->begin, i1->end));

				++i1;
			}
	
			std::vector<std::string> args, types;
			std::vector<variant> default_args;
			std::vector<variant_type_ptr> variant_types;
			variant_type_ptr result_type;
			parse_function_args(formula_str, i1, i2, &args, &types, &variant_types, &default_args, &result_type);

			ASSERT_LOG(i1 != i2, "Unexpected end of formula\n" << pinpoint_location(formula_str, (i1-1)->begin, (i1-1)->end));

			const Token* const beg = i1;
			while((i1 != i2) && (i1->type != FFL_TOKEN_TYPE::SEMICOLON || formula_name.empty())) {
				++i1;
			}
			ASSERT_LOG(beg != i2, "Unexpected end of function definition (missing return value definition): " << pinpoint_location(formula_str, (i1-1)->begin, (i1-1)->end));
			const std::string function_str = std::string(beg->begin, (i1-1)->end);
			variant function_var(function_str);
			if(formula_str.get_debug_info()) {
				//Set the debugging info for this new string, adjusting relative
				//to our parent formula, so we know where in the file it lies.
				const variant::debug_info* cur_info = formula_str.get_debug_info();
				variant::debug_info info = *cur_info;
				for(std::string::const_iterator i = formula_str.as_string().begin();
					i != beg->begin; ++i) {
					if(*i == '\n') {
						info.line++;
						info.column = 0;
					} else {
						info.column++;
					}
				}

				function_var.setDebugInfo(info);
			}
	
			std::shared_ptr<RecursiveFunctionSymbolTable> recursive_symbols(new RecursiveFunctionSymbolTable(formula_name.empty() ? "recurse" : formula_name, args, default_args, symbols, formula_name.empty() ? callable_def : nullptr, variant_types));

			//create a definition of the callable representing
			//function arguments.
			FormulaCallableDefinitionPtr args_definition;
			ConstFormulaCallableDefinitionPtr args_definition_ptr;
			if(args.size()) {
				args_definition = execute_command_callable_definition(&args[0], &args[0] + args.size(), formula_name.empty() ? callable_def : nullptr /*only get the surrounding scope if we have a lambda function.*/);
			} else if(formula_name.empty()) {
				//empty arg lambda function. Give the definition as our context.
				args_definition_ptr = callable_def;
			}

			if(args_definition) {
				args_definition_ptr = args_definition.get();
			}

			if(formula_name.empty() == false) {
				for(unsigned n = 0; n != types.size(); ++n) {
					ASSERT_LOG(n < args.size(), "FORMULA ARGS MIS-MATCH");

					if(types[n].empty()) {
						continue;
					}

					ASSERT_LOG(args_definition->getEntryById(args[n]) != nullptr, "FORMULA FUNCTION TYPE ARGS MIS-MATCH\n" << pinpoint_location(formula_str, i1->begin, i1->end));

					ConstFormulaCallableDefinitionPtr def = get_formula_callable_definition(types[n]);
					ASSERT_LOG(def != nullptr, "TYPE NOT FOUND: " << types[n] << "\n" << pinpoint_location(formula_str, i1->begin, i1->end));
					args_definition->getEntryById(args[n])->type_definition = def;
				}
			}

			if(args_definition) {
				for(unsigned n = 0; n != variant_types.size(); ++n) {
					args_definition->getEntryById(args[n])->setVariantType(variant_types[n]);
				}
			}

			if(generic_types.empty() == false) {
				ASSERT_LOG(formula_name.empty(), "non-lambda generic functions not currently supported\n" << pinpoint_location(formula_str, i1->begin, (i2-1)->end));
				ASSERT_LOG(result_type, "Generic functions must specify a result type" << pinpoint_location(formula_str, i1->begin, (i2-1)->end));
				ASSERT_LOG(args_definition, "Must have args definition in generic functions\n" << pinpoint_location(formula_str, i1->begin, i1->end));
				std::function<ConstFormulaPtr(const std::vector<variant_type_ptr>&)> factory =
				[=](const std::vector<variant_type_ptr>& types) {
					ASSERT_LOG(types.size() == generic_types.size(), "Incorrect number of arguments to generic function. Found " << types.size() << " expected " << generic_types.size());

					std::map<std::string, variant_type_ptr> mapping;
					for(unsigned n = 0; n != types.size(); ++n) {
						mapping[generic_types[n]] = types[n];
					}

					for(unsigned n = 0; n != variant_types.size(); ++n) {
						const variant_type_ptr def = variant_types[n]->map_generic_types(mapping);
						if(def) {
							args_definition->getEntryById(args[n])->setVariantType(def);
						}
					}

					ConstFormulaPtr fml(new Formula(function_var, recursive_symbols.get(), args_definition_ptr));
					return fml;
				};

				return ExpressionPtr(new GenericLambdaFunctionExpression(args, function_var, callable_def ? callable_def->getNumSlots() : 0, default_args, variant_types, result_type, recursive_symbols, generic_types, factory));
			}

			ConstFormulaPtr fml(new Formula(function_var, recursive_symbols.get(), args_definition_ptr));
			recursive_symbols->resolveRecursiveCalls(fml);
	
			if(formula_name.empty()) {
				bool uses_closure = false;

				//search and see if we make use of the closure. If we don't we can elide it.
				//this involves getting all the possible lookups the function makes and see
				//if any of them reference symbols in callable_def. If any of them do we
				//have to use the closure otherwise we don't.
				if(!callable_def) {
					uses_closure = true;
				} else {

					std::vector<const SlotIdentifierExpression*> slot_expr;
					std::vector<const IdentifierExpression*> id_expr;
					std::vector<const VMExpression*> vm_expr;
					query_formula_expression_lookups(fml->expr(), &slot_expr, &id_expr, &vm_expr);

					for(auto vm : vm_expr) {

						std::vector<bool> unrelated_scope_stack;

						for(VirtualMachine::Iterator itor(vm->get_vm().begin_itor()); !itor.at_end(); itor.next()) {
							if(itor.get() == formula_vm::OP_PUSH_SCOPE) {
								unrelated_scope_stack.push_back(true);
							} else if(itor.get() == formula_vm::OP_INLINE_FUNCTION) {
								unrelated_scope_stack.push_back(false);
							} else if(itor.get() == formula_vm::OP_WHERE && itor.arg() >= 0) {
								unrelated_scope_stack.push_back(false);
							} else if(itor.get() == formula_vm::OP_POP_SCOPE) {
								unrelated_scope_stack.pop_back();
							} else if((itor.get() == formula_vm::OP_LOOKUP_STR && std::find(unrelated_scope_stack.begin(), unrelated_scope_stack.end(), true) == unrelated_scope_stack.end()) || itor.get() == formula_vm::OP_CALL_BUILTIN_DYNAMIC || itor.get() == formula_vm::OP_LAMBDA_WITH_CLOSURE) {
								uses_closure = true;
								break;
							} else if(itor.get() == formula_vm::OP_LOOKUP && std::find(unrelated_scope_stack.begin(), unrelated_scope_stack.end(), true) == unrelated_scope_stack.end() && itor.arg() < callable_def->getNumSlots()) {
								uses_closure = true;
								break;
							}
						}

					}

					if(uses_closure == false) {
						for(auto id : id_expr) {
							if(callable_def->isStrict() == false || callable_def->getSlot(id->id()) >= 0) {
								uses_closure = true;
								break;
							}
						}
					}

					if(uses_closure == false) {
						for(auto slot_callable : slot_expr) {
							const FormulaCallableDefinition& def = slot_callable->getDefinition();

							//the basis is our symbol table's offset relative to the symbol table
							//in the scope of the symbol being resolved
							const int basis = def.querySubsetSlotBase(callable_def.get());
							if(basis == -1) {
								//our symbol table is unrelated to the symbol table of the symbol
								//getting looked up. As long as we are strict that means we
								//can be certain nothing is looking us up and we don't need the closure.
								if(!callable_def->isStrict()) {
									uses_closure = true;
									break;
								}
							} else {
								//look up the slot and see if it's within our symbol table.
								int numSlot = slot_callable->getSlot() - basis;
								if(numSlot >= 0 && numSlot < callable_def->getNumSlots()) {
									uses_closure = true;
									break;
								}
							}
						}
					}
				} //end of checking if we need the closure.

				if(g_strict_formula_checking) {
					std::ostringstream why;
					STRICT_ASSERT(!result_type || variant_types_compatible(result_type, fml->queryVariantType(), &why), "Formula function return type mis-match. Expects " << result_type->to_string() << " but expression evaluates to " << fml->queryVariantType()->to_string() << "\n" << pinpoint_location(formula_str, beg->begin, (i2-1)->end) << "\n" << why.str() << "\n\nSubexpressions:\n\n" << debugSubexpressionTypes(fml));
				}

				LambdaFunctionExpression* result = new LambdaFunctionExpression(args, fml, callable_def ? callable_def->getNumSlots() : 0, default_args, variant_types, result_type ? result_type : fml->queryVariantType());

				if(uses_closure == false) {
					//tell the expression that when we create the function we don't need to attach
					//a closure since it's not used.
					result->setNoClosure();
				}

				return ExpressionPtr(result);
			}

			const std::string precond = "";
			symbols->addFormulaFunction(formula_name, fml, Formula::createOptionalFormula(variant(precond), symbols), args, default_args, variant_types);
			return ExpressionPtr();
		}

		ExpressionPtr parse_expression_internal(const variant& formula_str, const Token* i1, const Token* i2, FunctionSymbolTable* symbols, ConstFormulaCallableDefinitionPtr callable_def, bool* can_optimize)
		{
			ASSERT_LOG(i1 != i2, "Empty expression in formula\n" << pinpoint_location(formula_str, (i1-1)->end));
	
			if(i1->type == FFL_TOKEN_TYPE::KEYWORD && i1+1 != i2 && i1+2 == i2 && i1->str() == "enum") {
				ASSERT_LOG((i1+1)->type == FFL_TOKEN_TYPE::IDENTIFIER, "Expected identifier after enum\n" << pinpoint_location(formula_str, i1->begin, i1->end));
				return ExpressionPtr(new VariantExpression(variant::create_enum((i1+1)->str())));
			}
			else if(symbols && i1->type == FFL_TOKEN_TYPE::KEYWORD && std::string(i1->begin, i1->end) == "def" &&
			   ((i1+1)->type == FFL_TOKEN_TYPE::IDENTIFIER || (i1+1)->type == FFL_TOKEN_TYPE::LPARENS ||
				(i1+1)->type == FFL_TOKEN_TYPE::LDUBANGLE)) {

				ExpressionPtr lambda = parse_function_def(formula_str, i1, i2, symbols, callable_def);
				if(lambda) {
					return lambda;
				}

				if((i1 == i2) || (i1 == (i2-1))) {
					//   Is this line unreachable?
					return ExpressionPtr(new FunctionListExpression(symbols));
				}
				else {
					return parse_expression(formula_str, (i1+1), i2, symbols, callable_def, can_optimize);
				}
			}
	
			int parens = 0;
			const Token* op = nullptr;
			const Token* fn_call = nullptr;

			for(const Token* i = i1; i != i2; ++i) {
				if(fn_call && i+1 == i2 && i->type != FFL_TOKEN_TYPE::RPARENS) {
					fn_call = nullptr;
				}
		
				if(i->type == FFL_TOKEN_TYPE::LPARENS || i->type == FFL_TOKEN_TYPE::LSQUARE || i->type == FFL_TOKEN_TYPE::LBRACKET) {
					if(i->type == FFL_TOKEN_TYPE::LPARENS && parens == 0 && i != i1) {
						fn_call = i;
					} else if(i->type == FFL_TOKEN_TYPE::LSQUARE && parens == 0 && i != i1 && (i-1)->type != FFL_TOKEN_TYPE::OPERATOR && (op == nullptr || operator_precedence(*op, formula_str) >= operator_precedence(*i, formula_str))) {
						//the square bracket itself is an operator
						op = i;
					}
			
					++parens;
				} else if(i->type == FFL_TOKEN_TYPE::RPARENS || i->type == FFL_TOKEN_TYPE::RSQUARE || i->type == FFL_TOKEN_TYPE::RBRACKET) {
					--parens;
			
					if(parens == 0 && i+1 != i2) {
						fn_call = nullptr;
					}
				} else if(parens == 0 && (i->type == FFL_TOKEN_TYPE::OPERATOR || i->type == FFL_TOKEN_TYPE::SEMICOLON || i->type == FFL_TOKEN_TYPE::LEFT_POINTER || (i->type == FFL_TOKEN_TYPE::LDUBANGLE && (i2-1)->type == FFL_TOKEN_TYPE::RDUBANGLE))) {
					if(op == nullptr || operator_precedence(*op, formula_str) >= operator_precedence(*i, formula_str)) {
						if(i != i1 && i->end - i->begin == 3 && std::equal(i->begin, i->end, "not")) {
							//The not operator is always unary and can only
							//appear at the start of an expression.
							continue;
						}

						if(op != nullptr && op->type == FFL_TOKEN_TYPE::SEMICOLON && op->type == i->type) {
							//semi-colons are left associative.
							continue;
						}

						op = i;
					}
				}
			}
	
			if(op != nullptr && (op->type == FFL_TOKEN_TYPE::LSQUARE)) {
				//the square bracket operator is handled below, just set the op
				//to nullptr and it'll be handled.
				op = nullptr;
			}
	
			if(op == nullptr) {
				if(i1->type == FFL_TOKEN_TYPE::LPARENS && (i2-1)->type == FFL_TOKEN_TYPE::RPARENS) {
					//   This condition will prevent ` ( def ( ) -> int 32993 ) ( ) `
					// from being incorrectly interpreted as that
					// ` def ( ) -> int 32993 ) ( ` must be parsed.
					if (i2 - 2 >= i1 && (i2 - 2)->type != FFL_TOKEN_TYPE::LPARENS) {
						return parse_expression(formula_str, i1+1,i2-1,symbols, callable_def, can_optimize);
					}
				} else if( (i2-1)->type == FFL_TOKEN_TYPE::RSQUARE) { //check if there is [ ] : either a list definition, or a operator 
					const Token* tok = i2-2;
					int square_parens = 0;
					while ( (tok->type != FFL_TOKEN_TYPE::LSQUARE || square_parens) && tok != i1) {
						if (tok->type == FFL_TOKEN_TYPE::RSQUARE) {
							square_parens++;
						} else if(tok->type == FFL_TOKEN_TYPE::LSQUARE) {
							square_parens--;
						}
						--tok;
					}	

					if (tok->type == FFL_TOKEN_TYPE::LSQUARE) {
						if (tok == i1) {
							const Token* pipe = i1+1;
							if(TokenMatcher().add(FFL_TOKEN_TYPE::PIPE).find_match(pipe, i2)) {
								//a list comprehension
								const Token* const begin_start_expr = i1+1;

								typedef std::pair<const Token*,const Token*> Arg;
								std::vector<Arg> args;
								const Token* arg = pipe+1;
								const Token* end_arg = arg;
								while(TokenMatcher().add(FFL_TOKEN_TYPE::COMMA).find_match(end_arg, i2-1)) {
									args.push_back(Arg(arg, end_arg));
									arg = ++end_arg;
								}
								args.push_back(Arg(arg, i2-1));

								std::map<std::string, ExpressionPtr> generators;
								std::vector<ExpressionPtr> filter_expr;

								std::vector<std::string> items;
								std::map<std::string, variant_type_ptr> item_types;

								ConstFormulaCallableDefinitionPtr def;

								bool seen_filter = false;

								for(const Arg& arg : args) {
									const Token* arrow = arg.first;
									if(TokenMatcher().add(FFL_TOKEN_TYPE::LEFT_POINTER).find_match(arrow, arg.second)) {
										ASSERT_LOG(arrow - arg.first == 1 && arg.first->type == FFL_TOKEN_TYPE::IDENTIFIER, "expected identifier to the left of <- in list comprehension\n" << pinpoint_location(formula_str, arg.first->begin, arrow->end));
										ASSERT_LOG(!seen_filter, "found <- after finding a filter in list comprehension\n" << pinpoint_location(formula_str, arg.first->begin, arrow->end));

										const std::string key(arg.first->begin, arg.first->end);
										ASSERT_LOG(generators.count(key) == 0, "repeated identifier in list generator: " << key << "\n" << pinpoint_location(formula_str, arg.first->begin, arrow->end));

										generators[key] = parse_expression(formula_str, arrow+1, arg.second, symbols, callable_def, can_optimize);
										items.push_back(key);
										variant_type_ptr gen_type = generators[key]->queryVariantType();

										if(gen_type) {
											gen_type = gen_type->is_list_of();
										}

										if(!gen_type) {
											gen_type = variant_type::get_any();
										}

										item_types[key] = gen_type;
									} else {
										if(!def) {
											ASSERT_LOG(items.empty() == false, "EMPTY ITEMS IN LIST COMPREHENSION: " << pinpoint_location(formula_str, arrow->begin, arrow->end));
											std::sort(items.begin(), items.end());
											std::vector<variant_type_ptr> types;
											for(const std::string& item : items) {
												types.push_back(item_types[item]);
											}
											def = execute_command_callable_definition(&items[0], &items[0] + items.size(), callable_def, &types[0]);
										}
										filter_expr.push_back(parse_expression(formula_str, arg.first, arg.second, symbols, def.get(), can_optimize));
										seen_filter = true;

										//if this filter condition passes, then we
										//know more about the possible objects that
										//can be produced by this list comprehension,
										//so modify the definition appropriately.
										ConstFormulaCallableDefinitionPtr new_def = filter_expr.back()->queryModifiedDefinitionBasedOnResult(true, def);
										if(new_def) {
											def = new_def;
										}
									}
								}

								if(!def) {
									ASSERT_LOG(items.empty() == false, "EMPTY ITEMS IN LIST COMPREHENSION: " << pinpoint_location(formula_str, pipe->begin, pipe->end));
									std::sort(items.begin(), items.end());
									std::vector<variant_type_ptr> types;
									for(const std::string& item : items) {
										types.push_back(item_types[item]);
									}
									def = execute_command_callable_definition(&items[0], &items[0] + items.size(), callable_def, &types[0]);
								}

								ExpressionPtr expr = parse_expression(formula_str, begin_start_expr, pipe, symbols, def.get(), can_optimize);

								return ExpressionPtr(new ListComprehensionExpression(expr, generators, filter_expr, callable_def ? callable_def->getNumSlots() : 0));
							} else {
								//create a list
								std::vector<ExpressionPtr> args;
								parse_args(formula_str,nullptr,i1+1,i2-1,&args,symbols, callable_def, can_optimize);
								return ExpressionPtr(new ListExpression(args));
							}
						} else {
							//determine if it's an array-style access of a single list element, or a slice.
							const Token* tok2 = i2-2;
							int bracket_parens_count = 0;
							const Token* colon_tok = nullptr;
							while (tok2 != tok){
								if (tok2->type == FFL_TOKEN_TYPE::RSQUARE || tok2->type == FFL_TOKEN_TYPE::RPARENS) {
									bracket_parens_count++;
								} else if (tok2->type == FFL_TOKEN_TYPE::LSQUARE || tok2->type == FFL_TOKEN_TYPE::LPARENS){
									bracket_parens_count--;
								} else if (tok2->type == FFL_TOKEN_TYPE::COLON){
									if(bracket_parens_count != 0){
											//TODO - handle error - mismatching brackets
											LOG_ERROR("mismatching brackets or parentheses inside [ ]: '" << std::string((i1+1)->begin, (i2-1)->end) << "'");
									} else if (colon_tok != nullptr){
											//TODO - handle error - more than one colon.
											LOG_ERROR("more than one colon inside a slice [:]: '" << std::string((i1+1)->begin, (i2-1)->end) << "'");
									} else {
										colon_tok = tok2;
									}
								}
								--tok2;	
							}
					
							if(colon_tok != nullptr){
								ExpressionPtr start, end;
								if(tok+1 < colon_tok) {
									start = parse_expression(formula_str, tok+1, colon_tok, symbols, callable_def, can_optimize);
								}

								if(colon_tok+1 < i2-1) {
									end = parse_expression(formula_str, colon_tok+1, i2-1, symbols, callable_def, can_optimize);
								}

								//it's a slice.  execute operator [ : ]
								return ExpressionPtr(new SliceSquareBracketExpression(
																					parse_expression(formula_str, i1,tok,symbols, callable_def, can_optimize), start, end));
							}else{	
								//execute operator [ ]
								return ExpressionPtr(new SquareBracketExpression(
																					parse_expression(formula_str, i1,tok,symbols, callable_def, can_optimize),
																					parse_expression(formula_str, tok+1,i2-1,symbols, callable_def, can_optimize)));
							}
						}
					}
				} else if(i1->type == FFL_TOKEN_TYPE::LBRACKET && (i2-1)->type == FFL_TOKEN_TYPE::RBRACKET) {
					//create a map TODO: add support for a set
					std::vector<ExpressionPtr> args;
					parse_set_args(formula_str,i1+1,i2-1,&args,symbols,callable_def);
					return ExpressionPtr(new MapExpression(args));
				} else if(i2 - i1 == 1) {
					if(i1->type == FFL_TOKEN_TYPE::KEYWORD) {
						if(std::string(i1->begin,i1->end) == "functions") {
							return ExpressionPtr(new FunctionListExpression(symbols));
						} else if(std::string(i1->begin,i1->end) == "null") {
							return ExpressionPtr(new VariantExpression(variant()));
						} else if(std::string(i1->begin,i1->end) == "true") {
							return ExpressionPtr(new VariantExpression(variant::from_bool(true)));
						} else if(std::string(i1->begin,i1->end) == "false") {
							return ExpressionPtr(new VariantExpression(variant::from_bool(false)));
						}
					} else if(i1->type == FFL_TOKEN_TYPE::CONST_IDENTIFIER) {
						return ExpressionPtr(new ConstIdentifierExpression(
																			  std::string(i1->begin,i1->end)));
					} else if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER) {
						if(can_optimize) {
						//	*can_optimize = false;
						}

						std::string symbol(i1->begin, i1->end);
						IdentifierExpression* expr =
							new IdentifierExpression(symbol, callable_def);
						const FormulaFunction* fn = symbols ? symbols->getFormulaFunction(symbol) : nullptr;
						if(fn != nullptr) {
							ExpressionPtr function(new LambdaFunctionExpression(fn->args(), fn->getFormula(), 0, fn->getDefaultArgs(), fn->variantTypes(), variant_type::get_any()));
							expr->set_function(function);
						}
						return ExpressionPtr(expr);
					} else if(i1->type == FFL_TOKEN_TYPE::INTEGER) {
						int n = strtol(std::string(i1->begin,i1->end).c_str(), nullptr, 0);
						return ExpressionPtr(new IntegerExpression(n));
					} else if(i1->type == FFL_TOKEN_TYPE::DECIMAL) {
						std::string decimal_string(i1->begin, i1->end);
						return ExpressionPtr(new decimal_expression(decimal::from_string(decimal_string)));
					} else if(i1->type == FFL_TOKEN_TYPE::STRING_LITERAL) {
						bool translate = *(i1->begin) == '~';
						int add = *(i1->begin) == 'q' ? 2 : 1;
						return ExpressionPtr(new StringExpression(std::string(i1->begin+add,i1->end-1), translate, symbols));
					}
				} else if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER &&
						  (i1+1)->type == FFL_TOKEN_TYPE::LPARENS &&
						  (i2-1)->type == FFL_TOKEN_TYPE::RPARENS) {
					int nleft = 0, nright = 0;
					for(const Token* i = i1; i != i2; ++i) {
						if(i->type == FFL_TOKEN_TYPE::LPARENS) {
							++nleft;
						} else if(i->type == FFL_TOKEN_TYPE::RPARENS) {
							++nright;
						}
					}
			
					if(nleft == nright) {
						const std::string function_name(i1->begin, i1->end);
						std::vector<ExpressionPtr> args;
						parse_args(formula_str,&function_name,i1+2,i2-1,&args,symbols, callable_def, can_optimize);
						ExpressionPtr result(createFunction(function_name, args, symbols, callable_def));
						if(result) {
							return result;
						}
					}
				}
		
				if(!fn_call) {
					if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && (i1+1)->type == FFL_TOKEN_TYPE::LPARENS) {
						const Token* match = i1+2;
						int depth = 0;
						while(match < i2) {
							if(match->type == FFL_TOKEN_TYPE::LPARENS) {
								++depth;
							} else if(match->type == FFL_TOKEN_TYPE::RPARENS) {
								if(depth == 0) {
									break;
								}
								--depth;
							}
							++match;
						}

						if(match != i2) {
							++match;
							ASSERT_LT(match, i2); 

							ASSERT_LOG(false, "unexpected tokens after function call\n" << pinpoint_location(formula_str, match->begin, (i2-1)->end));
						} else {
							ASSERT_LOG(false, "no closing parenthesis to function call\n" << pinpoint_location(formula_str, i1->begin, (i2-1)->end));
						}
					} else {
						ASSERT_LOG(false, "could not parse expression\n" << pinpoint_location(formula_str, i1->begin, (i2-1)->end));
					}

					assert(false); //should never reach here.
				}
			}
	
			if(fn_call && (op == nullptr ||
			   operator_precedence(*op, formula_str) >= operator_precedence(*fn_call, formula_str))) {
				op = fn_call;
			}

			if(op->type == FFL_TOKEN_TYPE::SEMICOLON) {

				if(i1->type == FFL_TOKEN_TYPE::KEYWORD && std::string(i1->begin, i1->end) == "let") {

					++i1;
					ASSERT_LOG(i1 < op && i1+1 < op, "Expected tokens after let before ;\n" << pinpoint_location(formula_str, op->begin, op->end));

					variant_type_ptr type;

					if(i1->type != FFL_TOKEN_TYPE::IDENTIFIER || std::string((i1+1)->begin, (i1+1)->end) != "=") {
						type = parse_variant_type(formula_str, i1, op);
					}

					ASSERT_LOG(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && std::string((i1+1)->begin, (i1+1)->end) == "=", "Expected identifier and assignment after let\n" << pinpoint_location(formula_str, i1->begin, i1->end));

					std::string identifier(i1->begin, i1->end);

					i1 += 2;
					ExpressionPtr let_expr(parse_expression(formula_str, i1, op, symbols, callable_def, can_optimize));

					variant_type_ptr expr_type = let_expr->queryVariantType();
					if(!type) {
						type = expr_type;
					}

					ASSERT_LOG(variant_types_compatible(type, expr_type), "Cannot convert " << expr_type->to_string() << " to variable type " << type->to_string() << "\n" << pinpoint_location(formula_str, i1->begin, i1->end));


					const int new_slot = callable_def->getNumSlots();

					FormulaCallableDefinitionPtr new_def = execute_command_callable_definition(&identifier, &identifier+1, callable_def, &type);
					new_def->setStrict(callable_def && callable_def->isStrict());

					ExpressionPtr right(parse_expression(formula_str, op+1,i2,symbols, new_def, can_optimize));

					return ExpressionPtr(new LetExpression(identifier, new_slot, let_expr, right));

				} else {

					ExpressionPtr left;
					
					std::vector<ExpressionPtr> list;
					if(i1 != op) {
						left = ExpressionPtr(parse_expression(formula_str, i1, op, symbols, callable_def, can_optimize));
						list.push_back(left);
					}
					ExpressionPtr right(parse_expression(formula_str, op+1,i2,symbols, callable_def, can_optimize));
						list.push_back(right);
					return ExpressionPtr(new CommandSequenceExpression(left, right));
				}
			}
	
			if(op == i1) {
				if(op+1 == i2) {
					LOG_WARN("No expression for operator '" << std::string(op->begin,op->end) << "' to operate on");
				}
				return ExpressionPtr(new UnaryOperatorExpression(
																	std::string(op->begin,op->end),
																	parse_expression(formula_str, op+1,i2,symbols, callable_def, can_optimize)));
			}

			if(op->type == FFL_TOKEN_TYPE::LDUBANGLE) {
				ASSERT_LOG((i2-1)->type == FFL_TOKEN_TYPE::RDUBANGLE, "Could not find matching closing >>\n" << pinpoint_location(formula_str, op->begin, op->end));
				ASSERT_LOG(i1 != op, "Could not find expression to apply << >> to\n" << pinpoint_location(formula_str, op->begin, op->end));

				ExpressionPtr left = parse_expression(formula_str, i1, op, symbols, callable_def, can_optimize);
				return ExpressionPtr(new InstantiateGenericExpression(formula_str, left, op+1, i2-1));
			}
	
			int consume_backwards = 0;
			std::string op_name(op->begin,op->end);

			if (op_name == "is" && op + 1 > i1 && op + 1 < i2 &&
					std::string((op + 1)->begin, (op + 1)->end) == "not") {
				op_name = "is not";
			}

			if(op_name == "in" && op > i1 && op-1 > i1 && std::string((op-1)->begin, (op-1)->end) == "not") {
				op_name = "not in";
				consume_backwards = 1;
			}

			if(op_name == "<-" || op_name == "::") {
				variant_type_ptr type = parse_variant_type(formula_str, i1, op);
				ASSERT_LOG(type && i1 == op, "UNEXPECTED TOKENS WHEN PARSING TYPE: " << pinpoint_location(formula_str, i1->begin, op->end));

				ExpressionPtr right(parse_expression(formula_str, op+1,i2,symbols, callable_def, can_optimize));

				if(op_name == "<-") {
					return ExpressionPtr(new TypeExpression(type, right));
				} else {
					return ExpressionPtr(new StaticTypeExpression(type, right));
				}
			}

			if (op_name == "is not") {
				const Token* type_tok = op + 2;
				variant_type_ptr type = parse_variant_type(
						formula_str, type_tok, i2);
				ASSERT_LOG(type_tok == i2, "Unexpected tokens after type: " <<  pinpoint_location(formula_str, type_tok->begin, (i2-1)->end));

				ExpressionPtr left(parse_expression(
						formula_str, i1, op, symbols,
						callable_def, can_optimize));
				return ExpressionPtr(new IsExpression(
						type, left, true));
			}

			if(op_name == "is") {
				const Token* type_tok = op+1;
				variant_type_ptr type = parse_variant_type(formula_str, type_tok, i2);
				ASSERT_LOG(type_tok == i2, "Unexpected tokens after type: " << pinpoint_location(formula_str, type_tok->begin, (i2-1)->end));

				ExpressionPtr left(parse_expression(formula_str, i1, op, symbols, callable_def, can_optimize));
				return ExpressionPtr(new IsExpression(type, left));
			}
	
			if(op_name == "(") {
				if(i2 - op < 2) {
					ASSERT_LOG(false, "MISSING PARENS IN FORMULA\n" << pinpoint_location(formula_str, op->begin, op->end));
				}

				std::vector<ExpressionPtr> args;
				parse_args(formula_str,nullptr,op+1, i2-1, &args, symbols, callable_def, can_optimize);
		
				return ExpressionPtr(new FunctionCallExpression(parse_expression(formula_str, i1, op, symbols, callable_def, can_optimize), args));
			}
	
			if(op_name == ".") {
				ExpressionPtr left(parse_expression(formula_str, i1,op,symbols, callable_def, can_optimize));
				ConstFormulaCallableDefinitionPtr type_definition = left->getTypeDefinition();
				ExpressionPtr right(parse_expression(formula_str, op+1,i2,nullptr, type_definition, can_optimize));
				return ExpressionPtr(new DotExpression(left, right, type_definition));
			}
	
			if(op_name == "where") {
				const int base_slots = callable_def ? callable_def->getNumSlots() : 0;
				WhereVariablesInfoPtr where_info(new WhereVariablesInfo(base_slots));

				expr_table_ptr table(new expr_table());
				parse_where_clauses(formula_str, op+1, i2, table, symbols, callable_def);
				std::vector<ExpressionPtr> entries;
				for(expr_table::iterator i = table->begin(); i != table->end(); ++i) {
					where_info->names.push_back(i->first);
					where_info->entries.push_back(i->second);
				}

				where_info->callable_where_def = create_where_definition(table, callable_def);
				return ExpressionPtr(new WhereExpression(parse_expression(formula_str, i1, op, symbols, where_info->callable_where_def.get(), can_optimize), where_info));
			} else if(op_name == "asserting") {
				ExpressionPtr debug_expr;

				const Token* pipe = op+1;
				if(TokenMatcher().add(FFL_TOKEN_TYPE::PIPE).find_match(pipe, i2)) {
					debug_expr = parse_expression(formula_str, pipe+1, i2, symbols, callable_def, can_optimize);
					i2 = pipe;
				}

				std::vector<ExpressionPtr> asserts;
				parse_args(formula_str,nullptr,op+1,i2,&asserts,symbols, callable_def, can_optimize);

				ConstFormulaCallableDefinitionPtr def_after_asserts = callable_def;
				for(ExpressionPtr expr : asserts) {
					ConstFormulaCallableDefinitionPtr new_def = expr->queryModifiedDefinitionBasedOnResult(true, def_after_asserts);
					if(new_def) {
						def_after_asserts = new_def;
					}
				}

				ExpressionPtr base_expr(parse_expression(formula_str, i1, op, symbols, def_after_asserts, can_optimize));

				return ExpressionPtr(new AssertExpression(base_expr, asserts, debug_expr));
			}

			ExpressionPtr left_expr = parse_expression(formula_str, i1, op-consume_backwards, symbols, callable_def, can_optimize);

			//In an 'and' or 'or', if we get to the right branch we can possibly
			//infer more information about the types of symbols. Do that here.
			ConstFormulaCallableDefinitionPtr right_callable_def = callable_def;
			if(op_name == "and") {
				ConstFormulaCallableDefinitionPtr new_def = left_expr->queryModifiedDefinitionBasedOnResult(true, callable_def);
				if(new_def) {
					right_callable_def = new_def;
				}
			} else if(op_name == "or") {
				ConstFormulaCallableDefinitionPtr new_def = left_expr->queryModifiedDefinitionBasedOnResult(false, callable_def);
				if(new_def) {
					right_callable_def = new_def;
				}
			}

			ExpressionPtr right_expr = parse_expression(formula_str, op+1,i2,symbols, right_callable_def, can_optimize);

			return ExpressionPtr(new OperatorExpression(op_name, left_expr, right_expr));
		}
	}

void Formula::failIfStaticContext()
{
	if(in_static_context) {
		throw non_static_expression_exception();
	}
}

Formula::StrictCheckScope::StrictCheckScope(bool is_strict, bool is_warnings)
  : old_value(g_strict_formula_checking), old_warning_value(g_strict_formula_checking_warnings)
{
	g_strict_formula_checking = is_strict;
	g_strict_formula_checking_warnings = is_warnings;
}

Formula::StrictCheckScope::~StrictCheckScope()
{
	g_strict_formula_checking = old_value;
	g_strict_formula_checking_warnings = old_warning_value;
}

FormulaPtr Formula::createOptionalFormula(const variant& val, FunctionSymbolTable* symbols, ConstFormulaCallableDefinitionPtr callableDefinition, FORMULA_LANGUAGE lang)
{
	if(val.is_null() || (val.is_string() && val.as_string().empty())) {
		return FormulaPtr();
	}
	
	if(lang == FORMULA_LANGUAGE::FFL) {
		return FormulaPtr(new Formula(val, symbols, callableDefinition));
	} else {
		assert(false);
		return FormulaPtr();
		//return FormulaPtr(new Formula(val, FORMULA_LANGUAGE::LUA));
	}
}

PREF_BOOL(ffl_vm, true, "Use VM for FFL optimization");

Formula::Formula()
{}

Formula::Formula(const variant& val, FunctionSymbolTable* symbols, ConstFormulaCallableDefinitionPtr callableDefinition)
	: str_(val), 
	def_(callableDefinition)
{
	using namespace formula_tokenizer;

	FunctionSymbolTable symbol_table;
	if(!symbols) {
		symbols = &symbol_table;
	}

	if(str_.is_callable()) {
#if defined(USE_LUA)
		lua::LuaFunctionReference* fn_ref = val.try_convert<lua::LuaFunctionReference>();
		ASSERT_LOG(fn_ref != nullptr, "FATAL: Couldn't convert function reference to the correct type.");
		expr_.reset(new LuaFnExpression(fn_ref));
#endif
		return;
	}

	if(str_.is_int() || str_.is_bool() || str_.is_decimal()) {
		//Allow ints, bools, and decimals to be interpreted as formulae.
		str_ = variant(str_.string_cast());
	}

	std::vector<Token> tokens;
	std::string::const_iterator i1 = str_.as_string().begin(), i2 = str_.as_string().end();
	while(i1 != i2) {
		try {
			tokens.push_back(get_token(i1,i2));
			if((tokens.back().type == FFL_TOKEN_TYPE::WHITESPACE) || (tokens.back().type == FFL_TOKEN_TYPE::COMMENT)) {
				tokens.pop_back();
			}
		} catch(TokenError& e) {
			ASSERT_LOG(false, "Token error: " << e.msg << ": " << pinpoint_location(str_, i1, i1));
		}
	}

	checkBracketsMatch(tokens);

	if(tokens.size() != 0) {
		ConstFormulaCallableDefinitionPtr global_where_def;

		const Token* tok = &tokens[0];
		const Token* end_tokens = &tokens[0] + tokens.size();

		if(tokens[0].type == FFL_TOKEN_TYPE::KEYWORD && std::string(tokens[0].begin, tokens[0].end) == "base") {

			const Token* recursive_case = tok;
			if(!TokenMatcher(FFL_TOKEN_TYPE::KEYWORD).add("recursive").find_match(recursive_case, end_tokens)) {
				ASSERT_LOG(false, "ERROR WHILE PARSING FORMULA: NO RECURSIVE CASE FOUND");
			}


			const Token* where_tok = recursive_case;

			if(TokenMatcher(FFL_TOKEN_TYPE::OPERATOR).add("where").find_match(where_tok, end_tokens)) {
				global_where_.reset(new WhereVariablesInfo(callableDefinition ? callableDefinition->getNumSlots() : 0));
				expr_table_ptr table(new expr_table());
				parse_where_clauses(str_, where_tok+1, end_tokens, table, symbols, callableDefinition);
				for(expr_table::iterator i = table->begin(); i != table->end(); ++i) {
					global_where_->names.push_back(i->first);
					global_where_->entries.push_back(i->second);
				}

				global_where_def = create_where_definition(table, callableDefinition);
				callableDefinition = global_where_def.get();

				end_tokens = where_tok;
			}

			while(tok->type == FFL_TOKEN_TYPE::KEYWORD && std::string(tok->begin, tok->end) == "base") {
				++tok;

				const Token* colon_ptr = tok;

				if(!TokenMatcher(FFL_TOKEN_TYPE::COLON).find_match(colon_ptr, end_tokens)) {
					ASSERT_LOG(false, "ERROR WHILE PARSING FORMULA: ':' EXPECTED AFTER BASE");
				}

				const Token* end_ptr = colon_ptr;

				if(!TokenMatcher(FFL_TOKEN_TYPE::KEYWORD).add("base").add("recursive").find_match(end_ptr, end_tokens)) {
					ASSERT_LOG(false, "ERROR WHILE PARSING FORMULA: NO RECURSIVE CASE FOUND");
				}

				BaseCase base;
				base.raw_guard = base.guard = parse_expression(str_, tok, colon_ptr, symbols, callableDefinition);
				base.expr = parse_expression(str_, colon_ptr+1, end_ptr, symbols, callableDefinition);

				base_expr_.push_back(base);

				tok = end_ptr;
			}

			//check that the part before the actual formula is recursive:
			ASSERT_LOG(tok + 2 < end_tokens && tok->type == FFL_TOKEN_TYPE::KEYWORD && std::string(tok->begin, tok->end) == "recursive" && (tok+1)->type == FFL_TOKEN_TYPE::COLON, "RECURSIVE CASE NOT FOUND");

			tok += 2;

		}

		expr_ = parse_expression(str_, tok, end_tokens, symbols, callableDefinition);

		if(global_where_) {
			expr_.reset(new WhereExpression(expr_, global_where_));
			for(BaseCase& base : base_expr_) {
				base.guard.reset(new WhereExpression(base.guard, global_where_));
				base.expr.reset(new WhereExpression(base.expr, global_where_));
			}
		}
	} else {
		expr_ = ExpressionPtr(new VariantExpression(variant()));
	}

	str_.add_formula_using_this(this);

#ifndef NO_EDITOR
	all_formulae().insert(this);
#endif

	if(g_ffl_vm) {
		int before = expr_->refcount();
		//VMizing can lose type information so save it here.
		type_ = expr_->queryVariantType();
		int before2 = expr_->refcount();

		static size_t total_before = 0, total_after = 0;

		const size_t before_children = expr_->queryChildrenRecursive().size();

		ExpressionPtr vm_expr = expr_->optimizeToVM();
		if(vm_expr) {
			type_->set_expr(vm_expr.get());
			expr_ = vm_expr;
		}
	}
}

ConstFormulaCallablePtr Formula::wrapCallableWithGlobalWhere(const FormulaCallable& callable) const
{
	if(global_where_) {
		ConstFormulaCallablePtr wrapped_variables(new WhereVariables(callable, global_where_));
		return wrapped_variables;
	} else {
		return ConstFormulaCallablePtr(&callable);
	}
}

variant_type_ptr Formula::queryVariantType() const
{
	if(type_) {
		return type_;
	}

	return expr_->queryVariantType();
}

void Formula::checkBracketsMatch(const std::vector<Token>& tokens) const
{
	std::string error_msg;
	int error_loc = -1;

	std::stack<formula_tokenizer::FFL_TOKEN_TYPE> brackets;
	std::stack<int> brackets_locs;
	for(int n = 0; n != tokens.size(); ++n) {
		switch(tokens[n].type) {
		case FFL_TOKEN_TYPE::LPARENS:
		case FFL_TOKEN_TYPE::LSQUARE:
		case FFL_TOKEN_TYPE::LBRACKET:
			brackets.push(tokens[n].type);
			brackets_locs.push(n);
			break;
		case FFL_TOKEN_TYPE::RPARENS:
		case FFL_TOKEN_TYPE::RSQUARE:
		case FFL_TOKEN_TYPE::RBRACKET:
			if(brackets.empty()) {
				error_msg = "UNEXPECTED TOKEN: " + std::string(tokens[n].begin, tokens[n].end);
				error_loc = n;
				break;
			} else if(brackets.top() != tokens[n].type-1) {
				const int m = brackets_locs.top();
				error_msg = "UNMATCHED BRACKET: " + std::string(tokens[m].begin, tokens[m].end);
				error_loc = m;
				break;
			}

			brackets.pop();
			brackets_locs.pop();
			break;
		default:
			break;
		}
	}

	if(error_msg.empty() && brackets.empty() == false) {
		const int m = brackets_locs.top();
		error_msg = "UNMATCHED BRACKET: " + std::string(tokens[m].begin, tokens[m].end);
		error_loc = m;
	}

	if(error_loc != -1) {
		const Token& tok = tokens[error_loc];
		std::string::const_iterator begin_line = tokens.front().begin;
		std::string::const_iterator i = begin_line;
		int nline = 0;
		while(i < tok.begin) {
			if(i == tok.begin) {
				break;
			}

			if(*i == '\n') {
				++nline;
				begin_line = i+1;
			}
			++i;
		}

		const std::string::const_iterator end_line = std::find(begin_line, tokens.back().end, '\n');
		while(begin_line < end_line && util::c_isspace(*begin_line)) {
			++begin_line;
		}

		std::string whitespace(begin_line, tok.begin);
		std::fill(whitespace.begin(), whitespace.end(), ' ');
		std::string error_line(begin_line, end_line);

		if(whitespace.size() > 60) {
			const auto erase_size = whitespace.size() - 60;
			whitespace.erase(whitespace.begin(), whitespace.begin() + erase_size);
			ASSERT_LOG(erase_size <= error_line.size(), "ERROR WHILE PARSING ERROR MESSAGE: " << erase_size << " <= " << error_line.size() << " IN " << error_line);
			error_line.erase(error_line.begin(), error_line.begin() + erase_size);
			std::fill(error_line.begin(), error_line.begin() + 3, '.');
		}

		if(error_line.size() > 78) {
			error_line.resize(78);
			std::fill(error_line.end()-3, error_line.end(), '.');
		}
		

		std::string location;
		const variant::debug_info* dbg_info = str_.get_debug_info();
		if(dbg_info) {
			location = formatter() << " AT " << *dbg_info->filename
		                           << " " << dbg_info->line;
		}
		//TODO: extract info from str_ about the location of the formula.
		ASSERT_LOG(false, "ERROR WHILE PARSING FORMULA" << location << ": "
		  << error_msg << "\n"
		  << error_line << "\n"
		  << whitespace << "^\n");
	}
}


Formula::~Formula() {
	if(last_executed_formula == this) {
		last_executed_formula = nullptr;
	}

	str_.remove_formula_using_this(this);
#ifndef NO_EDITOR
	all_formulae().erase(this);
#endif
}

std::string Formula::outputDebugInfo() const
{
	std::ostringstream s;
	s << "FORMULA: " << (str_.get_debug_info() ? str_.get_debug_info()->message() : "(UNKNOWN LOCATION): ");
	//TODO: add debug info from str_ variant here.
	
	s << str_.as_string() << "\n";
	return s.str();
}

bool Formula::outputDisassemble(std::string* result) const
{
	const VMExpression* ex = dynamic_cast<const VMExpression*>(expr().get());
	if(ex != nullptr) {
		if(result) {
			*result = ex->debugOutput();
		}
		return true;
	}

	return false;
}

int Formula::guardMatches(const FormulaCallable& variables) const
{
	if(base_expr_.empty() == false) {
		int index = 0;
		for(const BaseCase& b : base_expr_) {
			if(b.guard->evaluate(variables).as_bool()) {
				return index;
			}

			++index;
		}
	}

	return -1;
}

int Formula::rawGuardMatches(const FormulaCallable& variables) const
{
	if(base_expr_.empty() == false) {
		int index = 0;
		for(const BaseCase& b : base_expr_) {
			if(b.raw_guard->evaluate(variables).as_bool()) {
				return index;
			}

			++index;
		}
	}

	return -1;
}

Formula::NonStaticContext::NonStaticContext() { old_value_ = in_static_context; in_static_context = 0; }
Formula::NonStaticContext::~NonStaticContext() { in_static_context = old_value_; }

variant Formula::execute(const FormulaCallable& variables) const
{
	//We want to track the 'last executed' formula in last_executed_formula,
	//so we can use it for debugging purposes if there's a problem.
	//If one formula calls another, we want to restore the old value after
	//the nested formula exits. However, when a formula returns, if it's
	//the top-level formula we want to still keep it recorded as the
	//last executed, so we can complain about it if any commands it returns
	//have problems.
	//
	//As such we track the depth of the execution stack so we can tell if
	//we're a top-level formula or not. If we're a nested formula we restore
	//last_executed_formula upon return.
	//
	//Naturally if we throw an exception we DON'T want to restore the
	//last_executed_formula since we want to report the error.
	static int execution_stack = 0;
	const Formula* prev_executed = execution_stack ? last_executed_formula : nullptr;
	last_executed_formula = this;
	try {
		++execution_stack;

		const int nguard = guardMatches(variables);

		variant result = (nguard == -1 ? expr_ : base_expr_[nguard].expr)->evaluate(variables);
		--execution_stack;
		if(prev_executed) {
			last_executed_formula = prev_executed;
		}
		return result;
	} catch(std::string&) {
	}

	ASSERT_LOG(false, "");
}

variant Formula::execute() const
{
	last_executed_formula = this;
	
	MapFormulaCallable* null_callable = new MapFormulaCallable;
	variant ref(null_callable);
	return execute(*null_callable);
}

bool Formula::evaluatesToConstant(variant& result) const
{
	return expr_->canReduceToVariant(result);
}

ExpressionPtr VariantExpression::optimizeToVM()
{
	formula_vm::VirtualMachine vm;
	vm.addLoadConstantInstruction(v_);
	VMExpression* result = new VMExpression(vm, queryVariantType(), *this);
	result->setVariant(v_);
	return ExpressionPtr(result);
}

ExpressionPtr createVMExpression(formula_vm::VirtualMachine vm, variant_type_ptr t, const FormulaExpression& o)
{
	return ExpressionPtr(new VMExpression(vm, t, o));
}

UNIT_TEST(where_statement) {
	if(g_ffl_vm) {
		Formula* f = new Formula(variant("a * b + c where a = 2d8 where b = 1d4 where c = 2d6"));

		std::string assembly;
		bool result = f->outputDisassemble(&assembly);
		CHECK(result, "Could not disassemble");
		delete f;

//		fprintf(stderr, "ZZZ: ASM: %s\n", assembly.c_str());

		f = new Formula(variant("a * b + c where a = 2d8, b = 1d4, c = 2d6"));

		assembly = "";

		result = f->outputDisassemble(&assembly);

//		fprintf(stderr, "ZZZ: ASM2: %s\n", assembly.c_str());

		f = new Formula(variant("a * b + c where a = 2d8, b = 1d4 where c = 2d6"));

		assembly = "";

		result = f->outputDisassemble(&assembly);

//		fprintf(stderr, "ZZZ: ASM3: %s\n", assembly.c_str());

	}
}

UNIT_TEST(recursive_call_lambda) {
	CHECK(Formula(variant("def fact_tail(n,a,b) factt(n,1) where factt = def(m,x) if(m > 0, x + m + recurse(m-1,x*m),x); fact_tail(5,0,0)")).execute() != variant(), "test failed");
}

UNIT_TEST(formula_slice) {
	CHECK(Formula(variant("myList[2:4] where myList = [1,2,3,4,5,6]")).execute() == Formula(variant("[3,4]")).execute(), "test failed");
	CHECK(Formula(variant("myList[0:2] where myList = [1,2,3,4,5,6]")).execute() == Formula(variant("[1,2]")).execute(), "test failed");
	CHECK(Formula(variant("myList[1:4] where myList = [0,2,4,6,8,10,12,14]")).execute() == Formula(variant("[2,4,6]")).execute(), "test failed");
}
	
	
UNIT_TEST(formula_in) {
	CHECK(Formula(variant("1 in [4,5,6]")).execute() == variant::from_bool(false), "test failed");
	CHECK(Formula(variant("5 in [4,5,6]")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("5 not in [4,5,6]")).execute() == variant::from_bool(false), "test failed");
	CHECK(Formula(variant("8 not in [4,5,6]")).execute() == variant::from_bool(true), "test failed");
}

//   'is [not] null'.
UNIT_TEST(formula_is) {
	CHECK(Formula(variant("a is null where a = null")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("a is int where a = null")).execute() == variant::from_bool(false), "test failed");
	CHECK(Formula(variant("a is list where a = null")).execute() == variant::from_bool(false), "test failed");
	CHECK(Formula(variant("a is null where a = 0")).execute() == variant::from_bool(false), "test failed");
	CHECK(Formula(variant("a is int where a = 0")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("a is list where a = 0")).execute() == variant::from_bool(false), "test failed");
	CHECK(Formula(variant("a is null where a = [0]")).execute() == variant::from_bool(false), "test failed");
	CHECK(Formula(variant("a is int where a = [0]")).execute() == variant::from_bool(false), "test failed");
	CHECK(Formula(variant("a is list where a = [0]")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("not a is null where a = null")).execute() == variant::from_bool(false), "test failed");
	CHECK(Formula(variant("not a is int where a = null")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("not a is list where a = null")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("not a is null where a = 0")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("not a is int where a = 0")).execute() == variant::from_bool(false), "test failed");
	CHECK(Formula(variant("not a is list where a = 0")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("not a is null where a = [0]")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("not a is int where a = [0]")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("not a is list where a = [0]")).execute() == variant::from_bool(false), "test failed");
	CHECK(Formula(variant("a is not null where a = null")).execute() == variant::from_bool(false), "test failed");
	CHECK(Formula(variant("a is not int where a = null")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("a is not list where a = null")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("a is not null where a = 0")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("a is not int where a = 0")).execute() == variant::from_bool(false), "test failed");
	CHECK(Formula(variant("a is not list where a = 0")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("a is not null where a = [0]")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("a is not int where a = [0]")).execute() == variant::from_bool(true), "test failed");
	CHECK(Formula(variant("a is not list where a = [0]")).execute() == variant::from_bool(false), "test failed");
}

UNIT_TEST(formula_fn) {
	FunctionSymbolTable symbols;
	CHECK(Formula(variant("def f(g) g(5) + 1; def fn(n) n*n; f(fn)"), &symbols).execute() == variant(26), "test failed");
}

UNIT_TEST(array_index) {
	Formula f(variant("map(range(6), elements[value]) = elements "
			          "where elements = [5, 6, 7, 8, 9, 10]"));
	CHECK(f.execute() == variant::from_bool(true), "test failed");
}

UNIT_TEST(dot_precedence) {
	MapFormulaCallable* callable = new MapFormulaCallable;
	variant ref(callable);
	MapFormulaCallable* callable2 = new MapFormulaCallable;
	std::vector<variant> v;
	for(int n = 0; n != 10; ++n) {
		MapFormulaCallable* obj = new MapFormulaCallable;
		obj->add("value", variant(n));
		v.push_back(variant(obj));
	}
	callable2->add("item", variant(&v));
	callable->add("obj", variant(callable2));
	Formula f(variant("obj.item[n].value where n = 2"));
	const variant result = f.execute(*callable);
	CHECK(result == variant(2), "test failed: " << result.to_debug_string());
}

UNIT_TEST(short_circuit) {
	MapFormulaCallable* callable = new MapFormulaCallable;
	variant ref(callable);
	callable->add("x", variant(0));
	Formula f(variant("x and (5/x)"));
	f.execute(*callable);
}

UNIT_TEST(formula_decimal) {
	CHECK_EQ(Formula(variant("0.0005")).execute().string_cast(), "0.0005");
    CHECK_EQ(Formula(variant("0.005")).execute().string_cast(), "0.005");
	CHECK_EQ(Formula(variant("0.05")).execute().string_cast(), "0.05");
	CHECK_EQ(Formula(variant("0.5")).execute().string_cast(), "0.5");
	CHECK_EQ(Formula(variant("8.5 + 0.5")).execute().string_cast(), "9.0");
	CHECK_EQ(Formula(variant("4 * (-1.1)")).execute().string_cast(), "-4.4");
	//   In case of implicit zero valued integer part.
	CHECK_EQ(Formula(variant(".032993")).execute().string_cast(), "0.032993");
}

UNIT_TEST(formula_quotes) {
	CHECK_EQ(Formula(variant("q((4+2())) + q^a^")).execute().string_cast(), "(4+2())a");
}

UNIT_TEST(map_to_maps_FAILS) {
	CHECK_EQ(Formula(variant("{'a' -> ({'b' -> 2})}")).execute().string_cast(), Formula(variant("{'a' -> {'b' -> 2}}")).execute().string_cast());
}

UNIT_TEST(map_to_maps_1) {
	CHECK_EQ(Formula(variant("{'a': ({'b': 2})}")).execute().string_cast(),
			Formula(variant("{'a': {'b': 2}}")).execute().string_cast());
}

UNIT_TEST(formula_test_recursion) {
	FunctionSymbolTable symbols;
	Formula f(variant("def silly_add(a, c)"
					  "base b <= 0: a "
					  "recursive: silly_add(a+1, b-1) where b = c;"
					  "silly_add(50, 5000)"), &symbols);

	CHECK_EQ(f.execute().as_int(), 5050);
}

UNIT_TEST(formula_test_recurse_sort) {
	Formula f(variant("def my_qsort(items) "
					  "base size(items) <= 1: items "
					  "recursive: my_qsort(filter(items, i, i < items[0])) +"
					  "           filter(items, i, i = items[0]) +"
					  "           my_qsort(filter(items, i, i > items[0]));"
					 "my_qsort([4,10,2,9,1])"));
	CHECK_EQ(f.execute(), Formula(variant("[1,2,4,9,10]")).execute());
}

UNIT_TEST(formula_where_map) {
	CHECK_EQ(Formula(variant("{'a': a} where a = 4")).execute()["a"], variant(4));
}

UNIT_TEST(formula_function_default_args) {
	CHECK_EQ(Formula(variant("def f(x=5) x ; f() + f(1)")).execute(), variant(6));
	CHECK_EQ(Formula(variant("f(5) where f = def(x,y=2) x*y")).execute(), variant(10));
}

UNIT_TEST(formula_typeof) {
#define TYPEOF_TEST(a, b) CHECK_EQ(Formula(variant(a)).execute(), variant(b))
	TYPEOF_TEST("static_typeof(def(int n) n+5)", "function(int) -> int");
	TYPEOF_TEST("static_typeof(def(int n) n+5.0)", "function(int) -> decimal");
	TYPEOF_TEST("static_typeof(def([int] mylist) map(mylist, value+5.0))", "function([int]) -> [decimal]");
	TYPEOF_TEST("static_typeof(choose([1,2,3]))", "int");
	TYPEOF_TEST("static_typeof(choose([1,2,'abc',4.5]))", "string|decimal"); //int is compatible with decimal so gets subsumed by it.
	TYPEOF_TEST("static_typeof(if(1d6 = 5, 5))", "int|null");
	TYPEOF_TEST("static_typeof(if(1d6 = 2, 5, 8))", "int");
	TYPEOF_TEST("static_typeof(if(1d6 = 2, 'abc', 2))", "string|int");
	TYPEOF_TEST("static_typeof(def(obj dummy_gui_object c, [obj dummy_gui_object] s) -> [obj dummy_gui_object]	\
			 if (c.parent and (c.parent is obj dummy_gui_object) and (c.parent not in s), 	\
				recurse(c.parent, s + [c.parent]), 				\
				s 								\
			))", "function(obj dummy_gui_object,[obj dummy_gui_object]) -> [obj dummy_gui_object]");
#undef TYPEOF_TEST
}

UNIT_TEST(formula_types_compatible) {
	CHECK_EQ(Formula(variant("types_compatible('any', '[int,int]')")).execute().as_bool(), true);
	CHECK_EQ(Formula(variant("types_compatible('string|int', 'string')")).execute().as_bool(), true);
	CHECK_EQ(Formula(variant("types_compatible('string', 'string|int')")).execute().as_bool(), false);
	CHECK_EQ(Formula(variant("types_compatible('int|string', 'string|int')")).execute().as_bool(), true);
	CHECK_EQ(Formula(variant("types_compatible('[int]', '[int,int]')")).execute().as_bool(), true);
	CHECK_EQ(Formula(variant("types_compatible('[int,int]', '[int]')")).execute().as_bool(), false);
}

UNIT_TEST(formula_function_types_compatible) {
	CHECK_EQ(Formula(variant("types_compatible('function(string) ->int', 'function(string) ->any')")).execute().as_bool(), false);
	CHECK_EQ(Formula(variant("types_compatible('function(string) ->any', 'function(string) ->int')")).execute().as_bool(), true);
	CHECK_EQ(Formula(variant("types_compatible('function(string) ->int', 'function(any) ->int')")).execute().as_bool(), true);
	CHECK_EQ(Formula(variant("types_compatible('function(any) ->int', 'function(string) ->int')")).execute().as_bool(), false);
	CHECK_EQ(Formula(variant("types_compatible('function(string) ->int', 'function(any) ->any')")).execute().as_bool(), false);
	CHECK_EQ(Formula(variant("types_compatible('function(any) ->any', 'function(string) ->int')")).execute().as_bool(), false);
	CHECK_EQ(Formula(variant("types_compatible('function(any) ->int', 'function(string) ->any')")).execute().as_bool(), false);
	CHECK_EQ(Formula(variant("types_compatible('function(string) ->any', 'function(any) ->int')")).execute().as_bool(), true);
}

UNIT_TEST(formula_map_types_compatible) {
	CHECK_EQ(Formula(variant("types_compatible('{string -> int}', '{string -> any}')")).execute().as_bool(), false);
	CHECK_EQ(Formula(variant("types_compatible('{string -> any}', '{string -> int}')")).execute().as_bool(), true);
	CHECK_EQ(Formula(variant("types_compatible('{string -> int}', '{any -> int}')")).execute().as_bool(), false);
	CHECK_EQ(Formula(variant("types_compatible('{any -> int}', '{string -> int}')")).execute().as_bool(), true);
	CHECK_EQ(Formula(variant("types_compatible('{string -> int}', '{any -> any}')")).execute().as_bool(), false);
	CHECK_EQ(Formula(variant("types_compatible('{any -> any}', '{string -> int}')")).execute().as_bool(), true);
	CHECK_EQ(Formula(variant("types_compatible('{any -> int}', '{string -> any}')")).execute().as_bool(), false);
	CHECK_EQ(Formula(variant("types_compatible('{string -> any}', '{any -> int}')")).execute().as_bool(), false);
}

UNIT_TEST(formula_multifunction_types_compatible) {
	CHECK_EQ(Formula(variant("types_compatible('function(int,any) ->int', 'function(int,int) ->int')")).execute().as_bool(), false);
	CHECK_EQ(Formula(variant("types_compatible('function(int,int) ->int', 'function(int,int) ->int')")).execute().as_bool(), true);
	CHECK_EQ(Formula(variant("types_compatible('function(int,int) ->int', 'function(int,any) ->int')")).execute().as_bool(), true);
	CHECK_EQ(Formula(variant("types_compatible('function(int,int) ->int', 'function(any,int) ->int')")).execute().as_bool(), true);
	CHECK_EQ(Formula(variant("types_compatible('function(int,int) ->int', 'function(any,any) ->int')")).execute().as_bool(), true);
	CHECK_EQ(Formula(variant("types_compatible('function(int,int) ->int', 'function(any,any) ->any')")).execute().as_bool(), false);
	CHECK_EQ(Formula(variant("types_compatible('function(int,int) ->any', 'function(any,any) ->any')")).execute().as_bool(), true);
	CHECK_EQ(Formula(variant("types_compatible('function(int,int) ->any', 'function(any,string) ->any')")).execute().as_bool(), false);
	CHECK_EQ(Formula(variant("types_compatible('function(string,int) ->any', 'function(int,string) ->any')")).execute().as_bool(), false);
}

UNIT_TEST(formula_list_comprehension) {
	std::vector<variant> result;
	for(int n = 0; n != 4; ++n) {
		result.push_back(variant(n));
	}

	CHECK_EQ(Formula(variant("[x | x <- [0,1,2,3]]")).execute(), variant(&result));
	CHECK_EQ(Formula(variant("[x | x <- [0,1,2,3], x%2 = 1]")).execute(), Formula(variant("[1,3]")).execute());
}

UNIT_TEST(edit_distance) {
	CHECK_EQ(edit_distance_calculator("aa", "bb")(), 2);
	CHECK_EQ(edit_distance_calculator("ab", "bb")(), 1);
	CHECK_EQ(edit_distance_calculator("bb", "bb")(), 0);
	CHECK_EQ(edit_distance_calculator("abcdefg", "hijklmn")(), 7);
	CHECK_EQ(edit_distance_calculator("abcdefg", "bcdefg")(), 1);
	CHECK_EQ(edit_distance_calculator("abcdefg", "abcefg")(), 1);
	CHECK_EQ(edit_distance_calculator("abcdefg", "abdcefg")(), 1);
	CHECK_EQ(edit_distance_calculator("abcdefg", "abdcegf")(), 2);
	CHECK_EQ(edit_distance_calculator("abcdefg", "bdcegf")(), 3);
}

UNIT_TEST(formula_enum) {
	CHECK_EQ(Formula(variant("enum abc = enum abc")).execute(), variant::from_bool(true));
	CHECK_EQ(Formula(variant("enum abc != enum abc")).execute(), variant::from_bool(false));
	CHECK_EQ(Formula(variant("enum abc = enum d")).execute(), variant::from_bool(false));
}

UNIT_TEST(generic_function_0) {
	const std::string code =
			"f<<int>>(2) where f = def << T >> (T t) -> T t * t";
	const variant code_variant(code);
	const Formula code_variant_formula(code_variant);
	const variant output = code_variant_formula.execute();
	check::type_is_int(output);
	CHECK_EQ(output, variant(4));
}

UNIT_TEST(generic_function_1) {
	const std::string code =
			"f<<int>>(2.0) where f = def << T >> (T t) -> T t * t";
	const variant code_variant(code);
	const Formula code_variant_formula(code_variant);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			code_variant_formula.execute();
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(excepted, true);
}

UNIT_TEST(generic_function_2) {
	const std::string code =
			"f<<decimal>>(2.0) where f = def << T >> (T t) -> T t * t";
	const variant code_variant(code);
	const Formula code_variant_formula(code_variant);
	const variant output = code_variant_formula.execute();
	check::type_is_decimal(output);
	CHECK_EQ(output, variant(4.0));
}

UNIT_TEST(generic_function_3) {
	const std::string code =
			"f<<decimal>>(2) where f = def << T >> (T t) -> T t * t";
	const variant code_variant(code);
	const Formula code_variant_formula(code_variant);
	const variant output = code_variant_formula.execute();
	check::type_is_int(output);
	CHECK_EQ(output, variant(4));
}

UNIT_TEST(asserting_supposed_to_succeed_0) {
	const std::string code =
			"a asserting a is int where a = 3";
	const variant code_variant(code);
	const Formula code_variant_formula(code_variant);
	const variant output = code_variant_formula.execute();
	check::type_is_int(output);
	CHECK_EQ(output, variant(3));
}

UNIT_TEST(asserting_supposed_to_succeed_1) {
	const std::string code =
			"a asserting a is decimal where a = 3";
	const variant code_variant(code);
	const Formula code_variant_formula(code_variant);
	const variant output = code_variant_formula.execute();
	check::type_is_int(output);
	CHECK_EQ(output, variant(3));
}

// XXX    Code running normally will abort fatally, as it has to, when
// XXX  failing a type assertion. It would abort fatally also when
// XXX  running this test, that's why it is disabled.
UNIT_TEST(asserting_supposed_to_fail_FAILS) {
	const std::string code =
			"a asserting a is not decimal where a = 3.0";
	const variant code_variant(code);
	const Formula code_variant_formula(code_variant);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			code_variant_formula.execute();
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(excepted, true);
}

UNIT_TEST(identifier_suggested_0) {

	// XXX    Can not assert that with this `StrictCheckScope` code emits
	// XXX  a warning (suggesting a different identifier, typo detection),
	// XXX  but that there is no such warning when not providing this
	// XXX  `StrictCheckScope`.
	const game_logic::Formula::StrictCheckScope strict_checking(
			true, true);

	//   There is only one similar identifier at a same distance to `aaaa`.
	// So correcting to `aaaaa` is suggested.
	const std::string code =
			"aaaa where aaaaa = 3";

	const variant code_variant(code);
	const Formula code_variant_formula(code_variant);
	const variant output = code_variant_formula.execute();
	check::type_is_null(output);
	CHECK_EQ(output, variant());
}

UNIT_TEST(identifier_suggested_1) {

	// XXX    Can not assert that with this `StrictCheckScope` code emits
	// XXX  a warning (suggesting a different identifier, typo detection),
	// XXX  but that there is no such warning when not providing this
	// XXX  `StrictCheckScope`.
	const game_logic::Formula::StrictCheckScope strict_checking(
			true, true);

	//   There are two similar identifiers at the same distance to `aaaa`.
	// So no correction is suggested.
	const std::string code =
			"aaaa where aaab = 3 where aaaaa = 3";

	const variant code_variant(code);
	const Formula code_variant_formula(code_variant);
	const variant output = code_variant_formula.execute();
	check::type_is_null(output);
	CHECK_EQ(output, variant());
}

UNIT_TEST(semicolon_sequencing) {
	const std::string code = "null; null; null";
	const variant code_variant(code);
	const Formula code_variant_formula(code_variant);
	const variant output = code_variant_formula.execute();
	check::type_is_object(output);
}

UNIT_TEST(array_dereference_accepts_nesting) {
	const std::string code =
			"map(range(2), a[b[value]]) where a = [0, 0, 3, 0, 3] where b = [2, 4]";
	const variant code_variant(code);
	const Formula code_variant_formula(code_variant);
	const variant output = code_variant_formula.execute();
	check::type_is_list(output);
	const std::vector<variant> output_as_list = output.as_list();
	const uint_fast8_t output_as_list_size = output_as_list.size();
	CHECK_EQ(output_as_list_size, 2);
	for (uint_fast8_t i = 0; i < output_as_list_size; i++) {
		const variant element = output[i];
		check::type_is_int(element);
		const int_fast32_t element_as_int = element.as_int();
		CHECK_EQ(element_as_int, 3);
	}
}

BENCHMARK(formula_list_comprehension_bench) {
	Formula f(variant("[x*x + 5 | x <- range(input)]"));
	static MapFormulaCallable* callable = new MapFormulaCallable;
	callable->add("input", variant(1000));
	BENCHMARK_LOOP {
		f.execute(*callable);
	}
}

BENCHMARK(formula_map_bench) {
	Formula f(variant("map(range(input), value*value + 5)"));
	static MapFormulaCallable* callable = new MapFormulaCallable;
	callable->add("input", variant(1000));
	BENCHMARK_LOOP {
		f.execute(*callable);
	}
}

BENCHMARK(formula_recurse_sort) {
	Formula f(variant("def my_qsort(items) if(size(items) <= 1, items,"
					  " my_qsort(filter(items, i, i < items[0])) +"
					  "          filter(items, i, i = items[0]) +"
					  " my_qsort(filter(items, i, i > items[0])));"
					  "my_qsort(input)"));

	std::vector<variant> input;
	for(int n = 0; n != 100000; ++n) {
		input.push_back(variant(n));
	}

	std::vector<variant> expected_result = input;
	variant expected_result_v(&expected_result);

	std::random_shuffle(input.begin(), input.end());
	static MapFormulaCallable* callable = new MapFormulaCallable;
	callable->add("input", variant(&input));
	BENCHMARK_LOOP {
		CHECK_EQ(f.execute(*callable), expected_result_v);
	}
}

BENCHMARK(formula_recursion) {
	Formula f(variant(
"def my_index(ls, item, n)"
"base ls = []: -1 "
"base ls[0] = item: n "
"recursive: my_index(ls[1:], item, n+1);"
"my_index(range(1000001), pos, 0)"));

	Formula f2(variant(
"def silly_add(a, b)"
"base b <= 0: a "
"recursive: silly_add(a+1, b-1);"
"silly_add(0, pos)"));
	static MapFormulaCallable* callable = new MapFormulaCallable;
	callable->add("pos", variant(100000));
	BENCHMARK_LOOP {
		CHECK_EQ(f.execute(*callable), variant(100000));
	}
}

BENCHMARK(formula_if) {
	static MapFormulaCallable* callable = new MapFormulaCallable;
	callable->add("x", variant(1));
	static Formula f(variant("if(x, 1, 0)"));
	BENCHMARK_LOOP {
		f.execute(*callable);
	}
}

BENCHMARK(formula_add) {
	static MapFormulaCallable* callable = new MapFormulaCallable;
	callable->add("x", variant(1));
	static Formula f(variant("x+1"));
	BENCHMARK_LOOP {
		f.execute(*callable);
	}
}

COMMAND_LINE_UTILITY(test_multithread_variants) {
	std::vector<variant> lists;

	for(int n = 0; n != 20; ++n) {
		std::vector<variant> mylist;
		for(int m = 0; m != 2; ++m) {
			mylist.push_back(variant(int(rand()%10)));
		}

		lists.push_back(variant(&mylist));
	}

	for(int n = 0; n != 10; ++n) {
		std::map<variant,variant> mymap;
		mymap[variant("a")] = variant(int(rand()%10));
		lists.push_back(variant(&mymap));
	}

	std::vector<std::thread> threads;

	for(int n = 0; n != 16; ++n) {
		threads.push_back(std::thread([=,&lists] {
			fprintf(stderr, "THREAD: %d\n", n);
			for(;;) {
				int sum = 0;
				for(int m = 0; m != 10000; ++m) {
					variant item = lists[rand()%20];
					if(item.is_list()) {
						sum += item[0].as_int();
					} else {
						sum += item["a"].as_int();
					}
				}

				//fprintf(stderr, "THREAD %d: %d\n", n, sum);
			}
		}));
	}

	SDL_Delay(100000);
}

}
