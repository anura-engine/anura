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
#ifndef FORMULA_FUNCTION_HPP_INCLUDED
#define FORMULA_FUNCTION_HPP_INCLUDED

#include <boost/intrusive_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include <assert.h>

#include <iostream>
#include <map>

#include "formula_callable_definition_fwd.hpp"
#include "formula_callable_utils.hpp"
#include "formula_fwd.hpp"
#include "variant.hpp"
#include "variant_type.hpp"

namespace game_logic {

class formula_expression;
typedef boost::intrusive_ptr<formula_expression> expression_ptr;
typedef boost::intrusive_ptr<const formula_expression> const_expression_ptr;

struct PinpointedLoc {
	int begin_line, end_line, begin_col, end_col;
};

std::string pinpoint_location(variant v, std::string::const_iterator begin);
std::string pinpoint_location(variant v, std::string::const_iterator begin,
                                         std::string::const_iterator end,
										 PinpointedLoc* pos_info=0);

class formula_expression : public reference_counted_object {
public:
	explicit formula_expression(const char* name=NULL);
	virtual ~formula_expression() {}
	virtual variant static_evaluate(const formula_callable& variables) const {
		return evaluate(variables);
	}

	virtual bool is_identifier(std::string* id) const {
		return false;
	}

	virtual bool is_literal(variant& result) const {
		return false;
	}

	variant evaluate(const formula_callable& variables) const {
#if !TARGET_OS_IPHONE
		++ntimes_called_;
		call_stack_manager manager(this, &variables);
#endif
		return execute(variables);
	}

	variant evaluate_with_member(const formula_callable& variables, std::string& id, variant* variant_id=NULL) const {
#if !TARGET_OS_IPHONE
		call_stack_manager manager(this, &variables);
#endif
		return execute_member(variables, id, variant_id);
	}

	void perform_static_error_analysis() const {
		static_error_analysis();
	}

	virtual expression_ptr optimize() const {
		return expression_ptr();
	}

	virtual bool can_reduce_to_variant(variant& v) const {
		return false;
	}

	virtual const_formula_callable_definition_ptr get_type_definition() const;

	const char* name() const { return name_; }
	void set_name(const char* name) { name_ = name; }

	void copy_debug_info_from(const formula_expression& o);
	virtual void set_debug_info(const variant& parent_formula,
	                            std::string::const_iterator begin_str,
	                            std::string::const_iterator end_str);
	bool has_debug_info() const;
	std::string debug_pinpoint_location(PinpointedLoc* loc=NULL) const;
	std::pair<int, int> debug_loc_in_file() const;

	void set_str(const std::string& str) { str_ = str; }
	const std::string& str() const { return str_; }

	variant parent_formula() const { return parent_formula_; }

	int ntimes_called() const { return ntimes_called_; }

	variant_type_ptr query_variant_type() const { variant_type_ptr res = get_variant_type(); if(res) { return res; } else { return variant_type::get_any(); } }

	variant_type_ptr query_mutable_type() const { return get_mutable_type(); }

	const_formula_callable_definition_ptr query_modified_definition_based_on_result(bool result, const_formula_callable_definition_ptr current_def, variant_type_ptr expression_is_this_type=variant_type_ptr()) const { return get_modified_definition_based_on_result(result, current_def, expression_is_this_type); }

	std::vector<const_expression_ptr> query_children() const;
	std::vector<const_expression_ptr> query_children_recursive() const;

	void set_definition_used_by_expression(const_formula_callable_definition_ptr def) { definition_used_ = def; }
	const_formula_callable_definition_ptr get_definition_used_by_expression() const { return definition_used_; }

protected:
	virtual variant_type_ptr get_variant_type() const { return variant_type_ptr(); }
	virtual variant_type_ptr get_mutable_type() const { return variant_type_ptr(); }
	virtual variant execute_member(const formula_callable& variables, std::string& id, variant* variant_id) const;
private:
	virtual variant execute(const formula_callable& variables) const = 0;
	virtual void static_error_analysis() const {}
	virtual const_formula_callable_definition_ptr get_modified_definition_based_on_result(bool result, const_formula_callable_definition_ptr current_def, variant_type_ptr expression_is_this_type) const { return NULL; }

	virtual std::vector<const_expression_ptr> get_children() const { return std::vector<const_expression_ptr>(); }

	const char* name_;

	variant parent_formula_;
	std::string::const_iterator begin_str_, end_str_;
	std::string str_;

	mutable int ntimes_called_;

	const_formula_callable_definition_ptr definition_used_;
};

class function_expression : public formula_expression {
public:
	typedef std::vector<expression_ptr> args_list;
	explicit function_expression(
	                    const std::string& name,
	                    const args_list& args,
	                    int min_args=-1, int max_args=-1);

	virtual void set_debug_info(const variant& parent_formula,
	                            std::string::const_iterator begin_str,
	                            std::string::const_iterator end_str);

protected:
	const std::string& name() const { return name_; }
	const args_list& args() const { return args_; }

	void check_arg_type(int narg, const std::string& type) const;

private:
	std::vector<const_expression_ptr> get_children() const {
		return std::vector<const_expression_ptr>(args_.begin(), args_.end());
	}

	std::string name_;
	args_list args_;
	int min_args_, max_args_;
};

class formula_function_expression : public function_expression {
public:
	explicit formula_function_expression(const std::string& name, const args_list& args, const_formula_ptr formula, const_formula_ptr precondition, const std::vector<std::string>& arg_names, const std::vector<variant_type_ptr>& variant_types);
	virtual ~formula_function_expression() {}

	void set_formula(const_formula_ptr f) { formula_ = f; }
	void set_has_closure(int base_slot) { has_closure_ = true; base_slot_ = base_slot; }
private:
	boost::intrusive_ptr<slot_formula_callable> calculate_args_callable(const formula_callable& variables) const;
	variant execute(const formula_callable& variables) const;
	const_formula_ptr formula_;
	const_formula_ptr precondition_;
	std::vector<std::string> arg_names_;
	std::vector<variant_type_ptr> variant_types_;
	int star_arg_;

	//this is the callable object that is populated with the arguments to the
	//function. We try to reuse the same object every time the function is
	//called rather than recreating it each time.
	mutable boost::intrusive_ptr<slot_formula_callable> callable_;

	mutable boost::scoped_ptr<variant> fed_result_;
	bool has_closure_;
	int base_slot_;

};

typedef boost::intrusive_ptr<function_expression> function_expression_ptr;
typedef boost::intrusive_ptr<formula_function_expression> formula_function_expression_ptr;

class formula_function {
	std::string name_;
	const_formula_ptr formula_;
	const_formula_ptr precondition_;
	std::vector<std::string> args_;
	std::vector<variant> default_args_;
	std::vector<variant_type_ptr> variant_types_;
public:
	formula_function() {}
	formula_function(const std::string& name, const_formula_ptr formula, const_formula_ptr precondition, const std::vector<std::string>& args, const std::vector<variant>& default_args, const std::vector<variant_type_ptr>& variant_types) : name_(name), formula_(formula), precondition_(precondition), args_(args), default_args_(default_args), variant_types_(variant_types)
	{}

	formula_function_expression_ptr generate_function_expression(const std::vector<expression_ptr>& args) const;

	const std::vector<std::string>& args() const { return args_; }
	const std::vector<variant> default_args() const { return default_args_; }
	const_formula_ptr get_formula() const { return formula_; }
	const std::vector<variant_type_ptr>& variant_types() const { return variant_types_; }
};	

class function_symbol_table : private boost::noncopyable 
{
	std::map<std::string, formula_function> custom_formulas_;
	const function_symbol_table* backup_;
public:
	function_symbol_table() : backup_(0) {}
	virtual ~function_symbol_table() {}
	void set_backup(const function_symbol_table* backup) { backup_ = backup; }
	virtual void add_formula_function(const std::string& name, const_formula_ptr formula, const_formula_ptr precondition, const std::vector<std::string>& args, const std::vector<variant>& default_args, const std::vector<variant_type_ptr>& variant_types);
	virtual expression_ptr create_function(const std::string& fn,
					                       const std::vector<expression_ptr>& args,
										   const_formula_callable_definition_ptr callable_def) const;
	std::vector<std::string> get_function_names() const;
	const formula_function* get_formula_function(const std::string& fn) const;
};

//a special symbol table which is used to facilitate recursive functions.
//it is given to a formula function during parsing, and will give out
//function stubs for recursive calls. At the end of parsing it can fill
//in the real call.
class recursive_function_symbol_table : public function_symbol_table {
	std::string name_;
	formula_function stub_;
	function_symbol_table* backup_;
	mutable std::vector<formula_function_expression_ptr> expr_;
	const_formula_callable_definition_ptr closure_definition_;
public:
	recursive_function_symbol_table(const std::string& fn, const std::vector<std::string>& args, const std::vector<variant>& default_args, function_symbol_table* backup, const_formula_callable_definition_ptr closure_definition, const std::vector<variant_type_ptr>& variant_types);
	virtual expression_ptr create_function(const std::string& fn,
					                       const std::vector<expression_ptr>& args,
										   const_formula_callable_definition_ptr callable_def) const;
	void resolve_recursive_calls(const_formula_ptr f);
};

expression_ptr create_function(const std::string& fn,
                               const std::vector<expression_ptr>& args,
							   const function_symbol_table* symbols,
							   const_formula_callable_definition_ptr callable_def);
bool optimize_function_arguments(const std::string& fn,
                                 const function_symbol_table* symbols);
std::vector<std::string> builtin_function_names();

class variant_expression : public formula_expression {
public:
	explicit variant_expression(variant v) : formula_expression("_var"), v_(v)
	{}

	bool can_reduce_to_variant(variant& v) const {
		v = v_;
		return true;
	}

	bool is_literal(variant& result) const {
		result = v_;
		return true;
	}

	void set_type_override(variant_type_ptr type) {
		type_override_ = type;
	}
private:
	variant execute(const formula_callable& /*variables*/) const {
		return v_;
	}

	virtual variant_type_ptr get_variant_type() const;
	
	variant v_;
	variant_type_ptr type_override_;
};

const_formula_callable_definition_ptr get_map_callable_definition(const_formula_callable_definition_ptr base_def, variant_type_ptr key_type, variant_type_ptr value_type, const std::string& value_name);
const_formula_callable_definition_ptr get_variant_comparator_definition(const_formula_callable_definition_ptr base_def, variant_type_ptr type);

}

game_logic::function_symbol_table& get_formula_functions_symbol_table();

#endif
