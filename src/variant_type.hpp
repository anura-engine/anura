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

#include "ffl_weak_ptr.hpp"
#include "formula_callable.hpp"
#include "formula_tokenizer.hpp"
#include "reference_counted_object.hpp"
#include "variant.hpp"

namespace game_logic
{
	class FormulaCallableDefinition;
	class FormulaInterface;
class formula_expression;
}

struct types_cfg_scope
{
	explicit types_cfg_scope(variant v);
	~types_cfg_scope();
};

class variant_type;
typedef ffl::IntrusivePtr<const variant_type> variant_type_ptr;
typedef ffl::IntrusivePtr<const variant_type> const_variant_type_ptr;

class variant_type : public game_logic::FormulaCallable
{
public:
	variant getValue(const std::string& id) const override { return variant(); }

	static variant_type_ptr get_none();
	static variant_type_ptr get_any();
	static variant_type_ptr get_commands();
	static variant_type_ptr get_cairo_commands();
	static variant_type_ptr get_type(variant::TYPE type);
	static variant_type_ptr get_union(const std::vector<variant_type_ptr>& items);
	static variant_type_ptr get_list(variant_type_ptr element_type);
	static variant_type_ptr get_specific_list(const std::vector<variant_type_ptr>& types);
	static variant_type_ptr get_map(variant_type_ptr key_type, variant_type_ptr value_type);
	static variant_type_ptr get_specific_map(const std::map<variant, variant_type_ptr>& type_map);
	static variant_type_ptr get_class(const std::string& class_name);
	static variant_type_ptr get_custom_object(const std::string& name="");
	static variant_type_ptr get_voxel_object(const std::string& name="");
	static variant_type_ptr get_builtin(const std::string& id);
	static variant_type_ptr get_function_type(const std::vector<variant_type_ptr>& arg_types, variant_type_ptr return_type, int min_args);
	static variant_type_ptr get_function_overload_type(variant_type_ptr overloaded_fn, const std::vector<variant_type_ptr>& fn);

	//get a version of the type that we now know isn't null.
	static variant_type_ptr get_null_excluded(variant_type_ptr input);
	static variant_type_ptr get_with_exclusion(variant_type_ptr input, variant_type_ptr subtract);

	static variant_type_ptr get_generic_type(const std::string& id);

	variant_type();
	virtual ~variant_type();
	virtual bool match(const variant& v) const = 0;

	virtual std::string mismatch_reason(const variant& v) const { return ""; }

	//decay from enum.
	virtual variant_type_ptr base_type_no_enum() const { return variant_type_ptr(this); }

	struct conversion_failure_exception {};
	variant convert(const variant& v) const { if(match(v)) return v; return convert_impl(v); }

	virtual bool is_numeric() const { return false; }
	virtual bool is_none() const { return false; }
	virtual bool is_any() const { return false; }
	virtual const std::vector<variant_type_ptr>* is_union() const { return nullptr; }
	virtual variant_type_ptr is_list_of() const { return variant_type_ptr(); }
	virtual const std::vector<variant_type_ptr>* is_specific_list() const { return nullptr; }
	virtual std::pair<variant_type_ptr,variant_type_ptr> is_map_of() const { return std::pair<variant_type_ptr,variant_type_ptr>(); }
	virtual const std::map<variant, variant_type_ptr>* is_specific_map() const { return nullptr; }
	virtual bool is_type(variant::TYPE type) const { return false; }
	virtual bool is_class(std::string* class_name=nullptr) const { return false; }
	virtual const std::string* is_builtin() const { return nullptr; }
	virtual const std::string* is_custom_object() const { return nullptr; }
	virtual const std::string* is_voxel_object() const { return nullptr; }

	virtual bool is_function(std::vector<variant_type_ptr>* args, variant_type_ptr* return_type, int* min_args, bool* return_type_specified=nullptr) const { return false; }
	virtual bool is_generic(std::string* id=nullptr) const { return false; }
	virtual variant_type_ptr function_return_type_with_args(const std::vector<variant_type_ptr>& args) const { variant_type_ptr result; is_function(nullptr, &result, nullptr); return result; }

	virtual const game_logic::FormulaCallableDefinition* getDefinition() const { return nullptr; }

	virtual const game_logic::FormulaInterface* is_interface() const { return nullptr; }

	void setStr(const std::string& s) const { str_ = s; }
	const std::string& str() const { return str_; }

	std::string to_string() const { return to_string_impl(); }

	virtual bool is_equal(const variant_type& o) const = 0;

	virtual bool is_compatible(variant_type_ptr type, std::ostringstream* why=nullptr) const { return false; }

	virtual bool maybe_convertible_to(variant_type_ptr type) const { return false; }
	virtual variant_type_ptr map_generic_types(const std::map<std::string, variant_type_ptr>& mapping) const { return variant_type_ptr(); }


	static bool may_be_null(variant_type_ptr type);

	void set_expr(const game_logic::FormulaExpression* expr) const;
	ffl::IntrusivePtr<const game_logic::FormulaExpression> get_expr() const;

	virtual variant_type_ptr extend_type(variant_type_ptr extension) const { return variant_type_ptr(); }

private:
	virtual variant_type_ptr null_excluded() const { return variant_type_ptr(); }
	virtual variant_type_ptr subtract(variant_type_ptr other) const { return variant_type_ptr(); }

	virtual variant convert_impl(const variant& v) const { throw conversion_failure_exception(); }

	virtual std::string to_string_impl() const = 0;

	mutable std::string str_;

	mutable ffl::weak_ptr<const game_logic::FormulaExpression> expr_;
};


variant_type_ptr get_variant_type_from_value(const variant& value);

std::string variant_type_is_class_or_null(variant_type_ptr type);

bool variant_types_compatible(variant_type_ptr to, variant_type_ptr from, std::ostringstream* why=nullptr);
bool variant_types_might_match(variant_type_ptr to, variant_type_ptr from);

variant_type_ptr parse_variant_type(const variant& original_str,
                                    const formula_tokenizer::Token*& i1,
                                    const formula_tokenizer::Token* i2,
									bool allow_failure=false);
variant_type_ptr parse_variant_type(const variant& v);

variant_type_ptr
parse_optional_function_type(const variant& original_str,
                             const formula_tokenizer::Token*& i1,
                             const formula_tokenizer::Token* i2);
variant_type_ptr parse_optional_function_type(const variant& v);

variant_type_ptr
parse_optional_formula_type(const variant& original_str,
                            const formula_tokenizer::Token*& i1,
                            const formula_tokenizer::Token* i2);
variant_type_ptr parse_optional_formula_type(const variant& v);

class generic_variant_type_scope
{
	std::vector<std::string> entries_;
public:
	~generic_variant_type_scope();

	void register_type(const std::string& id);
	void clear();
};
