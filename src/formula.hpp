/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#ifndef FORMULA_HPP_INCLUDED
#define FORMULA_HPP_INCLUDED

#include <map>
#include <string>

#include "formula_callable_definition.hpp"
#include "formula_fwd.hpp"
#include "formula_function.hpp"
#include "formula_tokenizer.hpp"
#include "variant.hpp"
#include "variant_type.hpp"

std::string output_formula_error_info();

namespace game_logic
{

void set_verbatim_string_expressions(bool verbatim);

class formula_callable;
class formula_expression;
class function_symbol_table;
typedef boost::intrusive_ptr<formula_expression> expression_ptr;

//helper struct which contains info for a where expression.
struct where_variables_info : public reference_counted_object {
	explicit where_variables_info(int nslot) : base_slot(nslot) {}
	std::vector<std::string> names;
	std::vector<expression_ptr> entries;
	int base_slot;
	const_formula_callable_definition_ptr callable_where_def;
};

typedef boost::intrusive_ptr<where_variables_info> where_variables_info_ptr;

class formula {
public:

	//use one of these guys if you want to evaluate a formula but lower
	//down in the stack, formulas might be being parsed.
	struct non_static_context {
		int old_value_;
		non_static_context();
		~non_static_context();
	};

	//a function which makes the current executing formula fail if
	//it's attempting to evaluate in a static context.
	static void fail_if_static_context();

	static variant evaluate(const const_formula_ptr& f,
	                    const formula_callable& variables,
						variant default_res=variant(0)) {
		if(f) {
			return f->execute(variables);
		} else {
			return default_res;
		}
	}

	struct strict_check_scope {
		explicit strict_check_scope(bool is_strict=true, bool warnings=false);
		~strict_check_scope();

		bool old_value;
		bool old_warning_value;
	};

	enum FORMULA_LANGUAGE { LANGUAGE_FFL, LANGUAGE_LUA  };

	static const std::set<formula*>& get_all();

	static formula_ptr create_optional_formula(const variant& str, function_symbol_table* symbols=NULL, const_formula_callable_definition_ptr def=NULL, FORMULA_LANGUAGE lang=LANGUAGE_FFL);
	explicit formula(const variant& val, function_symbol_table* symbols=NULL, const_formula_callable_definition_ptr def=NULL);
	formula(const variant& lua_fn, FORMULA_LANGUAGE lang);
	~formula();
	variant execute(const formula_callable& variables) const;
	variant execute() const;
	bool evaluates_to_constant(variant& result) const;
	std::string str() const { return str_.as_string(); }
	variant str_var() const { return str_; }

	std::string output_debug_info() const;

	bool has_guards() const { return base_expr_.empty() == false; }
	int guard_matches(const formula_callable& variables) const;

	//guard matches without wrapping 'variables' in the global callable.
	int raw_guard_matches(const formula_callable& variables) const;

	const_formula_callable_ptr wrap_callable_with_global_where(const formula_callable& callable) const;

	const expression_ptr& expr() const { return expr_; }

	variant_type_ptr query_variant_type() const;

private:
	formula() {}
	variant str_;
	expression_ptr expr_;

	const_formula_callable_definition_ptr def_;

	//for recursive function formulae, we have base cases along with
	//base expressions.
	struct BaseCase {
		//raw_guard is the guard without wrapping in the global where.
		expression_ptr raw_guard, guard, expr;
	};
	std::vector<BaseCase> base_expr_;

	where_variables_info_ptr global_where_;

	void check_brackets_match(const std::vector<formula_tokenizer::token>& tokens) const;

};

}

#endif
