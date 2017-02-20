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

#include "intrusive_ptr.hpp"
#include <boost/noncopyable.hpp>

#include <assert.h>

#include <iostream>
#include <map>

#include "formula_garbage_collector.hpp"

#include "formula_callable_definition_fwd.hpp"
#include "formula_callable_utils.hpp"
#include "formula_fwd.hpp"
#include "variant.hpp"
#include "variant_type.hpp"

namespace formula_vm {
	class VirtualMachine;
}

namespace game_logic
{
	class FormulaExpression;
	typedef ffl::IntrusivePtr<FormulaExpression> ExpressionPtr;
	typedef ffl::IntrusivePtr<const FormulaExpression> ConstExpressionPtr;

	struct PinpointedLoc 
	{
		int begin_line, end_line, begin_col, end_col;
	};

	std::string pinpoint_location(variant v, std::string::const_iterator begin);
	std::string pinpoint_location(variant v, std::string::const_iterator begin,
											 std::string::const_iterator end,
											 PinpointedLoc* pos_info=0);

	class FormulaExpression : public FormulaCallable 
	{
	public:
		variant getValue(const std::string& key) const override { return variant(); }

		explicit FormulaExpression(const char* name=nullptr);
		virtual ~FormulaExpression();
		virtual variant staticEvaluate(const FormulaCallable& variables) const {
			return evaluate(variables);
		}

		virtual bool isIdentifier(std::string* id) const {
			return false;
		}

		virtual bool isLiteral(variant& result) const {
			return false;
		}

		variant evaluate(const FormulaCallable& variables) const {
			++ntimes_called_;
			CallStackManager manager(this, &variables);
			return execute(variables);
		}

		variant evaluateWithMember(const FormulaCallable& variables, std::string& id, variant* variant_id=nullptr) const {
			CallStackManager manager(this, &variables);
			return executeMember(variables, id, variant_id);
		}

		void performStaticErrorAnalysis() const {
			staticErrorAnalysis();
		}

		virtual ExpressionPtr optimize() const {
			return ExpressionPtr();
		}

		virtual bool canReduceToVariant(variant& v) const {
			return false;
		}

		virtual ConstFormulaCallableDefinitionPtr getTypeDefinition() const;

		const char* name() const { return name_; }
		void setName(const char* name) { name_ = name; }

		void copyDebugInfoFrom(const FormulaExpression& o);
		virtual void setDebugInfo(const variant& parent_formula,
									std::string::const_iterator begin_str,
									std::string::const_iterator end_str);
		virtual void setDebugInfo(const FormulaExpression& o);
		bool hasDebugInfo() const;
		std::string debugPinpointLocation(PinpointedLoc* loc=nullptr) const;
		std::pair<int, int> debugLocInFile() const;

		std::string str() const { return std::string(begin_str_, end_str_); }

		variant getParentFormula() const { return parent_formula_; }

		int getNTimesCalled() const { return ntimes_called_; }

		variant_type_ptr queryVariantType() const { variant_type_ptr res = getVariantType(); if(res) { if(res->refcount() == 1) { res.get()->set_expr(this); } return res; } else { return variant_type::get_any(); } }

		variant_type_ptr queryMutableType() const { return getMutableType(); }

		ConstFormulaCallableDefinitionPtr queryModifiedDefinitionBasedOnResult(bool result, ConstFormulaCallableDefinitionPtr current_def, variant_type_ptr expression_is_this_type=variant_type_ptr()) const { return getModifiedDefinitionBasedOnResult(result, current_def, expression_is_this_type); }

		std::vector<ConstExpressionPtr> queryChildren() const;
		std::vector<ConstExpressionPtr> queryChildrenRecursive() const;

		void setDefinitionUsedByExpression(ConstFormulaCallableDefinitionPtr def) { definition_used_ = def; }
		ConstFormulaCallableDefinitionPtr getDefinitionUsedByExpression() const { return definition_used_; }

		bool canChildrenVM() const { for(auto p : queryChildren()) { if(p && !p->canCreateVM()) return false; } return true; }
		virtual bool canCreateVM() const { return canChildrenVM(); }
		virtual void emitVM(formula_vm::VirtualMachine& vm) const;

		virtual ExpressionPtr optimizeToVM() { return ExpressionPtr(); }

		void setVMDebugInfo(formula_vm::VirtualMachine& vm) const;

		virtual bool isVM() const { return false; }

	protected:
		virtual variant_type_ptr getVariantType() const { return variant_type_ptr(); }
		virtual variant_type_ptr getMutableType() const { return variant_type_ptr(); }
		virtual variant executeMember(const FormulaCallable& variables, std::string& id, variant* variant_id) const;

		void optimizeChildToVM(ExpressionPtr& expr);

		virtual variant execute(const FormulaCallable& variables) const = 0;
		virtual void staticErrorAnalysis() const {}
		virtual ConstFormulaCallableDefinitionPtr getModifiedDefinitionBasedOnResult(bool result, ConstFormulaCallableDefinitionPtr current_def, variant_type_ptr expression_is_this_type) const { return nullptr; }

		virtual std::vector<ConstExpressionPtr> getChildren() const { return std::vector<ConstExpressionPtr>(); }
	private:

		const char* name_;

		variant parent_formula_;
		std::string::const_iterator begin_str_, end_str_;

		ConstFormulaCallableDefinitionPtr definition_used_;
		
		mutable int ntimes_called_;

	};

	class FunctionExpression : public FormulaExpression
	{
	public:
		typedef std::vector<ExpressionPtr> args_list;
		explicit FunctionExpression(
							const std::string& name,
							const args_list& args,
							int min_args=-1, int max_args=-1);

		virtual void setDebugInfo(const variant& parent_formula,
									std::string::const_iterator begin_str,
									std::string::const_iterator end_str) override;

		bool canCreateVM() const override;
		ExpressionPtr optimizeToVM() override;

		virtual variant executeWithArgs(const FormulaCallable& variables, const variant* passed_args, int num_passed_args) const { return execute(variables); }

		const std::string& name() const { return name_; }
		const std::string& module() const { return module_; }
		void setModule(const std::string& m) { module_ = m; }

		void clearUnusedArguments();

	protected:
		const args_list& args() const { return args_; }
		args_list& args_mutable() { return args_; }

		void check_arg_type(int narg, const std::string& type) const;
		void check_arg_type(int narg, variant_type_ptr type) const;

		//When optimizing to a virtual machine asks if a specific arg number
		//should be optimized. Normally returns true but a function might
		//return false if it does something special with the argument
		//and needs access to the actual expression.
		virtual bool optimizeArgNumToVM(int narg) const {
			return true;
		}

		virtual bool useSingletonVM() const { return true; }

		virtual bool dynamicArguments() const { return false; }

	private:
		std::vector<ConstExpressionPtr> getChildren() const override {
			return std::vector<ConstExpressionPtr>(args_.begin(), args_.end());
		}

		std::string module_, name_;
		args_list args_;
		int min_args_, max_args_;
	};

	class FormulaFunctionExpression : public FunctionExpression
	{
	public:
		explicit FormulaFunctionExpression(const std::string& name, const args_list& args, ConstFormulaPtr formula, ConstFormulaPtr precondition, const std::vector<std::string>& arg_names, const std::vector<variant_type_ptr>& variant_types);
		virtual ~FormulaFunctionExpression() {}

		void set_formula(ConstFormulaPtr f) { formula_ = f; }
		void set_has_closure(int base_slot) { has_closure_ = true; base_slot_ = base_slot; }
		virtual ExpressionPtr optimizeToVM() override { return ExpressionPtr(); }
		bool canCreateVM() const override { return false; }
	private:
		ffl::IntrusivePtr<SlotFormulaCallable> calculate_args_callable(const FormulaCallable& variables) const;
		variant execute(const FormulaCallable& variables) const override;

		ConstFormulaPtr formula_;
		ConstFormulaPtr precondition_;
		std::vector<std::string> arg_names_;
		std::vector<variant_type_ptr> variant_types_;
		int star_arg_;

		//this is the callable object that is populated with the arguments to the
		//function. We try to reuse the same object every time the function is
		//called rather than recreating it each time.
		mutable ffl::IntrusivePtr<SlotFormulaCallable> callable_;

		mutable std::unique_ptr<variant> fed_result_;
		bool has_closure_;
		int base_slot_;

	};

	typedef ffl::IntrusivePtr<FunctionExpression> FunctionExpressionPtr;
	typedef ffl::IntrusivePtr<FormulaFunctionExpression> FormulaFunctionExpressionPtr;

	class FormulaFunction
	{
		std::string name_;
		ConstFormulaPtr formula_;
		ConstFormulaPtr precondition_;
		std::vector<std::string> args_;
		std::vector<variant> default_args_;
		std::vector<variant_type_ptr> variant_types_;
	public:
		FormulaFunction() {}
		FormulaFunction(const std::string& name, 
			ConstFormulaPtr formula, 
			ConstFormulaPtr precondition, 
			const std::vector<std::string>& args, 
			const std::vector<variant>& default_args, 
			const std::vector<variant_type_ptr>& variant_types) 
			: name_(name), 
			formula_(formula), 
			precondition_(precondition), 
			args_(args), 
			default_args_(default_args), 
			variant_types_(variant_types)
		{}

		FormulaFunctionExpressionPtr generateFunctionExpression(const std::vector<ExpressionPtr>& args) const;

		const std::vector<std::string>& args() const { return args_; }
		const std::vector<variant> getDefaultArgs() const { return default_args_; }
		ConstFormulaPtr getFormula() const { return formula_; }
		const std::vector<variant_type_ptr>& variantTypes() const { return variant_types_; }
	};	

	class FunctionSymbolTable : private boost::noncopyable 
	{
		std::map<std::string, FormulaFunction> custom_formulas_;
		const FunctionSymbolTable* backup_;
	public:
		FunctionSymbolTable() : backup_(0) {}
		virtual ~FunctionSymbolTable() {}
		void setBackup(const FunctionSymbolTable* backup) { backup_ = backup; }
		virtual void addFormulaFunction(const std::string& name, ConstFormulaPtr formula, ConstFormulaPtr precondition, const std::vector<std::string>& args, const std::vector<variant>& default_args, const std::vector<variant_type_ptr>& variant_types);
		virtual ExpressionPtr createFunction(const std::string& fn,
											   const std::vector<ExpressionPtr>& args,
											   ConstFormulaCallableDefinitionPtr callable_def) const;
		std::vector<std::string> getFunctionNames() const;
		const FormulaFunction* getFormulaFunction(const std::string& fn) const;
	};

	//a special symbol table which is used to facilitate recursive functions.
	//it is given to a formula function during parsing, and will give out
	//function stubs for recursive calls. At the end of parsing it can fill
	//in the real call.
	class RecursiveFunctionSymbolTable : public FunctionSymbolTable
	{
		std::string name_;
		FormulaFunction stub_;
		FunctionSymbolTable* backup_;
		mutable std::vector<FormulaFunctionExpressionPtr> expr_;
		ConstFormulaCallableDefinitionPtr closure_definition_;
	public:
		RecursiveFunctionSymbolTable(const std::string& fn, const std::vector<std::string>& args, const std::vector<variant>& default_args, FunctionSymbolTable* backup, ConstFormulaCallableDefinitionPtr closure_definition, const std::vector<variant_type_ptr>& variant_types);
		virtual ExpressionPtr createFunction(const std::string& fn,
											   const std::vector<ExpressionPtr>& args,
											   ConstFormulaCallableDefinitionPtr callable_def) const override;
		void resolveRecursiveCalls(ConstFormulaPtr f);
	};

	ExpressionPtr createFunction(const std::string& fn,
								   const std::vector<ExpressionPtr>& args,
								   const FunctionSymbolTable* symbols,
								   ConstFormulaCallableDefinitionPtr callable_def);
	bool optimize_function_arguments(const std::string& fn,
									 const FunctionSymbolTable* symbols);
	std::vector<std::string> builtin_function_names();

	class VariantExpression : public FormulaExpression
	{
	public:
		explicit VariantExpression(variant v) : FormulaExpression("_var"), v_(v)
		{}

		bool canReduceToVariant(variant& v) const override {
			v = v_;
			return true;
		}

		bool isLiteral(variant& result) const override {
			result = v_;
			return true;
		}

		void setTypeOverride(variant_type_ptr type) {
			type_override_ = type;
		}

		bool canCreateVM() const override { return true; }
		ExpressionPtr optimizeToVM() override;
	private:
		variant execute(const FormulaCallable& /*variables*/) const override {
			return v_;
		}

		virtual variant_type_ptr getVariantType() const override;
	
		variant v_;
		variant_type_ptr type_override_;
	};

	ConstFormulaCallableDefinitionPtr get_map_callable_definition(ConstFormulaCallableDefinitionPtr base_def, variant_type_ptr key_type, variant_type_ptr value_type, const std::string& value_name);
	ConstFormulaCallableDefinitionPtr get_variant_comparator_definition(ConstFormulaCallableDefinitionPtr base_def, variant_type_ptr type);
}

game_logic::FunctionSymbolTable& get_formula_functions_symbol_table();
