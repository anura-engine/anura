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
#include <cmath>
#include <set>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <sstream>

#include "boost/algorithm/string/replace.hpp"
#include "boost/lexical_cast.hpp"

#include "asserts.hpp"
#include "ffl_weak_ptr.hpp"
#include "foreach.hpp"
#include "formatter.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_callable_utils.hpp"
#include "formula_interface.hpp"
#include "formula_object.hpp"

#include "i18n.hpp"
#include "unit_test.hpp"
#include "variant.hpp"
#include "variant_type.hpp"
#include "wml_formula_callable.hpp"

namespace {
static const std::string variant_type_str[] = {"null", "bool", "int", "decimal", "object", "object_loading", "list", "string", "map", "function", "generic_function", "multi_function", "delayed", "weak"};
}

std::string variant::variant_type_to_string(variant::TYPE type) {
	assert(type >= VARIANT_TYPE_NULL && type < VARIANT_TYPE_INVALID);
	return variant_type_str[type];
}

variant::TYPE variant::string_to_type(const std::string& str) {
	for(int n = 0; n != sizeof(variant_type_str)/sizeof(*variant_type_str); ++n) {
		if(str == variant_type_str[n]) {
			return static_cast<TYPE>(n);
		}
	}

	return VARIANT_TYPE_INVALID;
}

namespace {
std::set<variant*> callable_variants_loading, delayed_variants_loading;

std::vector<CallStackEntry> call_stack;

variant last_failed_query_map, last_failed_query_key;
variant last_query_map;
variant UnfoundInMapNullVariant;
}

void init_call_stack(int min_size)
{
	call_stack.reserve(min_size);
}

void swap_variants_loading(std::set<variant*>& v)
{
	callable_variants_loading.swap(v);
}

void push_call_stack(const game_logic::formula_expression* frame, const game_logic::formula_callable* callable)
{
	call_stack.resize(call_stack.size()+1);
	call_stack.back().expression = frame;
	call_stack.back().callable = callable;
	ASSERT_LOG(call_stack.size() < 4096, "FFL Recursion too deep (Exceeds 4096 frames)");
}

void pop_call_stack()
{
	call_stack.pop_back();
}

std::string get_call_stack()
{
	variant current_frame;
	std::string res;
	std::vector<CallStackEntry> reversed_call_stack = call_stack;
	std::reverse(reversed_call_stack.begin(), reversed_call_stack.end());
	for(std::vector<CallStackEntry>::const_iterator i = reversed_call_stack.begin(); i != reversed_call_stack.end(); ++i) {
		const game_logic::formula_expression* p = i->expression;
		if(p && p->parent_formula() != current_frame) {
			current_frame = p->parent_formula();
			const variant::debug_info* info = current_frame.get_debug_info();
			if(!info) {
				res += "(UNKNOWN LOCATION) (" + current_frame.write_json() + "\n";
			} else {
				res += p->debug_pinpoint_location() + "\n";
			}
		}
	}

	return res;
}

const std::vector<CallStackEntry>& get_expression_call_stack()
{
	return call_stack;
}

std::string get_full_call_stack()
{
	std::string res;
	for(std::vector<CallStackEntry>::const_iterator i = call_stack.begin();
	    i != call_stack.end(); ++i) {
		if(!i->expression) {
			continue;
		}
		res += formatter() << "  FRAME " << (i - call_stack.begin()) << ": " << i->expression->str() << "\n";
	}
	return res;
}

std::string output_formula_error_info();

namespace {
void generate_error(std::string message)
{
	if(call_stack.empty() == false && call_stack.back().expression) {
		message += "\n" + call_stack.back().expression->debug_pinpoint_location();
	}

	std::ostringstream s;
	s << "ERROR: " << message << "\n" << get_call_stack();
	s << output_formula_error_info();

	ASSERT_LOG(false, s.str() + "\ntype error");
}

}

type_error::type_error(const std::string& str) : message(str) {
	if(call_stack.empty() == false && call_stack.back().expression) {
		message += "\n" + call_stack.back().expression->debug_pinpoint_location();
	}

	std::cerr << "ERROR: " << message << "\n" << get_call_stack();
	std::cerr << output_formula_error_info();
}

VariantFunctionTypeInfo::VariantFunctionTypeInfo() : num_unneeded_args(0)
{}

struct variant_list {

	variant_list() : begin(elements.begin()), end(elements.end()),
	                 refcount(0), storage(NULL)
	{}

	variant_list(const variant_list& o) :
	   elements(o.begin, o.end), begin(elements.begin()), end(elements.end()),
	   refcount(1), storage(NULL)
	{}

	const variant_list& operator=(const variant_list& o) {
		elements.assign(o.begin, o.end),
		begin = elements.begin();
		end = elements.end();
		storage = NULL;
		return *this;
	}

	~variant_list() {
		if(storage && --storage->refcount == 0) {
			delete storage;
		}
	}

	size_t size() const { return end - begin; }

	variant::debug_info info;
	boost::intrusive_ptr<const game_logic::formula_expression> expression;
	std::vector<variant> elements;
	std::vector<variant>::iterator begin, end;
	int refcount;
	variant_list* storage;
};

struct variant_string {
	variant::debug_info info;
	boost::intrusive_ptr<const game_logic::formula_expression> expression;

	variant_string() : refcount(0)
	{}
	variant_string(const variant_string& o) : str(o.str), translated_from(o.translated_from), refcount(1)
	{}
	std::string str, translated_from;
	int refcount;

	std::vector<const game_logic::formula*> formulae_using_this;

	private:
	void operator=(const variant_string&);
};

struct variant_map {
	variant::debug_info info;
	boost::intrusive_ptr<const game_logic::formula_expression> expression;

	variant_map() : refcount(0), modcount(0)
	{}
	variant_map(const variant_map& o) : expression(o.expression), elements(o.elements), refcount(1), modcount(0)
	{}

	std::map<variant,variant> elements;
	int refcount;
	int modcount;
private:
	void operator=(const variant_map&);
};

struct variant_fn {
	variant::debug_info info;

	variant_fn() : refcount(0)
	{}

	VariantFunctionTypeInfoPtr type;

	std::function<variant(const game_logic::formula_callable&)> builtin_fn;
	game_logic::const_formula_ptr fn;
	game_logic::const_formula_callable_ptr callable;

	std::vector<variant> bound_args;

	int base_slot;
	int refcount;
};

struct variant_generic_fn {
	variant::debug_info info;

	variant_generic_fn() : refcount(0)
	{}

	VariantFunctionTypeInfoPtr type;

	variant fn;
	std::vector<std::string> generic_types;
	game_logic::const_formula_callable_ptr callable;

	std::vector<variant> bound_args;

	std::function<game_logic::const_formula_ptr(const std::vector<variant_type_ptr>&)> factory;

	mutable std::map<std::vector<std::string>, variant> cache;

	int base_slot;
	int refcount;
};

struct variant_multi_fn {
	variant_multi_fn() : refcount(0)
	{}

	int refcount;

	std::vector<variant> functions;
};

struct variant_delayed {
variant_delayed() : has_result(false), refcount(0)
{}

void calculate_result() {
if(!has_result) {
	if(callable) {
		result = fn->execute(*callable);
	} else {
		result = fn->execute();
	}

	has_result = true;
}
}

game_logic::const_formula_ptr fn;
game_logic::const_formula_callable_ptr callable;

bool has_result;
variant result;

int refcount;
};

struct variant_weak {

variant_weak() : refcount(0)
{}

int refcount;
ffl::weak_ptr<game_logic::formula_callable> ptr;

};

void variant::increment_refcount()
{
switch(type_) {
case VARIANT_TYPE_LIST:
++list_->refcount;
break;
case VARIANT_TYPE_STRING:
++string_->refcount;
break;
case VARIANT_TYPE_MAP:
++map_->refcount;
break;
case VARIANT_TYPE_CALLABLE:
intrusive_ptr_add_ref(callable_);
break;
case VARIANT_TYPE_CALLABLE_LOADING:
callable_variants_loading.insert(this);
break;
case VARIANT_TYPE_FUNCTION:
++fn_->refcount;
break;
case VARIANT_TYPE_GENERIC_FUNCTION:
++generic_fn_->refcount;
break;
case VARIANT_TYPE_MULTI_FUNCTION:
++multi_fn_->refcount;
break;
case VARIANT_TYPE_DELAYED:
delayed_variants_loading.insert(this);
++delayed_->refcount;
break;
case VARIANT_TYPE_WEAK:
++weak_->refcount;
break;

// These are not used here, add them to silence a compiler warning.
case VARIANT_TYPE_NULL:
case VARIANT_TYPE_INT:
case VARIANT_TYPE_BOOL:
case VARIANT_TYPE_DECIMAL:
case VARIANT_TYPE_INVALID:
break;
}
}

void variant::release()
{
switch(type_) {
case VARIANT_TYPE_LIST:
if(--list_->refcount == 0) {
	delete list_;
}
break;
case VARIANT_TYPE_STRING:
if(--string_->refcount == 0) {
	delete string_;
}
break;
case VARIANT_TYPE_MAP:
if(--map_->refcount == 0) {
	delete map_;
}
break;
case VARIANT_TYPE_CALLABLE:
intrusive_ptr_release(callable_);
break;
case VARIANT_TYPE_CALLABLE_LOADING:
callable_variants_loading.erase(this);
break;
case VARIANT_TYPE_FUNCTION:
if(--fn_->refcount == 0) {
	delete fn_;
}
break;
case VARIANT_TYPE_GENERIC_FUNCTION:
if(--generic_fn_->refcount == 0) {
	delete generic_fn_;
}
break;
case VARIANT_TYPE_MULTI_FUNCTION:
if(--multi_fn_->refcount == 0) {
	delete multi_fn_;
}
break;
case VARIANT_TYPE_DELAYED:
delayed_variants_loading.erase(this);
if(--delayed_->refcount == 0) {
	delete delayed_;
}
break;
case VARIANT_TYPE_WEAK:
if(--weak_->refcount == 0) {
	delete weak_;
}
break;

// These are not used here, add them to silence a compiler warning.
case VARIANT_TYPE_NULL:
case VARIANT_TYPE_INT:
case VARIANT_TYPE_BOOL:
case VARIANT_TYPE_DECIMAL:
case VARIANT_TYPE_INVALID:
break;
}
}

const game_logic::formula_expression* variant::get_source_expression() const
{
	switch(type_) {
	case VARIANT_TYPE_LIST:
	case VARIANT_TYPE_STRING:
	case VARIANT_TYPE_MAP:
		return map_->expression.get();
	default:
		break;
	}

	return NULL;
}

void variant::set_source_expression(const game_logic::formula_expression* expr)
{
	switch(type_) {
	case VARIANT_TYPE_LIST:
	case VARIANT_TYPE_STRING:
	case VARIANT_TYPE_MAP:
		map_->expression.reset(expr);
		break;
	default:
		break;
	}
}

void variant::set_debug_info(const debug_info& info)
{
	switch(type_) {
	case VARIANT_TYPE_LIST:
	case VARIANT_TYPE_STRING:
	case VARIANT_TYPE_MAP:
		*debug_info_ = info;
		break;
	default:
		break;
	}
}

const variant::debug_info* variant::get_debug_info() const
{
	switch(type_) {
	case VARIANT_TYPE_LIST:
	case VARIANT_TYPE_STRING:
	case VARIANT_TYPE_MAP:
		if(debug_info_->filename) {
			return debug_info_;
		}
		break;
	default:
		break;
	}

	return NULL;
}

std::string variant::debug_location() const
{
	const variant::debug_info* info = get_debug_info();
	if(!info) {
		return "(unknown location)";
	} else {
		return info->message();
	}
}

variant variant::create_delayed(game_logic::const_formula_ptr f, game_logic::const_formula_callable_ptr callable)
{
	variant v;
	v.type_ = VARIANT_TYPE_DELAYED;
	v.delayed_ = new variant_delayed;
	v.delayed_->fn = f;
	v.delayed_->callable = callable;

	v.increment_refcount();

	return v;
}

void variant::resolve_delayed()
{
	std::set<variant*> items = delayed_variants_loading;
	foreach(variant* v, items) {
		v->delayed_->calculate_result();
		variant res = v->delayed_->result;
		*v = res;
	}

	delayed_variants_loading.clear();
}

variant variant::create_function_overload(const std::vector<variant>& fn)
{
	variant result;
	result.type_ = VARIANT_TYPE_MULTI_FUNCTION;
	result.multi_fn_ = new variant_multi_fn;
	result.multi_fn_->functions = fn;
	result.increment_refcount();
	return result;
}

variant::variant(const game_logic::formula_callable* callable)
	: type_(VARIANT_TYPE_CALLABLE), callable_(callable)
{
	if(callable == NULL) {
		type_ = VARIANT_TYPE_NULL;
		return;
	}
	increment_refcount();
}

variant::variant(std::vector<variant>* array)
    : type_(VARIANT_TYPE_LIST)
{
	assert(array);
	list_ = new variant_list;
	list_->elements.swap(*array);
	list_->begin = list_->elements.begin();
	list_->end = list_->elements.end();
	increment_refcount();
}

variant::variant(const char* s)
   : type_(VARIANT_TYPE_STRING)
{
	if(s == NULL) {
		type_ = VARIANT_TYPE_NULL;
		return;
	}
	string_ = new variant_string;
	string_->str = std::string(s);
	increment_refcount();
}

variant::variant(const std::string& str)
	: type_(VARIANT_TYPE_STRING)
{
	string_ = new variant_string;
	string_->str = str;
	increment_refcount();
}

variant variant::create_translated_string(const std::string& str)
{
	return create_translated_string(str, i18n::tr(str));
}

variant variant::create_translated_string(const std::string& str, const std::string& translation)
{
	variant v(translation);
	v.string_->translated_from = str;
	return v;
}

variant::variant(std::map<variant,variant>* map)
    : type_(VARIANT_TYPE_MAP)
{
	for(std::map<variant, variant>::const_iterator i = map->begin(); i != map->end(); ++i) {
		if(i->first.is_bool()) {
			std::cerr << "VALUE: " << i->second.to_debug_string() << "\n";
			assert(false);
		}
	}

	assert(map);
	map_ = new variant_map;
	map_->elements.swap(*map);
	increment_refcount();
}

variant::variant(const variant& formula_var, const game_logic::formula_callable& callable, int base_slot, const VariantFunctionTypeInfoPtr& type_info, const std::vector<std::string>& generic_types, std::function<game_logic::const_formula_ptr(const std::vector<variant_type_ptr>&)> factory)
	: type_(VARIANT_TYPE_GENERIC_FUNCTION)
{
	generic_fn_ = new variant_generic_fn;
	generic_fn_->fn = formula_var;
	generic_fn_->callable = &callable;
	generic_fn_->base_slot = base_slot;
	generic_fn_->type = type_info;
	generic_fn_->generic_types = generic_types;
	generic_fn_->factory = factory;

	increment_refcount();

	if(formula_var.get_debug_info()) {
		set_debug_info(*formula_var.get_debug_info());
	}
}

variant::variant(const game_logic::const_formula_ptr& formula, const game_logic::formula_callable& callable, int base_slot, const VariantFunctionTypeInfoPtr& type_info)
  : type_(VARIANT_TYPE_FUNCTION)
{
	fn_ = new variant_fn;
	fn_->fn = formula;
	fn_->callable = &callable;
	fn_->base_slot = base_slot;
	fn_->type = type_info;

	ASSERT_EQ(fn_->type->variant_types.size(), fn_->type->arg_names.size());

	increment_refcount();

	if(formula->str_var().get_debug_info()) {
		set_debug_info(*formula->str_var().get_debug_info());
	}
}

variant::variant(std::function<variant(const game_logic::formula_callable&)> builtin_fn, const VariantFunctionTypeInfoPtr& type_info)
  : type_(VARIANT_TYPE_FUNCTION)
{
	fn_ = new variant_fn;
	fn_->builtin_fn = builtin_fn;
	fn_->base_slot = 0;
	fn_->type = type_info;

	ASSERT_EQ(fn_->type->variant_types.size(), fn_->type->arg_names.size());

	increment_refcount();
}

/*
variant::variant(game_logic::const_formula_ptr fml, const std::vector<std::string>& args, const game_logic::formula_callable& callable, int base_slot, const std::vector<variant>& default_args, const std::vector<variant_type_ptr>& variant_types, const variant_type_ptr& return_type)
  : type_(VARIANT_TYPE_FUNCTION)
{
	fn_ = new variant_fn;
	fn_->type->arg_names = args;
	fn_->base_slot = base_slot;
	fn_->fn = fml;
	fn_->callable = &callable;
	fn_->type->default_args = default_args;
	fn_->variant_types = variant_types;

	ASSERT_EQ(fn_->variant_types.size(), fn_->type->arg_names.size());

	fn_->return_type = return_type;
	increment_refcount();

	if(fml->str_var().get_debug_info()) {
		set_debug_info(*fml->str_var().get_debug_info());
	}
}
*/

const variant& variant::operator=(const variant& v)
{
	if(&v != this) {
		if(type_ > VARIANT_TYPE_INT) {
			release();
		}

		type_ = v.type_;
		value_ = v.value_;
		if(type_ > VARIANT_TYPE_INT) {
			increment_refcount();
		}
	}
	return *this;
}

const variant& variant::operator[](size_t n) const
{
	if(type_ == VARIANT_TYPE_CALLABLE) {
		assert(n == 0);
		return *this;
	}

	must_be(VARIANT_TYPE_LIST);
	assert(list_);
	if(n >= list_->size()) {
		generate_error(formatter() << "invalid index of " << n << " into " << write_json());
	}

	return list_->begin[n];
}

const variant& variant::operator[](const variant v) const
{
	if(type_ == VARIANT_TYPE_CALLABLE) {
		assert(v.as_int() == 0);
		return *this;
	}

	if(type_ == VARIANT_TYPE_MAP) {
		assert(map_);
		std::map<variant,variant>::const_iterator i = map_->elements.find(v);
		if (i == map_->elements.end())
		{
			last_failed_query_map = *this;
			last_failed_query_key = v;

			return UnfoundInMapNullVariant;
		}

		last_query_map = *this;
		return i->second;
	} else if(type_ == VARIANT_TYPE_LIST) {
		return operator[](v.as_int());
	} else {
		const debug_info* info = get_debug_info();
		std::string loc;
		if(info) {
			loc = formatter() << " at " << *info->filename << " " << info->line << " (column " << info->column << ")\n";
		}
		generate_error(formatter() << "type error: " << " expected a list or a map but found " << variant_type_to_string(type_) << " (" << write_json() << ") " << loc);
		return *this;
	}	
}

const variant& variant::operator[](const std::string& key) const
{
	return (*this)[variant(key)];
}

bool variant::has_key(const variant& key) const
{
	if(type_ != VARIANT_TYPE_MAP) {
		return false;
	}

	std::map<variant,variant>::const_iterator i = map_->elements.find(key);
	if(i != map_->elements.end() && i->second.is_null() == false) {
		return true;
	} else {
		return false;
	}
}

bool variant::has_key(const std::string& key) const
{
	return has_key(variant(key));
}

variant variant::get_keys() const
{
	must_be(VARIANT_TYPE_MAP);
	assert(map_);
	std::vector<variant> tmp;
	for(std::map<variant,variant>::const_iterator i=map_->elements.begin(); i != map_->elements.end(); ++i) {
			tmp.push_back(i->first);
	}
	return variant(&tmp);
}

variant variant::get_values() const
{
	must_be(VARIANT_TYPE_MAP);
	assert(map_);
	std::vector<variant> tmp;
	for(std::map<variant,variant>::const_iterator i=map_->elements.begin(); i != map_->elements.end(); ++i) {
			tmp.push_back(i->second);
	}
	return variant(&tmp);
}

size_t variant::num_elements() const
{
	if (type_ == VARIANT_TYPE_NULL){
		return 0;
	} else if(type_ == VARIANT_TYPE_CALLABLE) {
		return 1;
	} else if (type_ == VARIANT_TYPE_LIST) {
		assert(list_);
		return list_->size();
	} else if (type_ == VARIANT_TYPE_STRING) {
		assert(string_);
		return string_->str.size();
	} else if (type_ == VARIANT_TYPE_MAP) {
		assert(map_);
		return map_->elements.size();
	} else {
		const debug_info* info = get_debug_info();
		std::string loc;
		if(info) {
			loc = formatter() << " at " << *info->filename << " " << info->line << " (column " << info->column << ")\n";
		}
		generate_error(formatter() << "type error: " << " expected a list or a map but found " << variant_type_to_string(type_) << " (" << write_json() << ")" << loc);
		return 0;
	}
}

variant variant::get_list_slice(int begin, int end) const
{
	std::vector<variant> items;
	variant result(&items);
	if(end <= begin) {
		return result;
	}

	must_be(VARIANT_TYPE_LIST);

	if(begin < 0 || end > list_->size()) {
		generate_error(formatter() << "ILLEGAL INDEX INTO LIST WHEN SLICING: " << begin << ", " << end << " / " << list_->size());
	}

	result.list_->begin = list_->begin + begin;
	result.list_->end = list_->begin + end;
	result.list_->storage = list_;
	list_->refcount++;

	return result;
}

bool variant::function_call_valid(const std::vector<variant>& passed_args, std::string* message, bool allow_partial) const
{
	if(type_ == VARIANT_TYPE_MULTI_FUNCTION) {
		foreach(const variant& v, multi_fn_->functions) {
			if(v.function_call_valid(passed_args)) {
				return true;
			}
		}

		if(message) {
			*message = "Arguments do not match any overloaded functions";
		}

		return false;
	}

	if(type_ != VARIANT_TYPE_FUNCTION) {
		if(message) {
			*message = "Not a function";
		}
		return false;
	}

	std::vector<variant> args_buf;

	if(fn_->bound_args.empty() == false) {
		args_buf = fn_->bound_args;
		args_buf.insert(args_buf.end(), passed_args.begin(), passed_args.end());
	}

	const std::vector<variant>& args = args_buf.empty() ? passed_args : args_buf;

	const int max_args = fn_->type->arg_names.size();
	const int min_args = max_args - fn_->type->num_default_args();

	if(args.size() > max_args || (args.size() < min_args && !allow_partial)) {
		if(message) {
			*message = "Incorrect number of arguments to function";
		}
		return false;
	}

	for(int n = 0; n != args.size(); ++n) {
		if(n < fn_->type->variant_types.size() && fn_->type->variant_types[n]) {
			if(fn_->type->variant_types[n]->match(args[n]) == false) {
				if(message) {
					*message = formatter() << "Argument " << (n+1) << " does not match. Expects " << fn_->type->variant_types[n]->to_string() << " but found " << args[n].write_json();
				}
				return false;
			}
		}
	}

	return true;
}

variant variant::operator()(const std::vector<variant>& passed_args) const
{
	if(type_ == VARIANT_TYPE_MULTI_FUNCTION) {
		foreach(const variant& v, multi_fn_->functions) {
			if(v.function_call_valid(passed_args)) {
				return v(passed_args);
			}
		}

		int narg = 1;
		std::ostringstream msg;
		for(variant arg : passed_args) {
			msg << "Argument " << narg << ": " << arg.write_json() << " Type: " << get_variant_type_from_value(arg)->to_string() << "\n";
			++narg;
		}

		msg << "\nPossible functions:\n";

		foreach(const variant& v, multi_fn_->functions) {
			msg << "  args: ";
			for(variant_type_ptr type : v.fn_->type->variant_types) {
				msg << type->to_string() << ", ";
			}

			msg << "\n";
		}


		generate_error(formatter() << "Function overload has no matches to arguments: \n" << msg.str());
		return variant();
	}

	must_be(VARIANT_TYPE_FUNCTION);

	std::vector<variant> args_buf;
	if(fn_->bound_args.empty() == false) {
		args_buf = fn_->bound_args;
		args_buf.insert(args_buf.end(), passed_args.begin(), passed_args.end());
	}

	const std::vector<variant>* args = args_buf.empty() ? &passed_args : &args_buf;

	boost::intrusive_ptr<game_logic::slot_formula_callable> callable = new game_logic::slot_formula_callable;
	if(fn_->callable) {
		callable->set_fallback(fn_->callable);
	}

	callable->set_base_slot(fn_->base_slot);

	const int max_args = fn_->type->arg_names.size();
	const int min_args = max_args - fn_->type->num_default_args();

	if(args->size() < min_args || args->size() > max_args) {
		std::ostringstream str;
		for(std::vector<std::string>::const_iterator a = fn_->type->arg_names.begin(); a != fn_->type->arg_names.end(); ++a) {
			if(a != fn_->type->arg_names.begin()) {
				str << ", ";
			}

			str << *a;
		}
		generate_error(formatter() << "Function passed " << args->size() << " arguments, between " <<  min_args << " and " << max_args << " expected (" << str.str() << ")");
	}

	for(size_t n = 0; n != args->size(); ++n) {
		if(n < fn_->type->variant_types.size() && fn_->type->variant_types[n]) {
	//		if((*args)[n].is_map() && fn_->type->variant_types[n]->is_class(NULL))
			if(fn_->type->variant_types[n]->match((*args)[n]) == false) {
				std::string class_name;
				if((*args)[n].is_map() && fn_->type->variant_types[n]->is_class(&class_name)) {
					//auto-construct an object from a map in a function argument
					game_logic::formula::fail_if_static_context();

					boost::intrusive_ptr<game_logic::formula_object> obj(game_logic::formula_object::create(class_name, (*args)[n]));

					args_buf = *args;
					args = &args_buf;

					args_buf[n] = variant(obj.get());

				} else if(const game_logic::formula_interface* interface = fn_->type->variant_types[n]->is_interface()) {
					if((*args)[n].is_map() == false && (*args)[n].is_callable() == false) {
						generate_error((formatter() << "FUNCTION ARGUMENT " << (n+1) << " EXPECTED INTERFACE " << fn_->type->variant_types[n]->str() << " BUT FOUND " << (*args)[n].write_json()).str());
					}

					variant obj = interface->get_dynamic_factory()->create((*args)[n]);

					args_buf = *args;
					args = &args_buf;

					args_buf[n] = obj;

				} else {
					variant_type_ptr arg_type = get_variant_type_from_value((*args)[n]);
					generate_error((formatter() << "FUNCTION ARGUMENT " << (n+1) << " EXPECTED TYPE " << fn_->type->variant_types[n]->str() << " BUT FOUND " << (*args)[n].write_json() << " of type " << arg_type->to_string()).str());
				}
			}
		}

		callable->add((*args)[n]);
	}

	for(size_t n = args->size(); n < max_args && (n - min_args) < fn_->type->default_args.size(); ++n) {
		callable->add(fn_->type->default_args[n - min_args]);
	}

	if(fn_->fn) {
		const variant result = fn_->fn->execute(*callable);
		if(fn_->type->return_type && !fn_->type->return_type->match(result)) {
			call_stack_manager scope(fn_->fn->expr().get(), callable.get());
			generate_error(formatter() << "Function returned incorrect type, expecting " << fn_->type->return_type->to_string() << " but found " << result.write_json() << " (type: " << get_variant_type_from_value(result)->to_string() << ") FOR " << fn_->fn->str());
		}
		return result;
	} else {
		return fn_->builtin_fn(*callable);
	}
}

variant variant::instantiate_generic_function(const std::vector<variant_type_ptr>& args) const
{
	must_be(VARIANT_TYPE_GENERIC_FUNCTION);

	ASSERT_LOG(args.size() == generic_fn_->generic_types.size(), "Expected " << generic_fn_->generic_types.size() << " generic arguments but found " << args.size());

	std::vector<std::string> key;
	key.resize(args.size());
	for(int n = 0; n != args.size(); ++n) {
		key[n] = args[n]->to_string();
	}

	auto itor = generic_fn_->cache.find(key);
	if(itor != generic_fn_->cache.end()) {
		return itor->second;
	}

	std::map<std::string, variant_type_ptr> mapping;
	for(int n = 0; n != args.size(); ++n) {
		mapping[generic_fn_->generic_types[n]] = args[n];
	}

	VariantFunctionTypeInfoPtr info(new VariantFunctionTypeInfo(*generic_fn_->type));
	for(variant_type_ptr& type : info->variant_types) {
		if(!type) {
			continue;
		}
		variant_type_ptr result = type->map_generic_types(mapping);
		if(result) {
			type = result;
		}
	}

	if(info->return_type) {
		variant_type_ptr new_return_type = info->return_type->map_generic_types(mapping);
		if(new_return_type) {
			info->return_type = new_return_type;
		}
	}

	game_logic::const_formula_ptr fml = generic_fn_->factory(args);
	variant result(fml, *generic_fn_->callable, generic_fn_->base_slot, info);
	generic_fn_->cache[key] = result;
	return result;
}

variant variant::get_member(const std::string& str) const
{
	if(is_callable()) {
		return callable_->query_value(str);
	} else if(is_map()) {
		return (*this)[str];
	}

	if(str == "self") {
		return *this;
	} else {
		return variant();
	}
}

bool variant::as_bool(bool default_value) const
{
	switch(type_) {
	case VARIANT_TYPE_INT: return int_value_ != 0;
	case VARIANT_TYPE_BOOL: return bool_value_;
	default: return default_value;
	}
}

bool variant::as_bool() const
{
	bool default_value = false;
	switch(type_) {
	case VARIANT_TYPE_NULL:
		return default_value;
	case VARIANT_TYPE_BOOL:
		return bool_value_;
	case VARIANT_TYPE_INT:
		return int_value_ != 0;
	case VARIANT_TYPE_DECIMAL:
		return decimal_value_ != 0;
	case VARIANT_TYPE_CALLABLE_LOADING:
		return true;
	case VARIANT_TYPE_CALLABLE:
		return callable_ != NULL;
	case VARIANT_TYPE_LIST:
		return list_->size() != 0;
	case VARIANT_TYPE_MAP:
		return !map_->elements.empty();
	case VARIANT_TYPE_STRING:
		return !string_->str.empty();
	case VARIANT_TYPE_FUNCTION:
		return true;
	default:
		assert(false);
		return false;
	}
}

std::vector<variant> variant::as_list() const
{
	if(is_list()) {
		if(list_->elements.empty() == false) {
			return list_->elements;
		} else {
			return std::vector<variant>(list_->begin, list_->end);
		}
	} else if(is_null()) {
		return std::vector<variant>();
	} else {
		std::vector<variant> v;
		v.push_back(*this);
		return v;
	}
}

std::vector<std::string> variant::as_list_string() const
{
	std::vector<std::string> result;
	must_be(VARIANT_TYPE_LIST);
	result.reserve(list_->size());
	for(int n = 0; n != list_->size(); ++n) {
		list_->begin[n].must_be(VARIANT_TYPE_STRING);
		result.push_back(list_->begin[n].as_string());
	}

	return result;
}

std::vector<std::string> variant::as_list_string_optional() const
{
	if(is_null()) {
		return std::vector<std::string>();
	}
	
	if(is_string()) {
		std::vector<std::string> res;
		res.push_back(as_string());
		return res;
	}

	return as_list_string();
}

std::vector<int> variant::as_list_int() const
{
	std::vector<int> result;
	must_be(VARIANT_TYPE_LIST);
	result.reserve(list_->size());
	for(int n = 0; n != list_->size(); ++n) {
		result.push_back(list_->begin[n].as_int());
	}

	return result;
}

std::vector<decimal> variant::as_list_decimal() const
{
	std::vector<decimal> result;
	must_be(VARIANT_TYPE_LIST);
	result.reserve(list_->size());
	for(int n = 0; n != list_->size(); ++n) {
		result.push_back(list_->begin[n].as_decimal());
	}

	return result;
}

const std::map<variant,variant>& variant::as_map() const
{
	if(is_map()) {
		return map_->elements;
	} else {
		static const std::map<variant,variant>* EmptyMap = new std::map<variant,variant>;
		return *EmptyMap;
	}
}

bool variant::is_unmodified_single_reference() const
{
	if(is_map()) {
		if(map_->refcount > 1 || map_->modcount > 0) {
			return false;
		}

		for(const auto& p : map_->elements) {
			if(!p.first.is_unmodified_single_reference()) {
				return false;
			}

			if(!p.second.is_unmodified_single_reference()) {
				return false;
			}
		}

	} else if(is_list()) {
		if(list_->refcount > 1) {
			return false;
		}
		for(auto i = list_->begin; i != list_->end; ++i) {
			if(!i->is_unmodified_single_reference()) {
				return false;
			}
		}
	}

	return true;
}

variant variant::add_attr(variant key, variant value)
{
	last_query_map = variant();

	if(is_map()) {
		if(map_->refcount > 1) {
			map_->refcount--;
			map_ = new variant_map(*map_);
			map_->refcount = 1;
		}

		make_unique();
		map_->elements[key] = value;
		return *this;
	} else {
		return variant();
	}
}

variant variant::remove_attr(variant key)
{
	last_query_map = variant();

	if(is_map()) {
		if(map_->refcount > 1) {
			map_->refcount--;
			map_ = new variant_map(*map_);
			map_->refcount = 1;
		}

		make_unique();
		map_->elements.erase(key);
		return *this;
	} else {
		return variant();
	}
}

void variant::add_attr_mutation(variant key, variant value)
{
	if(is_map()) {
		map_->elements[key] = value;
		map_->modcount++;
	}
}

void variant::remove_attr_mutation(variant key)
{
	if(is_map()) {
		map_->elements.erase(key);
		map_->modcount++;
	}
}

variant* variant::get_attr_mutable(variant key)
{
	if(is_map()) {
		std::map<variant,variant>::iterator i = map_->elements.find(key);
		if(i != map_->elements.end()) {
			map_->modcount++;
			return &i->second;
		}
	}

	return NULL;
}

variant* variant::get_index_mutable(int index)
{
	if(is_list()) {
		if(index >= 0 && index < list_->size()) {
			return &list_->begin[index];
		}
	}

	return NULL;
}

void variant::weaken()
{
	if(type_ == VARIANT_TYPE_CALLABLE) {
		variant_weak* weak = new variant_weak;
		weak->refcount++;
		weak->ptr = ffl::weak_ptr<game_logic::formula_callable>(mutable_callable_);

		release();
		type_ = VARIANT_TYPE_WEAK;
		weak_ = weak;
	}
}

void variant::strengthen()
{
	if(is_weak()) {
		*this = variant(weak_->ptr.get());
	}
}

variant variant::bind_closure(const game_logic::formula_callable* callable)
{
	must_be(VARIANT_TYPE_FUNCTION);

	variant result;
	result.type_ = VARIANT_TYPE_FUNCTION;
	result.fn_ = new variant_fn(*fn_);
	result.fn_->refcount = 1;
	result.fn_->callable.reset(callable);
	return result;
}

variant variant::bind_args(const std::vector<variant>& args)
{
	must_be(VARIANT_TYPE_FUNCTION);

	std::string msg;
	ASSERT_LOG(function_call_valid(args, &msg, true), "Invalid argument binding: " << msg);
	
	variant result;
	result.type_ = VARIANT_TYPE_FUNCTION;
	result.fn_ = new variant_fn(*fn_);
	result.fn_->refcount = 1;
	result.fn_->bound_args.insert(result.fn_->bound_args.end(), args.begin(), args.end());

	return result;
}

void variant::get_mutable_closure_ref(std::vector<boost::intrusive_ptr<const game_logic::formula_callable>*>& result)
{
	if(type_ == VARIANT_TYPE_MULTI_FUNCTION) {
		for(int n = 0; n != multi_fn_->functions.size(); ++n) {
			multi_fn_->functions[n].get_mutable_closure_ref(result);
		}

		return;
	}

	must_be(VARIANT_TYPE_FUNCTION);
	if(fn_->callable) {
		result.push_back(&fn_->callable);
	}
}

int variant::min_function_arguments() const
{
	if(type_ == VARIANT_TYPE_MULTI_FUNCTION) {
		int result = -1;
		for(int n = 0; n != multi_fn_->functions.size(); ++n) {
			const int value = multi_fn_->functions[n].min_function_arguments();
			if(value < result || result == -1) {
				result = value;
			}
		}

		return result;
	} else if(type_ == VARIANT_TYPE_GENERIC_FUNCTION) {
		return std::max<int>(0, generic_fn_->type->arg_names.size() - generic_fn_->type->num_default_args() - static_cast<int>(generic_fn_->bound_args.size()));
	}
	
	must_be(VARIANT_TYPE_FUNCTION);
	return std::max<int>(0, fn_->type->arg_names.size() - fn_->type->num_default_args() - static_cast<int>(fn_->bound_args.size()));
}

int variant::max_function_arguments() const
{
	if(type_ == VARIANT_TYPE_MULTI_FUNCTION) {
		int result = -1;
		for(int n = 0; n != multi_fn_->functions.size(); ++n) {
			const int value = multi_fn_->functions[n].max_function_arguments();
			if(value > result || result == -1) {
				result = value;
			}
		}

		return result;
	} else if(type_ == VARIANT_TYPE_GENERIC_FUNCTION) {
		return generic_fn_->type->arg_names.size() - generic_fn_->bound_args.size();
	}

	must_be(VARIANT_TYPE_FUNCTION);
	return fn_->type->arg_names.size() - fn_->bound_args.size();
}

variant_type_ptr variant::function_return_type() const
{
	if(type_ == VARIANT_TYPE_MULTI_FUNCTION) {
		std::vector<variant_type_ptr> result;
		for(int n = 0; n != multi_fn_->functions.size(); ++n) {
			result.push_back(multi_fn_->functions[n].function_return_type());
		}

		return variant_type::get_union(result);
	} else if(type_ == VARIANT_TYPE_GENERIC_FUNCTION) {
		return generic_fn_->type->return_type;
	}
	
	must_be(VARIANT_TYPE_FUNCTION);
	return fn_->type->return_type;
}

std::vector<variant_type_ptr> variant::function_arg_types() const
{
	if(type_ == VARIANT_TYPE_MULTI_FUNCTION) {
		std::vector<std::vector<variant_type_ptr> > result;
		for(int n = 0; n != multi_fn_->functions.size(); ++n) {
			std::vector<variant_type_ptr> types = multi_fn_->functions[n].function_arg_types();
			for(int m = 0; m != types.size(); ++m) {
				if(result.size() <= m) {
					result.resize(m+1);
				}

				result[m].push_back(types[m]);
			}
		}

		std::vector<variant_type_ptr> res;
		for(auto item : result) {
			res.push_back(variant_type::get_union(item));
		}

		return res;
	} else if(type_ == VARIANT_TYPE_GENERIC_FUNCTION) {
		std::vector<variant_type_ptr> result = generic_fn_->type->variant_types;
		return result;
	}

	must_be(VARIANT_TYPE_FUNCTION);
	std::vector<variant_type_ptr> result = fn_->type->variant_types;
	if(fn_->bound_args.empty() == false) {
		ASSERT_LOG(fn_->bound_args.size() <= fn_->type->variant_types.size(), "INVALID FUNCTION BINDING: " << fn_->bound_args.size() << "/" << fn_->type->variant_types.size());
		result.erase(result.begin(), result.begin() + fn_->bound_args.size());
	}

	return result;
}

std::vector<std::string> variant::generic_function_type_args() const
{
	must_be(VARIANT_TYPE_GENERIC_FUNCTION);
	return generic_fn_->generic_types;
}

std::string variant::as_string_default(const char* default_value) const
{
	if(is_null()) {
		if(default_value) {
			return std::string(default_value);
		} else {
			return std::string();
		}
	}

	return as_string();
}

const std::string& variant::as_string() const
{
	must_be(VARIANT_TYPE_STRING);
	assert(string_);
	return string_->str;
}

variant variant::operator+(const variant& v) const
{
	if(type_ == VARIANT_TYPE_INT && v.type_ == VARIANT_TYPE_INT) {
		//strictly an optimization -- this is handled below, but the case
		//of adding two integers is the most common so we want it to be fast.
		return variant(int_value_ + v.int_value_);
	}

	if(type_ == VARIANT_TYPE_STRING) {
		if(v.type_ == VARIANT_TYPE_MAP) {
			return variant(as_string() + v.as_string());
		} else if(v.type_ == VARIANT_TYPE_STRING) {
			return variant(as_string() + v.as_string());
		}

		std::string s;
		v.serialize_to_string(s);
		return variant(as_string() + s);
	}

	if(v.type_ == VARIANT_TYPE_STRING) {
		std::string s;
		serialize_to_string(s);
		return variant(s + v.as_string());
	}
	if(type_ == VARIANT_TYPE_DECIMAL || v.type_ == VARIANT_TYPE_DECIMAL) {
		return variant(as_decimal() + v.as_decimal());
	}

	if(type_ == VARIANT_TYPE_INT) {
		return variant(int_value_ + v.as_int());
	}

	if(type_ == VARIANT_TYPE_BOOL) {
		return variant(as_int() + v.as_int());
	}

	if(type_ == VARIANT_TYPE_NULL) {
		return v;
	} else if(v.type_ == VARIANT_TYPE_NULL) {
		return *this;
	}

	if(type_ == VARIANT_TYPE_LIST) {
		if(v.type_ == VARIANT_TYPE_LIST) {
			const size_t new_size = list_->size() + v.list_->size();

			bool adopt_list = false;

			std::vector<variant> res;
			if(new_size <= list_->elements.capacity() && list_->storage == NULL) {
				res.swap(list_->elements);
				adopt_list = true;
			} else {
				res.reserve(new_size*2);
				for(size_t i = 0; i < list_->size(); ++i) {
					const variant& var = list_->begin[i];
					res.push_back(var);
				}
			}

			for(size_t j = 0; j < v.list_->size(); ++j) {
				const variant& var = v.list_->begin[j];
				res.push_back(var);
			}

			variant result(&res);
			if(adopt_list) {
				list_->storage = result.list_;
				result.list_->refcount++;
			}
			return result;
		}
	}
	if(type_ == VARIANT_TYPE_MAP) {
		if(v.type_ == VARIANT_TYPE_MAP) {
			std::map<variant,variant> res(map_->elements);

			for(std::map<variant,variant>::const_iterator i = v.map_->elements.begin(); i != v.map_->elements.end(); ++i) {
				res[i->first] = i->second;
			}

			return variant(&res);
		}
	}

	if(is_callable()) {
		game_logic::formula_object* obj = try_convert<game_logic::formula_object>();
		if(obj && v.is_map()) {
			boost::intrusive_ptr<game_logic::formula_object> new_obj(obj->clone());
			const std::map<variant,variant>& m = v.as_map();
			for(std::map<variant,variant>::const_iterator i = m.begin();
			    i != m.end(); ++i) {
				i->first.must_be(VARIANT_TYPE_STRING);
				new_obj->mutate_value(i->first.as_string(), i->second);
			}

			return variant(new_obj.get());
		}
	}

	ASSERT_LOG(false, "ILLEGAL ADDITION OF VARIANTS: " << write_json() << " + " << v.write_json());

	return variant(as_int() + v.as_int());
}

variant variant::operator-(const variant& v) const
{
	if(type_ == VARIANT_TYPE_DECIMAL || v.type_ == VARIANT_TYPE_DECIMAL) {
		return variant(as_decimal() - v.as_decimal());
	}

	return variant(as_int() - v.as_int());
}

variant variant::operator*(const variant& v) const
{
	if(type_ == VARIANT_TYPE_DECIMAL || v.type_ == VARIANT_TYPE_DECIMAL) {
		return variant(as_decimal() * v.as_decimal());
	}

	if(type_ == VARIANT_TYPE_LIST) {
		int ncopies = v.as_int();
		if(ncopies < 0) {
			ncopies *= -1;
		}
		std::vector<variant> res;
		res.reserve(list_->size()*ncopies);
		for(int n = 0; n != ncopies; ++n) {
			for(int m = 0; m != list_->size(); ++m) {
				res.push_back(list_->begin[m]);
			}
		}

		return variant(&res);
	}
	
	return variant(as_int() * v.as_int());
}

variant variant::operator/(const variant& v) const
{
	if(type_ == VARIANT_TYPE_DECIMAL || v.type_ == VARIANT_TYPE_DECIMAL) {
		if(v.as_decimal().value() == 0) {
			generate_error((formatter() << "divide by zero error").str());
		}

		return variant(as_decimal() / v.as_decimal());
	}

	const int numerator = as_int();
	const int denominator = v.as_int();
	if(denominator == 0) {
		generate_error(formatter() << "divide by zero error");
	}

	return variant(numerator/denominator);
}

variant variant::operator%(const variant& v) const
{
	const int numerator = as_int();
	const int denominator = v.as_int();
	if(denominator == 0) {
		generate_error(formatter() << "divide by zero error");
	}

	return variant(numerator%denominator);
}

variant variant::operator^(const variant& v) const
{
	if( type_ == VARIANT_TYPE_DECIMAL || v.type_ == VARIANT_TYPE_DECIMAL ) {
		double res = pow( as_decimal().value()/double(VARIANT_DECIMAL_PRECISION),
		                v.as_decimal().value()/double(VARIANT_DECIMAL_PRECISION));		
		res *= DECIMAL_PRECISION;
#if defined(TARGET_BLACKBERRY)
		return variant(static_cast<int64_t>(llround(res)), DECIMAL_VARIANT);
#else
		return variant(static_cast<int64_t>(res), DECIMAL_VARIANT);
#endif
	}

	return variant(static_cast<int>(pow(static_cast<double>(as_int()), v.as_int())));
}

variant variant::operator-() const
{
	if(type_ == VARIANT_TYPE_DECIMAL) {
		return variant(-decimal_value_, variant::DECIMAL_VARIANT);
	}

	return variant(-as_int());
}

bool variant::operator==(const variant& v) const
{
	if(type_ != v.type_) {
		if(type_ == VARIANT_TYPE_DECIMAL || v.type_ == VARIANT_TYPE_DECIMAL) {
			if(!is_numeric() && !is_null() || !v.is_numeric() && !v.is_null()) {
				return false;
			}

			return as_decimal() == v.as_decimal();
		}

		return false;
	}

	switch(type_) {
	case VARIANT_TYPE_NULL: {
		return v.is_null();
	}

	case VARIANT_TYPE_STRING: {
		return string_->str == v.string_->str;
	}

	case VARIANT_TYPE_BOOL: {
		return bool_value_ == v.bool_value_;
	}

	case VARIANT_TYPE_INT: {
		return int_value_ == v.int_value_;
	}

	case VARIANT_TYPE_DECIMAL: {
		return decimal_value_ == v.decimal_value_;
	}

	case VARIANT_TYPE_LIST: {
		if(num_elements() != v.num_elements()) {
			return false;
		}

		for(size_t n = 0; n != num_elements(); ++n) {
			if((*this)[n] != v[n]) {
				return false;
			}
		}

		return true;
	}

	case VARIANT_TYPE_MAP: {
		return map_->elements == v.map_->elements;
	}

	case VARIANT_TYPE_CALLABLE_LOADING: {
		return false;
	}

	case VARIANT_TYPE_CALLABLE: {
		return callable_->equals(v.callable_);
	}
	case VARIANT_TYPE_FUNCTION: {
		return fn_ == v.fn_;
	}
	case VARIANT_TYPE_GENERIC_FUNCTION: {
		return generic_fn_ == v.generic_fn_;
	}
	case VARIANT_TYPE_MULTI_FUNCTION: {
		return multi_fn_ == v.multi_fn_;
	}
	case VARIANT_TYPE_WEAK:
	case VARIANT_TYPE_DELAYED:
	case VARIANT_TYPE_INVALID:
		assert(false);
	}

	assert(false);
	return false;
}

bool variant::operator!=(const variant& v) const
{
	return !operator==(v);
}

bool variant::operator<=(const variant& v) const
{
	if(type_ != v.type_) {
		if(type_ == VARIANT_TYPE_DECIMAL && v.is_numeric() ||
		   v.type_ == VARIANT_TYPE_DECIMAL && is_numeric()) {
			return as_decimal() <= v.as_decimal();
		}

		return type_ < v.type_;
	}

	switch(type_) {
	case VARIANT_TYPE_NULL: {
		return true;
	}

	case VARIANT_TYPE_STRING: {
		return string_->str <= v.string_->str;
	}

	case VARIANT_TYPE_BOOL: {
		return bool_value_ <= v.bool_value_;
	}

	case VARIANT_TYPE_INT: {
		return int_value_ <= v.int_value_;
	}

	case VARIANT_TYPE_DECIMAL: {
		return decimal_value_ <= v.decimal_value_;
	}

	case VARIANT_TYPE_LIST: {
		for(size_t n = 0; n != num_elements() && n != v.num_elements(); ++n) {
			if((*this)[n] < v[n]) {
				return true;
			} else if((*this)[n] > v[n]) {
				return false;
			}
		}

		return num_elements() <= v.num_elements();
	}

	case VARIANT_TYPE_MAP: {
		return map_->elements <= v.map_->elements;
	}

	case VARIANT_TYPE_CALLABLE_LOADING: {
		return false;
	}

	case VARIANT_TYPE_CALLABLE: {
		return !v.callable_->less(callable_);
	}
	case VARIANT_TYPE_FUNCTION: {
		return fn_ <= v.fn_;
	}
	case VARIANT_TYPE_GENERIC_FUNCTION: {
		return generic_fn_ <= v.generic_fn_;
	}
	case VARIANT_TYPE_MULTI_FUNCTION: {
		return multi_fn_ <= v.multi_fn_;
	}
	case VARIANT_TYPE_WEAK:
	case VARIANT_TYPE_DELAYED:
	case VARIANT_TYPE_INVALID:
		assert(false);
	}

	assert(false);
	return false;
}

bool variant::operator>=(const variant& v) const
{
	return v <= *this;
}

bool variant::operator<(const variant& v) const
{
	return !(*this >= v);
}

bool variant::operator>(const variant& v) const
{
	return !(*this <= v);
}

void variant::throw_type_error(variant::TYPE t) const
{
	if(this == &UnfoundInMapNullVariant) {
		const debug_info* info = last_failed_query_map.get_debug_info();
		if(info) {
			generate_error(formatter() << "In object at " << *info->filename << " " << info->line << " (column " << info->column << ") did not find attribute " << last_failed_query_key << " which was expected to be a " << variant_type_to_string(t));
		} else if(last_failed_query_map.get_source_expression()) {
			generate_error(formatter() << "Map object generated in FFL was expected to have key '" << last_failed_query_key << "' of type " << variant_type_to_string(t) << " but this key wasn't found. The map was generated by this expression:\n" << last_failed_query_map.get_source_expression()->debug_pinpoint_location());
		}
	}

	if(last_query_map.is_map() && last_query_map.get_debug_info()) {
		for(std::map<variant,variant>::const_iterator i = last_query_map.map_->elements.begin(); i != last_query_map.map_->elements.end(); ++i) {
			if(this == &i->second) {
				const debug_info* info = i->first.get_debug_info();
				if(info == NULL) {
					info = last_query_map.get_debug_info();
				}
				generate_error(formatter() << "In object at " << *info->filename << " " << info->line << " (column " << info->column << ") attribute for " << i->first << " was " << *this << ", which is a " << variant_type_to_string(type_) << ", must be a " << variant_type_to_string(t));
				
			}
		}
	} else if(last_query_map.is_map() && last_query_map.get_source_expression()) {
		for(std::map<variant,variant>::const_iterator i = last_query_map.map_->elements.begin(); i != last_query_map.map_->elements.end(); ++i) {
			if(this == &i->second) {
				std::ostringstream expression;
				if(last_failed_query_map.get_source_expression()) {
					expression << " The map was generated by this expression:\n" << last_failed_query_map.get_source_expression()->debug_pinpoint_location();
				}

				generate_error(formatter() << "Map object generated in FFL was expected to have key '" << last_failed_query_key << "' of type " << variant_type_to_string(t) << " but this key was of type " << variant_type_to_string(i->second.type_) << " instead." << expression.str());
			}
		}
	}

	const debug_info* info = get_debug_info();
	std::string loc;
	if(info) {
		loc = formatter() << " at " << *info->filename << " " << info->line << " (column " << info->column << ")\n";
	}

	std::string representation;
	try {
		representation = write_json();
	} catch(...) {
		representation = "(COULD NOT SERIALIZE TYPE)";
	}

	std::ostringstream fmt;
	fmt << "type error: " << " expected " << variant_type_to_string(t) << " but found " << variant_type_to_string(type_) << " " << representation << loc;
	generate_error(fmt.str());
}

void variant::serialize_to_string(std::string& str) const
{
	switch(type_) {
	case VARIANT_TYPE_NULL:
		str += "null";
		break;
	case VARIANT_TYPE_BOOL:
		str += bool_value_ ? "true" : "false";
		break;
	case VARIANT_TYPE_INT:
		str += boost::lexical_cast<std::string>(int_value_);
		break;
	case VARIANT_TYPE_DECIMAL: {
		std::ostringstream s;
		s << decimal::from_raw_value(decimal_value_);
		str += s.str();
		break;
	}
	case VARIANT_TYPE_CALLABLE_LOADING: {
		ASSERT_LOG(false, "TRIED TO SERIALIZE A VARIANT LOADING");
		break;
	}
	case VARIANT_TYPE_CALLABLE: {
		const game_logic::wml_serializable_formula_callable* obj = try_convert<game_logic::wml_serializable_formula_callable>();
		if(obj) {
			//we have an object that is to be serialized into WML. However,
			//it might be present in the level or a reference to it held
			//from multiple objects. So we record the address of it and
			//register it to be recorded seperately.
			char buf[256];
			sprintf(buf, "deserialize('%p')", obj);
			str += buf;
			return;
		}

		callable_->serialize(str);
		break;
	}
	case VARIANT_TYPE_LIST: {
		str += "[";
		bool first_time = true;
		for(size_t i=0; i < list_->size(); ++i) {
			const variant& var = list_->begin[i];
			if(!first_time) {
				str += ",";
			}
			first_time = false;
			var.serialize_to_string(str);
		}
		str += "]";
		break;
	}
	case VARIANT_TYPE_MAP: {
		str += "{";
		bool first_time = true;
		for(std::map<variant,variant>::const_iterator i=map_->elements.begin(); i != map_->elements.end(); ++i) {
			if(!first_time) {
				str += ",";
			}
			first_time = false;
			i->first.serialize_to_string(str);
			str += ": ";
			i->second.serialize_to_string(str);
		}
		str += "}";
		break;
	}
	case VARIANT_TYPE_STRING: {
		if( !string_->str.empty() ) {
			if(string_->str[0] == '~' && string_->str[string_->str.length()-1] == '~') {
				str += string_->str;
			} else {
				const char* delim = "'";
				if(strchr(string_->str.c_str(), '\'')) {
					delim = "~";
				}

				str += delim;
				str += string_->str;
				str += delim;
			}
		}
		break;
	}
	case VARIANT_TYPE_FUNCTION:
		if(fn_->fn) {
			fprintf(stderr, "ATTEMPT TO SERIALIZE FUNCTION: %s\n", fn_->fn->str().c_str());
		}
		assert(false);
	default:
		assert(false);
	}
}

void variant::serialize_from_string(const std::string& str)
{
	try {
		*this = game_logic::formula(variant(str)).execute();
	} catch(...) {
		*this = variant(str);
	}
}

variant variant::create_variant_under_construction(intptr_t id)
{
	variant v;
	if(game_logic::wml_formula_callable_read_scope::try_load_object(id, v)) {
		return v;
	}

	v.type_ = VARIANT_TYPE_CALLABLE_LOADING;
	v.callable_loading_ = id;
	v.increment_refcount();
	return v;
}

int variant::refcount() const
{

	switch(type_) {
	case VARIANT_TYPE_LIST:
		return list_->refcount;
		break;
	case VARIANT_TYPE_STRING:
		return string_->refcount;
		break;
	case VARIANT_TYPE_MAP:
		return map_->refcount;
		break;
	case VARIANT_TYPE_CALLABLE:
		return callable_->refcount();
		break;
	default:
		return -1;
	}
}

void variant::make_unique()
{
	if(refcount() == 1) {
		return;
	}

	switch(type_) {
	case VARIANT_TYPE_LIST: {
		list_->refcount--;
		list_ = new variant_list(*list_);
		foreach(variant& v, list_->elements) {
			v.make_unique();
		}
		break;
	}
	case VARIANT_TYPE_STRING:
		string_->refcount--;
		string_ = new variant_string(*string_);
		string_->refcount = 1;
		break;
	case VARIANT_TYPE_MAP: {
		std::map<variant,variant> m;
		for(std::map<variant,variant>::const_iterator i = map_->elements.begin(); i != map_->elements.end(); ++i) {
			variant key = i->first;
			variant value = i->second;
			key.make_unique();
			value.make_unique();
			m[key] = value;
		}

		map_->refcount--;

		variant_map* vm = new variant_map;
		vm->info = map_->info;
		vm->refcount = 1;
		vm->elements.swap(m);
		map_ = vm;
		break;
	}
	default:
		break;
	}
}

std::string variant::string_cast() const
{
	switch(type_) {
	case VARIANT_TYPE_NULL:
		return "null";
	case VARIANT_TYPE_BOOL:
		return bool_value_ ? "true" : "false";
	case VARIANT_TYPE_INT:
		return boost::lexical_cast<std::string>(int_value_);
	case VARIANT_TYPE_DECIMAL: {
		std::string res;
		serialize_to_string(res);
		return res;
	}
	case VARIANT_TYPE_CALLABLE_LOADING:
		return "(object loading)";
	case VARIANT_TYPE_CALLABLE:
		return "(object)";
	case VARIANT_TYPE_LIST: {
		std::string res = "";
		for(size_t i=0; i < list_->size(); ++i) {
			const variant& var = list_->begin[i];
			if(!res.empty()) {
				res += ", ";
			}

			res += var.string_cast();
		}

		return res;
	}
	case VARIANT_TYPE_MAP: {
		std::string res = "";
		for(std::map<variant,variant>::const_iterator i=map_->elements.begin(); i != map_->elements.end(); ++i) {
			if(!res.empty()) {
				res += ",";
			}
			res += i->first.string_cast();
			res += ": ";
			res += i->second.string_cast();
		}
		return res;
	}

	case VARIANT_TYPE_STRING:
		return string_->str;
	default:
		assert(false);
		return "invalid";
	}
}

std::string variant::to_debug_string(std::vector<const game_logic::formula_callable*>* seen) const
{
	std::vector<const game_logic::formula_callable*> seen_stack;
	if(!seen) {
		seen = &seen_stack;
	}

	std::ostringstream s;
	switch(type_) {
	case VARIANT_TYPE_NULL:
		s << "null";
		break;
	case VARIANT_TYPE_BOOL:
		s << (bool_value_ ? "true" : "false");
		break;
	case VARIANT_TYPE_INT:
		s << int_value_;
		break;
	case VARIANT_TYPE_DECIMAL:
		s << string_cast();
		break;
	case VARIANT_TYPE_LIST: {
		s << "[";
		for(size_t n = 0; n != num_elements(); ++n) {
			if(n != 0) {
				s << ", ";
			}

			s << operator[](n).to_debug_string(seen);
		}
		s << "]";
		break;
	}
	case VARIANT_TYPE_CALLABLE_LOADING: {
		char buf[64];
		sprintf(buf, "(loading %lx)", (long unsigned int)callable_loading_);
		s << buf;
		break;
	}

	case VARIANT_TYPE_CALLABLE: {
		char buf[64];
		sprintf(buf, "(%p)", callable_);
		s << buf << "{";
		if(std::find(seen->begin(), seen->end(), callable_) == seen->end()) {
			seen->push_back(callable_);

			const variant_type_ptr type = get_variant_type_from_value(*this);
			const game_logic::formula_callable_definition* def = type->get_definition();
			if(def) {
				bool first = true;
				for(int slot = 0; slot < def->num_slots(); ++slot) {
					if(!first) {
						s << ",\n";
					}

					variant value;

					try {
						const assert_recover_scope scope;
						if(def->supports_slot_lookups()) {
							value = callable_->query_value_by_slot(slot);
						} else {
							value = callable_->query_value(def->get_entry(slot)->id);
						}
					} catch(...) {
						value = variant("(Unknown)");
					}

					first = false;
					s << def->get_entry(slot)->id << ": " << value.to_debug_string(seen);
				}
			} else {
				s << "Uninspectable Object: " << type->to_string();
			}
		} else {
			s << "...";
		}
		s << "}";
		break;
	}
	case VARIANT_TYPE_FUNCTION: {
		char buf[64];
		sprintf(buf, "(%p)", fn_);
		s << buf << "(";
		bool first = true;
		for(std::vector<std::string>::const_iterator i = fn_->type->arg_names.begin(); i != fn_->type->arg_names.end(); ++i) {
			if (first)
				first = false;
			else
				s << ", ";
			s << *i;
		}
		s << ")";
		break;
	}
	case VARIANT_TYPE_GENERIC_FUNCTION: {
		char buf[64];
		sprintf(buf, "(%p)", generic_fn_);
		s << "<>";
		s << buf << "(";
		bool first = true;
		for(std::vector<std::string>::const_iterator i = generic_fn_->type->arg_names.begin(); i != generic_fn_->type->arg_names.end(); ++i) {
			if (first)
				first = false;
			else
				s << ", ";
			s << *i;
		}
		s << ")";
		break;
	}
	case VARIANT_TYPE_MULTI_FUNCTION: {
		s << "overload(";
		foreach(const variant& v, multi_fn_->functions) {
			s << v.to_debug_string() << ", ";
		}
		s << ")";
		break;
	}
	case VARIANT_TYPE_WEAK: {
		char buf[64];
		sprintf(buf, "(weak %p)", delayed_);
		s << buf;
		break;
	}
	case VARIANT_TYPE_DELAYED: {
		char buf[64];
		sprintf(buf, "(delayed %p)", delayed_);
		s << buf;
		break;
	}
	case VARIANT_TYPE_MAP: {
		s << "{";
		bool first_time = true;
		for(std::map<variant,variant>::const_iterator i=map_->elements.begin(); i != map_->elements.end(); ++i) {
			if(!first_time) {
				s << ",";
			}
			first_time = false;
			s << i->first.to_debug_string(seen);
			s << ": ";
			s << i->second.to_debug_string(seen);
		}
		s << "}";
		break;
	}
	case VARIANT_TYPE_STRING: {
		s << "'" << string_->str << "'";
		break;
	}
	case VARIANT_TYPE_INVALID: {
		assert(false);
		break;

	}
	}

	return s.str();
}

std::string variant::write_json(bool pretty, write_flags flags) const
{
	std::ostringstream s;
	if(pretty) {
		write_json_pretty(s, "", flags);
	} else {
		write_json(s, flags);
	}
	return s.str();
}

void variant::write_json(std::ostream& s, write_flags flags) const
{
	switch(type_) {
	case VARIANT_TYPE_NULL: {
		s << "null";
		return;
	}
	case VARIANT_TYPE_BOOL:
		s << (bool_value_ ? "true" : "false");
		break;
	case VARIANT_TYPE_INT: {
		s << as_int();
		return;
	}
	case VARIANT_TYPE_DECIMAL: {
		s << decimal::from_raw_value(decimal_value_);
		return;
	}
	case VARIANT_TYPE_MAP: {
		s << "{";
		for(std::map<variant,variant>::const_iterator i = map_->elements.begin(); i != map_->elements.end(); ++i) {
			if(i != map_->elements.begin()) {
				s << ',';
			}

			if(i->first.is_string()) {
				s << '"' << i->first.string_cast() << "\":";
			} else {
				std::string str = i->first.write_json(true, flags);
				boost::replace_all(str, "\"", "\\\"");
				s << "\"@eval " << str << "\":";
			}

			i->second.write_json(s, flags);
		}

		s << "}";
		return;
	}
	case VARIANT_TYPE_LIST: {
		s << "[";

		for(std::vector<variant>::const_iterator i = list_->begin;
		    i != list_->end; ++i) {
			if(i != list_->begin) {
				s << ',';
			}

			i->write_json(s, flags);
		}

		s << "]";
		return;
	}
	case VARIANT_TYPE_STRING: {
		const std::string& str = string_->translated_from.empty() ? string_->str : string_->translated_from;
		const char delim = string_->translated_from.empty() ? '"' : '~';
		if(std::count(str.begin(), str.end(), '\\') 
			|| std::count(str.begin(), str.end(), delim) 
			|| (flags == JSON_COMPLIANT && std::count(str.begin(), str.end(), '\n'))) {
			//escape the string
			s << delim;
			for(std::string::const_iterator i = str.begin(); i != str.end(); ++i) {
				if(*i == '\\' || *i == delim) {
					s << '\\';
				}

				if(flags == JSON_COMPLIANT && *i == '\n') {
					s << "\\n";
				} else {
					s << *i;
				}
			}
			s << delim;
		} else {
			s << delim << string_->str << delim;
		}
		return;
	}
	case VARIANT_TYPE_CALLABLE: {
		std::string str;
		serialize_to_string(str);
		s << "\"@eval " << str << "\"";
		return;
	}
	case VARIANT_TYPE_FUNCTION: {
		s << "\"@eval ";
		write_function(s);
		s << "\"";
		return;
	}
	case VARIANT_TYPE_GENERIC_FUNCTION: {
		//TODO: implement serialization of generic functions
		s << "generic_function_serialization_not_implemented";
		return;
	}

	case VARIANT_TYPE_MULTI_FUNCTION: {
		s << "\"@eval overload(";
		for(int n = 0; n != multi_fn_->functions.size(); ++n) {
			const variant& v = multi_fn_->functions[n];
			if(n != 0) {
				s << ", ";
			}
			v.write_function(s);
		}

		s << ")\"";
		return;
	}
	default:
		generate_error(formatter() << "illegal type to serialize to json: " << to_debug_string());
	}
}

void variant::write_function(std::ostream& s) const
{
	assert(fn_->fn);

	//Serialize the closure along with the object, if we can.
	const bool serialize_closure = fn_->callable && dynamic_cast<const game_logic::wml_serializable_formula_callable*>(fn_->callable.get());
	if(serialize_closure) {
		s << "delay_until_end_of_loading(q(bind_closure(";
	}
	
	s << "def(";
	const int default_base = fn_->type->arg_names.size() - fn_->type->default_args.size();
	for(std::vector<std::string>::const_iterator p = fn_->type->arg_names.begin(); p != fn_->type->arg_names.end(); ++p) {
		if(p != fn_->type->arg_names.begin()) {
			s << ",";
		}

		s << *p;
		
		const int index = p - fn_->type->arg_names.begin();
		if(index >= default_base) {
			variant v = fn_->type->default_args[index - default_base];
			std::string str;
			v.serialize_to_string(str);
			s << "=" << str;
		}
	}

	s << ") " << fn_->fn->str();

	if(serialize_closure) {
		std::string str;
		variant(fn_->callable.get()).serialize_to_string(str);
		s << "," << str << ")))";
	}
}

void variant::write_json_pretty(std::ostream& s, std::string indent, write_flags flags) const
{
	switch(type_) {
	case VARIANT_TYPE_MAP: {
		s << "{";
		indent += "\t";
		for(std::map<variant,variant>::const_iterator i = map_->elements.begin(); i != map_->elements.end(); ++i) {
			if(i != map_->elements.begin()) {
				s << ',';
			}

			s << "\n" << indent << '"';
			if(i->first.is_string()) {
				s << i->first.string_cast();
			} else {
				std::string str = i->first.write_json(true, flags);
				boost::replace_all(str, "\"", "\\\"");
				s << "@eval " << str;
			}

			s << "\": ";

			i->second.write_json_pretty(s, indent, flags);
		}
		indent.resize(indent.size()-1);

		s << "\n" << indent << "}";
		return;
	}
	case VARIANT_TYPE_LIST: {
		bool found_non_scalar = false;
		for(std::vector<variant>::const_iterator i = list_->begin;
		    i != list_->end; ++i) {
			if(i->is_list() || i->is_map()) {
				found_non_scalar = true;
				break;
			}
		}

		if(!found_non_scalar) {
			write_json(s, flags);
			return;
		}


		s << "[";

		indent += "\t";
		for(std::vector<variant>::const_iterator i = list_->begin;
		    i != list_->end; ++i) {
			if(i != list_->begin) {
				s << ',';
			}

			s << "\n" << indent;

			i->write_json_pretty(s, indent, flags);
		}

		indent.resize(indent.size()-1);

		if(list_->size() > 0) {
			s << "\n" << indent << "]";
		} else {
			s << "]";
		}

		return;
	}

	default:
		write_json(s, flags);
		break;
	}
}

void variant::add_formula_using_this(const game_logic::formula* f)
{
	if(is_string()) {
		string_->formulae_using_this.push_back(f);
	}
}

void variant::remove_formula_using_this(const game_logic::formula* f)
{
	if(is_string()) {
		string_->formulae_using_this.erase(std::remove(string_->formulae_using_this.begin(), string_->formulae_using_this.end(), f), string_->formulae_using_this.end());
	}
}

const std::vector<const game_logic::formula*>* variant::formulae_using_this() const
{
	if(is_string()) {
		return &string_->formulae_using_this;
	} else {
		return NULL;
	}
}

std::string variant::debug_info::message() const
{
	std::ostringstream s;
	s << *filename << " " << line << " (column " << column << ")";
	return s.str();
}

std::ostream& operator<<(std::ostream& os, const variant& v)
{
	os << v.write_json();
	return os;
}

std::pair<variant*,variant*> variant::range() const
{
	if(type_ == VARIANT_TYPE_LIST) {
		return std::pair<variant*,variant*>(&(*list_->begin), &(*list_->end));
	}
	variant v;
	return std::pair<variant*,variant*>(&v,&v);
}

UNIT_TEST(variant_decimal)
{
	variant d(9876000, variant::DECIMAL_VARIANT);
	variant d2(4000, variant::DECIMAL_VARIANT);
	CHECK_EQ(d.as_decimal().value(), 9876000);
	CHECK_EQ(d.as_int(), 9);
	CHECK_EQ(d.string_cast(), "9.876");
	CHECK_EQ((d + d2).as_decimal().value(), 9880000);
}

BENCHMARK(variant_assign)
{
	variant v(4);
	std::vector<variant> vec(1000);
	BENCHMARK_LOOP {
		for(int n = 0; n != vec.size(); ++n) {
			vec[n] = v;
		}
	}
}

UNIT_TEST(variant_foreach)
{
	std::vector<variant> l1;
	l1.push_back(variant(1));
	l1.push_back(variant(2));
	l1.push_back(variant(3));
	variant vl1(&l1);
	int n = 1;
	foreach(const variant& v, vl1.range()) {
		CHECK_EQ(v.as_int(), n);
		n++;
	}

	foreach(const variant& v, variant().range()) {
		CHECK(false, "foreach null variant operator failed");
	}
}
