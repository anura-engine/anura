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

#include <map>
#include <string>

#include "formula_callable_definition.hpp"
#include "formula_fwd.hpp"
#include "formula_function.hpp"
#include "formula_tokenizer.hpp"
#include "formula_where.hpp"
#include "variant.hpp"
#include "variant_type.hpp"

std::string output_formula_error_info();

namespace game_logic
{
	void set_verbatim_string_expressions(bool verbatim);

	class FormulaCallable;
	class FormulaExpression;
	class FunctionSymbolTable;
	typedef ffl::IntrusivePtr<FormulaExpression> ExpressionPtr;

	class Formula 
	{
	public:
		//use one of these guys if you want to evaluate a formula but lower
		//down in the stack, formulas might be being parsed.
		struct NonStaticContext {
			int old_value_;
			NonStaticContext();
			~NonStaticContext();
		};

		//a function which makes the current executing formula fail if
		//it's attempting to evaluate in a static context.
		static void failIfStaticContext();

		static variant evaluate(const ConstFormulaPtr& f,
							const FormulaCallable& variables,
							variant default_res=variant(0)) {
			if(f) {
				return f->execute(variables);
			} else {
				return default_res;
			}
		}

		struct StrictCheckScope {
			explicit StrictCheckScope(bool is_strict=true, bool warnings=false);
			~StrictCheckScope();

			bool old_value;
			bool old_warning_value;
		};

		enum class FORMULA_LANGUAGE { FFL, LUA };

		static const std::set<Formula*>& getAll();

		static FormulaPtr createOptionalFormula(const variant& str, FunctionSymbolTable* symbols=nullptr, ConstFormulaCallableDefinitionPtr def=nullptr, FORMULA_LANGUAGE lang=FORMULA_LANGUAGE::FFL);
		explicit Formula(const variant& val, FunctionSymbolTable* symbols=nullptr, ConstFormulaCallableDefinitionPtr def=nullptr);
		~Formula();
		variant execute(const FormulaCallable& variables) const;
		variant execute() const;
		bool evaluatesToConstant(variant& result) const;
		std::string str() const { return str_.as_string(); }
		const variant& strVal() const { return str_; }

		std::string outputDebugInfo() const;
		bool outputDisassemble(std::string* result) const;

		bool hasGuards() const { return base_expr_.empty() == false; }
		int guardMatches(const FormulaCallable& variables) const;

		//guard matches without wrapping 'variables' in the global callable.
		int rawGuardMatches(const FormulaCallable& variables) const;

		ConstFormulaCallablePtr wrapCallableWithGlobalWhere(const FormulaCallable& callable) const;

		const ExpressionPtr& expr() const { return expr_; }

		variant_type_ptr queryVariantType() const;

	private:
		Formula();
		variant str_;
		ExpressionPtr expr_;

		variant_type_ptr type_;

		ConstFormulaCallableDefinitionPtr def_;

		//for recursive function formulae, we have base cases along with
		//base expressions.
		struct BaseCase {
			//raw_guard is the guard without wrapping in the global where.
			ExpressionPtr raw_guard, guard, expr;
		};
		std::vector<BaseCase> base_expr_;

		WhereVariablesInfoPtr global_where_;

		void checkBracketsMatch(const std::vector<formula_tokenizer::Token>& tokens) const;
	};
}
