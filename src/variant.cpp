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

#include <cmath>
#include <set>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <sstream>

#include "boost/algorithm/string/replace.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/json_parser.hpp"

#include "asserts.hpp"
#include "ffl_weak_ptr.hpp"
#include "formatter.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_callable_utils.hpp"
#include "formula_garbage_collector.hpp"
#include "formula_interface.hpp"
#include "formula_object.hpp"
#include "formula_profiler.hpp"

#include "i18n.hpp"
#include "unit_test.hpp"
#include "variant.hpp"
#include "variant_type.hpp"
#include "variant_type_check.hpp"
#include "utf8_to_codepoint.hpp"
#include "wml_formula_callable.hpp"

namespace 
{
	static const std::string variant_type_str[] = {"null", "bool", "int", "decimal", "object", "object_loading", "list", "string", "map", "function", "generic_function", "multi_function", "delayed", "weak", "enum"};
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

//enums
namespace
{
std::map<std::string,int> g_enum_map;
std::vector<std::string> g_enum_vector;
}

variant variant::create_enum(const std::string& enum_id)
{
	const int n = get_enum_index(enum_id);
	variant res(n);
	res.type_ = VARIANT_TYPE_ENUM;
	return res;
}

int variant::get_enum_index(const std::string& enum_id) {
	auto itor = g_enum_map.find(enum_id);
	if(itor == g_enum_map.end()) {
		const int result = static_cast<int>(g_enum_vector.size());
		g_enum_vector.push_back(enum_id);
		g_enum_map[enum_id] = result;
		return result;
	}

	return itor->second;
}

namespace {

struct VariantThreadInfo {
	VariantThreadInfo() : to_debug_string_depth(0) {}
	std::set<variant*> callable_variants_loading, delayed_variants_loading;
	std::vector<CallStackEntry> call_stack;
	variant last_failed_query_map, last_failed_query_key;
	variant last_query_map;
	variant UnfoundInMapNullVariant;
	int to_debug_string_depth;
};

THREAD_LOCAL VariantThreadInfo *g_variant_thread_info;

struct ToDebugStringDepthContext {
	ToDebugStringDepthContext() {
		++g_variant_thread_info->to_debug_string_depth;
	}

	~ToDebugStringDepthContext() {
		--g_variant_thread_info->to_debug_string_depth;
	}

	bool isTooDeep() const { return g_variant_thread_info->to_debug_string_depth > 100; }
};

}

void variant::registerThread()
{
	g_variant_thread_info = new VariantThreadInfo;
}

void variant::unregisterThread()
{
}

void init_call_stack(int min_size)
{
	g_variant_thread_info->call_stack.reserve(min_size);
}

void swap_variants_loading(std::set<variant*>& v)
{
	g_variant_thread_info->callable_variants_loading.swap(v);
}

void push_call_stack(const game_logic::FormulaExpression* frame, const game_logic::FormulaCallable* callable)
{
	g_variant_thread_info->call_stack.resize(g_variant_thread_info->call_stack.size()+1);
	g_variant_thread_info->call_stack.back().expression = frame;
	g_variant_thread_info->call_stack.back().callable = callable;
	ASSERT_LOG(g_variant_thread_info->call_stack.size() < 4096, "FFL Recursion too deep (Exceeds 4096 frames)");
}

void pop_call_stack()
{
	g_variant_thread_info->call_stack.pop_back();
}

std::string get_call_stack()
{
	variant current_frame;
	std::string res;
	std::vector<CallStackEntry> reversed_call_stack = g_variant_thread_info->call_stack;
	std::reverse(reversed_call_stack.begin(), reversed_call_stack.end());
	for(std::vector<CallStackEntry>::const_iterator i = reversed_call_stack.begin(); i != reversed_call_stack.end(); ++i) {
		const game_logic::FormulaExpression* p = i->expression;
		if(p && p->getParentFormula() != current_frame) {
			current_frame = p->getParentFormula();
			const variant::debug_info* info = current_frame.get_debug_info();
			if(!info) {
				res += "(UNKNOWN LOCATION) (" + current_frame.write_json() + "\n";
			} else {
				res += p->debugPinpointLocation() + "\n";
			}
		}
	}

	return res;
}

std::string get_typed_call_stack()
{
	variant current_frame;
	std::string res;
	std::vector<CallStackEntry> reversed_call_stack = g_variant_thread_info->call_stack;
	std::reverse(reversed_call_stack.begin(), reversed_call_stack.end());
	for(std::vector<CallStackEntry>::const_iterator i = reversed_call_stack.begin(); i != reversed_call_stack.end(); ++i) {
		const game_logic::FormulaExpression* p = i->expression;
		if(p && p->getParentFormula() != current_frame) {
			current_frame = p->getParentFormula();
			const variant::debug_info* info = current_frame.get_debug_info();
			if(!info) {
				res += "(UNKNOWN LOCATION) (" + current_frame.write_json() + "\n";
			} else {
				res += p->debugPinpointLocation() + "\n";
			}
			res += " has type " + variant::variant_type_to_string(current_frame.type()) + ".\n\n";
		}
	}

	return res;
}

const std::vector<CallStackEntry>& get_expression_call_stack()
{
	return g_variant_thread_info->call_stack;
}

std::string get_full_call_stack()
{
	std::string res;
	for(std::vector<CallStackEntry>::const_iterator i = g_variant_thread_info->call_stack.begin();
	    i != g_variant_thread_info->call_stack.end(); ++i) {
		if(!i->expression) {
			continue;
		}
		res += formatter() << "  FRAME " << (i - g_variant_thread_info->call_stack.begin()) << ": " << i->expression->str() << "\n";
	}
	return res;
}

std::string output_formula_error_info();

namespace {
void generate_error(std::string message)
{
	if(g_variant_thread_info->call_stack.empty() == false && g_variant_thread_info->call_stack.back().expression) {
		message += "\n" + g_variant_thread_info->call_stack.back().expression->debugPinpointLocation();
	}

	std::ostringstream s;
	s << "ERROR: " << message << "\n" << get_typed_call_stack();
	s << output_formula_error_info();

	ASSERT_LOG(false, s.str() + "\ntype error");
}

}

type_error::type_error(const std::string& str) : message(str) {
	if(g_variant_thread_info->call_stack.empty() == false && g_variant_thread_info->call_stack.back().expression) {
		message += "\n" + g_variant_thread_info->call_stack.back().expression->debugPinpointLocation();
	}

	LOG_ERROR(message << "\n" << get_typed_call_stack());
	LOG_ERROR(output_formula_error_info());
}

VariantFunctionTypeInfo::VariantFunctionTypeInfo() : num_unneeded_args(0)
{}

struct variant_uuid : public GarbageCollectible {
	explicit variant_uuid(boost::uuids::uuid id) : uuid(id) {}
	boost::uuids::uuid uuid;
};

struct variant_list : public GarbageCollectible {

	variant_list() : begin(elements.begin()), end(elements.end()),
	                 storage(nullptr)
	{}

	variant_list(const variant_list& o) :
	   elements(o.begin, o.end), begin(elements.begin()), end(elements.end()),
	   storage(nullptr)
	{}

	const variant_list& operator=(const variant_list& o) {
		elements.assign(o.begin, o.end),
		begin = elements.begin();
		end = elements.end();
		storage = nullptr;
		return *this;
	}

	~variant_list() {
	}

	void surrenderReferences(GarbageCollector* collector) override {
		collector->surrenderPtr(&storage, "STORAGE");
		for(variant& el : elements) {
			collector->surrenderVariant(&el, "ELEMENT");
		}
	}

	std::string debugObjectName() const override {
		std::ostringstream s;
		s << "list[" << size() << "]";
		if(info.filename) {
			s << " @" << info.message();
		} else {
			s << " @UNK";
		}

		return s.str();
	}

	std::string debugObjectSpew() const override {
		std::ostringstream s;
		s << "list[" << size() << "]";
		if(info.filename) {
			s << " @" << info.message();
		} else {
			s << " @UNK";
		}

		s << " [[";
		for(const variant& el : elements) {
			s << el.to_debug_string();
		}

		s << "]]";

		return s.str();
	}

#if defined(_MSC_VER) && defined(_DEBUG)
	// hack to work around checked iterators failing on this.
	size_t size() const { return end._Ptr - begin._Ptr; }
#else
	size_t size() const { return end - begin; }
#endif

	variant::debug_info info;
	ffl::IntrusivePtr<const game_logic::FormulaExpression> expression;
	std::vector<variant> elements;
	ffl::IntrusivePtr<variant_list> storage;
	std::vector<variant>::iterator begin, end;
};

struct variant_string {
	variant::debug_info info;
	ffl::IntrusivePtr<const game_logic::FormulaExpression> expression;

	variant_string() : refcount(0), str_len(0)
	{}
	variant_string(const variant_string& o) : str(o.str), translated_from(o.translated_from), refcount(1), str_len(o.str_len)
	{}
	explicit variant_string(const std::string& s) : str(s), refcount(0) {
		str_len = utils::str_len_utf8(str);
	}

	std::string str, translated_from;
	IntRefCount refcount;

	std::vector<const game_logic::Formula*> formulae_using_this;

	//number of characters. Might not be equal to str.size() if the string contains
	//extended utf-8 characters.
	size_t str_len;

	private:
	void operator=(const variant_string&);
};

struct variant_map : public GarbageCollectible {
	variant::debug_info info;
	ffl::IntrusivePtr<const game_logic::FormulaExpression> expression;

	variant_map() : GarbageCollectible(), modcount(0)
	{
	}
	variant_map(const variant_map& o) : GarbageCollectible(o), expression(o.expression), elements(o.elements), modcount(0)
	{
	}

	~variant_map()
	{
	}

	void surrenderReferences(GarbageCollector* collector) override {
		for(std::pair<const variant,variant>& p : elements) {
			collector->surrenderVariant(&p.first, "KEY");
			collector->surrenderVariant(&p.second, p.first.is_string() ? p.first.as_string().c_str() : "VALUE");
		}
	}

	std::string debugObjectName() const override {
		std::string res = "map(";
		for(const std::pair<const variant,variant>& p : elements) {
			if(p.first.is_string()) {
				res += p.first.as_string() + ",";
			}
		}

		res += ")";
		return res;
	}

	std::string debugObjectSpew() const override {
		std::string res = "map(";
		if(info.filename) {
			res += info.message() + ", ";
		}

		for(const std::pair<const variant,variant>& p : elements) {
			res += p.first.to_debug_string() + ": " + p.second.to_debug_string() + ", ";
		}

		res += ")";
		return res;
	}

	std::map<variant,variant> elements;
	int modcount;
private:
	void operator=(const variant_map&);
};

struct variant_fn : public GarbageCollectible {
	variant::debug_info info;

	variant_fn() : base_slot(0), needs_type_checking(false)
	{}

	void surrenderReferences(GarbageCollector* collector) override {
		collector->surrenderPtr(&callable, "CLOSURE");
		for(variant& v : bound_args) {
			collector->surrenderVariant(&v, "BOUND ARG");
		}
	}

	VariantFunctionTypeInfoPtr type;

	std::function<variant(const game_logic::FormulaCallable&)> builtin_fn;
	game_logic::ConstFormulaPtr fn;
	game_logic::ConstFormulaCallablePtr callable;

	boost::intrusive_ptr<game_logic::SlotFormulaCallable> cached_callable;

	std::vector<variant> bound_args;

	int base_slot;

	bool needs_type_checking;

	void calculate_needs_type_checking()
	{
		needs_type_checking = false;
		for(variant_type_ptr t : type->variant_types) {
			if(t->is_class() || t->is_interface()) {
				needs_type_checking = true;
				break;
			}
		}
	}
};

struct variant_generic_fn : public GarbageCollectible {
	variant::debug_info info;

	variant_generic_fn() : base_slot(0)
	{}

	void surrenderReferences(GarbageCollector* collector) override {
		collector->surrenderVariant(&fn, "CLOSURE");
		collector->surrenderPtr(&callable);
		for(variant& v : bound_args) {
			collector->surrenderVariant(&v, "BOUND ARG");
		}
	}

	VariantFunctionTypeInfoPtr type;

	variant fn;
	std::vector<std::string> generic_types;
	game_logic::ConstFormulaCallablePtr callable;

	std::vector<variant> bound_args;

	std::function<game_logic::ConstFormulaPtr(const std::vector<variant_type_ptr>&)> factory;

	mutable std::map<std::vector<std::string>, variant> cache;

	int base_slot;
};

struct variant_multi_fn : public GarbageCollectible {
	variant_multi_fn()
	{}

	~variant_multi_fn()
	{
	}

	void surrenderReferences(GarbageCollector* collector) override {
		for(variant& fn : functions) {
			collector->surrenderVariant(&fn, "FUNCTION");
		}
	}

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

game_logic::ConstFormulaPtr fn;
game_logic::ConstFormulaCallablePtr callable;

bool has_result;
variant result;

IntRefCount refcount;
};

struct variant_weak 
{
	variant_weak() : refcount(0)
	{}

	IntRefCount refcount;
	ffl::weak_ptr<game_logic::FormulaCallable> ptr;
};

void variant::increment_refcount()
{
switch(type_) {
case VARIANT_TYPE_LIST:
if(list_) list_->add_reference();
break;
case VARIANT_TYPE_STRING:
++string_->refcount;
break;
case VARIANT_TYPE_MAP:
map_->add_reference();
break;
case VARIANT_TYPE_CALLABLE:
variant_ptr_add_ref(callable_);
break;
case VARIANT_TYPE_FUNCTION:
fn_->add_reference();
break;
case VARIANT_TYPE_GENERIC_FUNCTION:
generic_fn_->add_reference();
break;
case VARIANT_TYPE_MULTI_FUNCTION:
multi_fn_->add_reference();
break;
case VARIANT_TYPE_DELAYED:
g_variant_thread_info->delayed_variants_loading.insert(this);
++delayed_->refcount;
break;
case VARIANT_TYPE_WEAK:
++weak_->refcount;
break;
case VARIANT_TYPE_CALLABLE_LOADING:
ASSERT_LOG(callable_loading_->refcount() > 0 || game_logic::wmlFormulaCallableReadScope::isActive() > 0, "Callable loading created when not in a read scope");
callable_loading_->add_reference();
g_variant_thread_info->callable_variants_loading.insert(this);

// These are not used here, add them to silence a compiler warning.
case VARIANT_TYPE_NULL:
case VARIANT_TYPE_INT:
case VARIANT_TYPE_ENUM:
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
if(list_) list_->dec_reference();
break;
case VARIANT_TYPE_STRING:
if(--string_->refcount == 0) {
	delete string_;
}
break;
case VARIANT_TYPE_MAP:
map_->dec_reference();
break;
case VARIANT_TYPE_CALLABLE:
variant_ptr_release(callable_);
break;
case VARIANT_TYPE_FUNCTION:
fn_->dec_reference();
break;
case VARIANT_TYPE_GENERIC_FUNCTION:
generic_fn_->dec_reference();
break;
case VARIANT_TYPE_MULTI_FUNCTION:
multi_fn_->dec_reference();
break;
case VARIANT_TYPE_DELAYED:
g_variant_thread_info->delayed_variants_loading.erase(this);
if(--delayed_->refcount == 0) {
	delete delayed_;
}
break;
case VARIANT_TYPE_WEAK:
if(--weak_->refcount == 0) {
	delete weak_;
}
break;

case VARIANT_TYPE_CALLABLE_LOADING:
g_variant_thread_info->callable_variants_loading.erase(this);
callable_loading_->dec_reference();
break;

// These are not used here, add them to silence a compiler warning.
case VARIANT_TYPE_NULL:
case VARIANT_TYPE_INT:
case VARIANT_TYPE_ENUM:
case VARIANT_TYPE_BOOL:
case VARIANT_TYPE_DECIMAL:
case VARIANT_TYPE_INVALID:
break;
}
}

const game_logic::FormulaExpression* variant::get_source_expression() const
{
	switch(type_) {
	case VARIANT_TYPE_LIST:
	case VARIANT_TYPE_STRING:
	case VARIANT_TYPE_MAP:
		return map_->expression.get();
	default:
		break;
	}

	return nullptr;
}

void variant::set_source_expression(const game_logic::FormulaExpression* expr)
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

void variant::setDebugInfo(const debug_info& info)
{
	switch(type_) {
	case VARIANT_TYPE_LIST:
		if(list_) list_->info = info;
		break;
	case VARIANT_TYPE_STRING:
		string_->info = info;
		break;
	case VARIANT_TYPE_MAP:
		map_->info = info;
		break;
	default:
		break;
	}
}

const variant::debug_info* variant::get_debug_info() const
{
	switch(type_) {
	case VARIANT_TYPE_LIST:
		if(list_ && list_->info.filename) { return &list_->info; }
		break;
	case VARIANT_TYPE_STRING:
		if(string_->info.filename) { return &string_->info; }
		break;
	case VARIANT_TYPE_MAP:
		if(map_->info.filename) { return &map_->info; }
		break;
	default:
		break;
	}

	return nullptr;
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

variant variant::create_delayed(game_logic::ConstFormulaPtr f, game_logic::ConstFormulaCallablePtr callable)
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
	std::set<variant*> items = g_variant_thread_info->delayed_variants_loading;
	for(variant* v : items) {
		v->delayed_->calculate_result();
		variant res = v->delayed_->result;
		*v = res;
	}

	g_variant_thread_info->delayed_variants_loading.clear();
}

variant variant::create_function_overload(const std::vector<variant>& fn)
{
	variant result;
	result.type_ = VARIANT_TYPE_MULTI_FUNCTION;
	result.multi_fn_ = new variant_multi_fn;
	result.multi_fn_->add_reference();
	result.multi_fn_->functions = fn;
	return result;
}

variant::variant(const game_logic::FormulaCallable* callable)
	: type_(VARIANT_TYPE_CALLABLE), callable_(callable)
{
	if(callable == nullptr) {
		type_ = VARIANT_TYPE_NULL;
		return;
	}
	increment_refcount();

	registerGlobalVariant(this);
}

variant::variant(std::vector<variant>* array)
    : type_(VARIANT_TYPE_LIST)
{
	assert(array);
	if(array->empty() == false) {
		list_ = new variant_list;
		list_->add_reference();
		list_->elements.swap(*array);
		list_->begin = list_->elements.begin();
		list_->end = list_->elements.end();
	} else {
		list_ = nullptr;
	}

	registerGlobalVariant(this);
}

variant::variant(const char* s)
   : type_(VARIANT_TYPE_STRING)
{
	if(s == nullptr) {
		type_ = VARIANT_TYPE_NULL;
		return;
	}
	string_ = new variant_string(std::string(s));
	increment_refcount();

	registerGlobalVariant(this);
}

variant::variant(const std::string& str)
	: type_(VARIANT_TYPE_STRING)
{
	string_ = new variant_string(str);
	increment_refcount();

	registerGlobalVariant(this);
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
			LOG_ERROR("VALUE: " << i->second.to_debug_string());
			assert(false);
		}
	}
	
	assert(map);
	map_ = new variant_map;
	map_->add_reference();
	map_->elements.swap(*map);

	registerGlobalVariant(this);
}

variant::variant(const variant& formula_var, const game_logic::FormulaCallable& callable, int base_slot, const VariantFunctionTypeInfoPtr& type_info, const std::vector<std::string>& generic_types, std::function<game_logic::ConstFormulaPtr(const std::vector<variant_type_ptr>&)> factory)
	: type_(VARIANT_TYPE_GENERIC_FUNCTION)
{
	generic_fn_ = new variant_generic_fn;
	generic_fn_->add_reference();
	generic_fn_->fn = formula_var;
	generic_fn_->callable = &callable;
	generic_fn_->base_slot = base_slot;
	generic_fn_->type = type_info;
	generic_fn_->generic_types = generic_types;
	generic_fn_->factory = factory;

	if(formula_var.get_debug_info()) {
		setDebugInfo(*formula_var.get_debug_info());
	}

	registerGlobalVariant(this);
}

variant::variant(const game_logic::ConstFormulaPtr& formula, const game_logic::FormulaCallable& callable, int base_slot, const VariantFunctionTypeInfoPtr& type_info)
  : type_(VARIANT_TYPE_FUNCTION)
{
	fn_ = new variant_fn;
	fn_->add_reference();
	fn_->fn = formula;
	fn_->callable = &callable;
	fn_->base_slot = base_slot;
	fn_->type = type_info;

	fn_->calculate_needs_type_checking();

	ASSERT_EQ(fn_->type->variant_types.size(), fn_->type->arg_names.size());

	if(formula->strVal().get_debug_info()) {
		setDebugInfo(*formula->strVal().get_debug_info());
	}

	registerGlobalVariant(this);
}

variant variant::change_function_callable(const game_logic::FormulaCallable& callable) const
{
	variant res;
	res.type_ = VARIANT_TYPE_FUNCTION;
	res.fn_ = new variant_fn(*fn_);
	res.fn_->add_reference();
	res.fn_->callable = &callable;
	return res;
}

const game_logic::FormulaCallable* variant::get_function_closure() const
{
	return fn_->callable.get();
}

variant::variant(std::function<variant(const game_logic::FormulaCallable&)> builtin_fn, const VariantFunctionTypeInfoPtr& type_info)
  : type_(VARIANT_TYPE_FUNCTION)
{
	fn_ = new variant_fn;
	fn_->add_reference();
	fn_->builtin_fn = builtin_fn;
	fn_->base_slot = 0;
	fn_->type = type_info;

	fn_->calculate_needs_type_checking();

	ASSERT_EQ(fn_->type->variant_types.size(), fn_->type->arg_names.size());

	registerGlobalVariant(this);
}

/*
variant::variant(game_logic::ConstFormulaPtr fml, const std::vector<std::string>& args, const game_logic::FormulaCallable& callable, int base_slot, const std::vector<variant>& default_args, const std::vector<variant_type_ptr>& variant_types, const variant_type_ptr& return_type)
  : type_(VARIANT_TYPE_FUNCTION)
{
	fn_ = new variant_fn;
	fn_->type->arg_names = args;
	fn_->base_slot = base_slot;
	fn_->fn = fml;
	fn_->callable = &callable;
	fn_->type->default_args = default_args;
	fn_->variant_types = variant_types;

	fn_->calculate_needs_type_checking();

	ASSERT_EQ(fn_->variant_types.size(), fn_->type->arg_names.size());

	fn_->return_type = return_type;
	increment_refcount();

	if(fml->strVal().get_debug_info()) {
		setDebugInfo(*fml->strVal().get_debug_info());
	}
}
*/

const variant& variant::operator=(const variant& v)
{
	if(&v != this) {
		if (type_ > VARIANT_TYPE_DECIMAL) {
			release();
		}

		type_ = v.type_;
		value_ = v.value_;
		if (type_ > VARIANT_TYPE_DECIMAL) {
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
	if(list_ == nullptr || n >= list_->size()) {
		generate_error(formatter() << "invalid index of " << static_cast<int>(n) << " into " << write_json());
	}

	return list_->begin[n];
}

const variant& variant::operator[](const variant& v) const
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
			g_variant_thread_info->last_failed_query_map = *this;
			g_variant_thread_info->last_failed_query_key = v;

			return g_variant_thread_info->UnfoundInMapNullVariant;
		}

		g_variant_thread_info->last_query_map = *this;
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

variant variant::getKeys() const
{
	must_be(VARIANT_TYPE_MAP);
	assert(map_);
	std::vector<variant> tmp;
	for(std::map<variant,variant>::const_iterator i=map_->elements.begin(); i != map_->elements.end(); ++i) {
			tmp.push_back(i->first);
	}
	return variant(&tmp);
}

variant variant::getValues() const
{
	must_be(VARIANT_TYPE_MAP);
	assert(map_);
	std::vector<variant> tmp;
	for(std::map<variant,variant>::const_iterator i=map_->elements.begin(); i != map_->elements.end(); ++i) {
			tmp.push_back(i->second);
	}
	return variant(&tmp);
}

int variant::num_elements() const
{
	if (type_ == VARIANT_TYPE_NULL){
		return 0;
	} else if(type_ == VARIANT_TYPE_CALLABLE) {
		return 1;
	} else if (type_ == VARIANT_TYPE_LIST) {
		if(list_ == nullptr) {
			return 0;
		}
		return static_cast<int>(list_->size());
	} else if (type_ == VARIANT_TYPE_STRING) {
		assert(string_);
		return static_cast<int>(string_->str_len);
	} else if (type_ == VARIANT_TYPE_MAP) {
		assert(map_);
		return static_cast<int>(map_->elements.size());
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

bool variant::is_str_utf8() const
{
	must_be(VARIANT_TYPE_STRING);
	return string_->str_len != string_->str.size();
}

variant variant::get_list_slice(int begin, int end) const
{
	std::vector<variant> items;
	variant result(&items);
	if(end <= begin) {
		return result;
	}

	must_be(VARIANT_TYPE_LIST);

	if(begin < 0 || static_cast<unsigned>(end) > num_elements()) {
		generate_error(formatter() << "ILLEGAL INDEX INTO LIST WHEN SLICING: " << begin << ", " << end << " / " << list_->size());
	}

	if(list_ == nullptr) {
		return result;
	}

	result.list_ = new variant_list;
	result.list_->add_reference();
	result.list_->begin = list_->begin + begin;
	result.list_->end = list_->begin + end;
	result.list_->storage.reset(list_);

	return result;
}

bool variant::function_call_valid(const std::vector<variant>& passed_args, std::string* message, bool allow_partial) const
{
	if(type_ == VARIANT_TYPE_MULTI_FUNCTION) {
		for(const variant& v : multi_fn_->functions) {
			if(v.function_call_valid(passed_args)) {
				return true;
			}
		}

		if(message) {
			std::ostringstream s;
			s << "Arguments do not match any overloaded functions.\n";
			for(int i = 0; i != passed_args.size(); ++i) {
				s << " Argument " << (i+1) << ": " << passed_args[i].write_json() << "\n";
			}

			s << "\nFunction signatures:\n";
			for(const variant& v : multi_fn_->functions) {
				s << "  (";
				for(variant_type_ptr type : v.function_arg_types()) {
					s << type->to_string() << ",";
				}

				s << ")\n";
			}


			*message = s.str();
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

	const auto max_args = fn_->type->arg_names.size();
	const auto min_args = max_args - fn_->type->num_default_args();

	if(args.size() > max_args || (args.size() < min_args && !allow_partial)) {
		if(message) {
			*message = "Incorrect number of arguments to function";
		}
		return false;
	}

	for(std::vector<variant>::size_type n = 0; n != args.size(); ++n) {
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

VariantFunctionTypeInfoPtr variant::get_function_info() const
{
	must_be(VARIANT_TYPE_FUNCTION);
	return fn_->type;
}

game_logic::ConstFormulaPtr variant::get_function_formula() const
{
	must_be(VARIANT_TYPE_FUNCTION);
	return fn_->fn;
}

int variant::get_function_base_slot() const
{
	must_be(VARIANT_TYPE_FUNCTION);
	return fn_->base_slot;
}

variant variant::operator()(const std::vector<variant>& passed_args) const
{
	std::vector<variant> args(passed_args);
	return (*this)(&args);
}

variant variant::operator()(std::vector<variant>* passed_args) const
{
	if(type_ == VARIANT_TYPE_MULTI_FUNCTION) {
		for(const variant& v : multi_fn_->functions) {
			if(v.function_call_valid(*passed_args)) {
				return v(passed_args);
			}
		}

		int narg = 1;
		std::ostringstream msg;
		for(variant arg : *passed_args) {
			msg << "Argument " << narg << ": " << arg.write_json() << " Type: " << get_variant_type_from_value(arg)->to_string() << "\n";
			++narg;
		}

		msg << "\nPossible functions:\n";

		for(const variant& v : multi_fn_->functions) {
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
		args_buf.insert(args_buf.end(), passed_args->begin(), passed_args->end());
	}

	std::vector<variant>* args = args_buf.empty() ? passed_args : &args_buf;

	ffl::IntrusivePtr<game_logic::SlotFormulaCallable> callable = fn_->cached_callable;
	
	if(callable) {
		fn_->cached_callable.reset();
	} else {
		callable.reset(new game_logic::SlotFormulaCallable);
	}

	if(fn_->callable) {
		callable->setFallback(fn_->callable);
	}

	callable->setBaseSlot(fn_->base_slot);

	const auto max_args = fn_->type->arg_names.size();
	const auto min_args = max_args - fn_->type->num_default_args();

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

	const int num_args_provided = args->size();

	if(fn_->needs_type_checking == false) {
		callable->setValues(args);
	} else {

	for(size_t n = 0; n != args->size(); ++n) {
		if(n < fn_->type->variant_types.size() && fn_->type->variant_types[n]) {
	//		if((*args)[n].is_map() && fn_->type->variant_types[n]->is_class(nullptr))
			if(fn_->type->variant_types[n]->match((*args)[n]) == false) {
				std::string class_name;
				if((*args)[n].is_map() && fn_->type->variant_types[n]->is_class(&class_name)) {
					//auto-construct an object from a map in a function argument
					game_logic::Formula::failIfStaticContext();

					ffl::IntrusivePtr<game_logic::FormulaObject> obj(game_logic::FormulaObject::create(class_name, (*args)[n]));

					args_buf = *args;
					args = &args_buf;

					args_buf[n] = variant(obj.get());

				} else if(fn_->type->variant_types[n]->is_interface()) {
					const game_logic::FormulaInterface* iface = fn_->type->variant_types[n]->is_interface();
					if((*args)[n].is_map() == false && (*args)[n].is_callable() == false) {
						generate_error((formatter() << "FUNCTION ARGUMENT " << (n+1) << " EXPECTED INTERFACE " << fn_->type->variant_types[n]->str() << " BUT FOUND " << (*args)[n].write_json()).str());
					}

					variant obj = iface->getDynamicFactory()->create((*args)[n]);

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
	}

	for(std::vector<variant>::size_type n = num_args_provided; n < max_args && (n - min_args) < fn_->type->default_args.size(); ++n) {
		callable->add(fn_->type->default_args[n - min_args]);
	}

	if(fn_->fn) {
		const variant result = fn_->fn->execute(*callable);
		if(fn_->type->return_type && !fn_->type->return_type->match(result)) {
			CallStackManager scope(fn_->fn->expr().get(), callable.get());
			generate_error(formatter() << "Function returned incorrect type, expecting " << fn_->type->return_type->to_string() << " but found " << result.write_json() << " (type: " << get_variant_type_from_value(result)->to_string() << ") FOR " << fn_->fn->str());
		}

		if(callable->refcount() == 1) {
			callable->clear();
			fn_->cached_callable = callable;
		}

		return result;
	} else {
		return fn_->builtin_fn(*callable);
	}
}

bool variant::disassemble(std::string* result) const
{
	if(type_ != VARIANT_TYPE_FUNCTION || !fn_->fn) {
		return false;
	}

	return fn_->fn->outputDisassemble(result);
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

	game_logic::ConstFormulaPtr fml = generic_fn_->factory(args);
	variant result(fml, *generic_fn_->callable, generic_fn_->base_slot, info);
	generic_fn_->cache[key] = result;
	return result;
}

variant variant::get_member(const std::string& str) const
{
	if(is_callable()) {
		return callable_->queryValue(str);
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
		return callable_ != nullptr;
	case VARIANT_TYPE_LIST:
		return list_ && list_->size() != 0;
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

std::string variant::as_enum() const
{
	must_be(VARIANT_TYPE_ENUM);
	return g_enum_vector[int_value_];
}

const std::vector<variant>& variant::as_list_ref() const
{
	must_be(VARIANT_TYPE_LIST);

	return list_->elements;
}

std::vector<variant> variant::as_list_optional() const
{
	if(is_null()) {
		return std::vector<variant>();
	}

	return as_list();
}

std::vector<variant> variant::as_list() const
{
	if(is_list()) {
		if(list_ == nullptr) {
			return std::vector<variant>();
		} else if(list_->elements.empty() == false) {
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
	if(list_ == nullptr) {
		return result;
	}

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
	if(list_ == nullptr) {
		return result;
	}

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
	if(list_ == nullptr) {
		return result;
	}

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
		if(map_->refcount() > 1 || map_->modcount > 0) {
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
		if(list_ == nullptr) {
			return true;
		}

		if(list_->refcount() > 1) {
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
	g_variant_thread_info->last_query_map = variant();

	if(is_map()) {
		if(map_->refcount() > 1) {
			map_->dec_reference();
			map_ = new variant_map(*map_);
			map_->add_reference();
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
	g_variant_thread_info->last_query_map = variant();

	if(is_map()) {
		if(map_->refcount() > 1) {
			map_->dec_reference();
			map_ = new variant_map(*map_);
			map_->add_reference();
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

	return nullptr;
}

variant* variant::get_index_mutable(int index)
{
	if(is_list()) {
		if(index >= 0 && static_cast<unsigned>(index) < num_elements()) {
			return &list_->begin[index];
		}
	}

	return nullptr;
}

void variant::weaken()
{
	if(type_ == VARIANT_TYPE_CALLABLE) {
		variant_weak* weak = new variant_weak;
		weak->refcount++;
		weak->ptr = ffl::weak_ptr<game_logic::FormulaCallable>(mutable_callable_);

		release();
		type_ = VARIANT_TYPE_WEAK;
		weak_ = weak;
	}
}

void variant::strengthen()
{
	if(is_weak()) {
		auto p = weak_->ptr.get();
		*this = variant(p.get());
	}
}

variant variant::bind_closure(const game_logic::FormulaCallable* callable)
{
	must_be(VARIANT_TYPE_FUNCTION);

	variant result;
	result.type_ = VARIANT_TYPE_FUNCTION;
	result.fn_ = new variant_fn(*fn_);
	result.fn_->add_reference();
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
	result.fn_->add_reference();
	result.fn_->bound_args.insert(result.fn_->bound_args.end(), args.begin(), args.end());

	return result;
}

void variant::get_mutable_closure_ref(std::vector<ffl::IntrusivePtr<const game_logic::FormulaCallable>*>& result)
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
	return std::max(0, static_cast<int>(fn_->type->arg_names.size()) - static_cast<int>(fn_->bound_args.size()) - fn_->type->num_default_args());
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
		return static_cast<int>(generic_fn_->type->arg_names.size()) - static_cast<int>(generic_fn_->bound_args.size());
	}

	must_be(VARIANT_TYPE_FUNCTION);
	return static_cast<int>(fn_->type->arg_names.size()) - static_cast<int>(fn_->bound_args.size());
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
			for(unsigned m = 0; m != types.size(); ++m) {
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

boost::uuids::uuid variant::as_callable_loading() const
{
	must_be(VARIANT_TYPE_CALLABLE_LOADING);
	return callable_loading_->uuid;
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
			// FIXME - Possible bug - `as_string()` requires type string via `must_be(variant::TYPE)`. `v.as_string()` will fail because `v` is dictionary in this flow.
			return variant(as_string() + v.as_string());
		} else if(v.type_ == VARIANT_TYPE_STRING) {
			return variant(as_string() + v.as_string());
		}

		std::string s;
		v.serializeToString(s);
		return variant(as_string() + s);
	}

	if(v.type_ == VARIANT_TYPE_STRING) {
		std::string s;
		serializeToString(s);
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
			if(list_ == nullptr) {
				return v;
			} else if(v.list_ == nullptr) {
				return *this;
			}

			const size_t new_size = num_elements() + v.num_elements();

			std::vector<variant> res;
			res.reserve(new_size);
			for(size_t i = 0; i < list_->size(); ++i) {
				const variant& var = list_->begin[i];
				res.push_back(var);
			}

			for(size_t j = 0; j < v.list_->size(); ++j) {
				const variant& var = v.list_->begin[j];
				res.push_back(var);
			}

			return variant(&res);
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
		game_logic::FormulaObject* obj = try_convert<game_logic::FormulaObject>();
		if(obj && v.is_map()) {
			ffl::IntrusivePtr<game_logic::FormulaObject> new_obj(obj->clone());
			const std::map<variant,variant>& m = v.as_map();
			for(std::map<variant,variant>::const_iterator i = m.begin();
			    i != m.end(); ++i) {
				i->first.must_be(VARIANT_TYPE_STRING);
				new_obj->mutateValue(i->first.as_string(), i->second);
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
		if(list_ == nullptr) {
			return variant(&res);
		}

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

/**  Will return _one_ for _two_, _four_, _eight_, _sixteen_... */
uint_fast8_t count_bits_set(uint32_t n)
{
	uint_fast8_t returning = 0;
	while (n) {
		returning += n & 1;
		n >>= 1;
	}
	return returning;
}

/**  Given the way `operator^` is currently implemented, when the exponent
 * variant is typed `int`, arithmetic overflows can occur in a way so that the
 * result does not change its sign incorrectly, but becomes zero incorrectly,
 * so returning zero incorrectly for that exponent, but also for every exponent
 * bigger than that, as long as it shares the `int` type.
 *   This function simply returns whenever there is no possibility for an
 * exponentiation to collapse to zero.
 *   This function is aimed to prevent execution when detecting conditions for
 * an exponentiation to collapse to zero. */
void prevent_invalid_collapse_in_zero(
		const int_fast32_t & base, const int_fast32_t & exponent)
{
	ASSERT_LOG(exponent >= 1, "precondition failed");
	if (base >= 0) {
		return;
	}
	if (base == -2147483648) {
		//   Precondition on exponent makes possible to assume this is
		// safe when exponent is one, else unsafe.
		ASSERT_LOG(exponent == 1,
				"prevented arithmetic overflow, at `prevent_invalid_collapse_in_zero(const int_fast32_t & base = " << base << ", const int_fast32_t & exponent = " << exponent << ")`");
		return;
	}
	//   Can do this because base is less than `-2147483648` (`-2^31`) and
	// the positive range of the 32 bit unsigned ends in `2147483647`
	// (`2 ^ 31 - 1`).
	const int_fast32_t minus_base = -base;
	if (count_bits_set(minus_base) > 1) {
		return;
	}
	bool arithmetic_overflow = false;
	switch (minus_base) {
	case 1 << 0:  //   1
		//   Can not overflow, because it oscillates back and forth.
		return;
	case 1 << 1:  //   2
		arithmetic_overflow = exponent >= 32;
		break;
	case 1 << 2:  //   4
		arithmetic_overflow = exponent >= 16;
		break;
	case 1 << 3:
		arithmetic_overflow = exponent >= 11;
		break;
	case 1 << 4:  //   16
		arithmetic_overflow = exponent >= 8;
		break;
	case 1 << 5:
		arithmetic_overflow = exponent >= 7;
		break;
	case 1 << 6:  //   64
		arithmetic_overflow = exponent >= 6;
		break;
	case 1 << 7:
		arithmetic_overflow = exponent >= 5;
		break;
	case 1 << 8:
	case 1 << 9:  //   512
	case 1 << 10:
		arithmetic_overflow = exponent >= 4;
		break;
	case 1 << 11:
	case 1 << 12:  //   4096
	case 1 << 13:
	case 1 << 14:
	case 1 << 15:  //   32768
		arithmetic_overflow = exponent >= 3;
		break;
	case 1 << 16:
	case 1 << 17:
	case 1 << 18:  //   262144
	case 1 << 19:
	case 1 << 20:
	case 1 << 21:  //   2097152
	case 1 << 22:
	case 1 << 23:
	case 1 << 24:  //   16777216
	case 1 << 25:
	case 1 << 26:
	case 1 << 27:  //   134217728
	case 1 << 28:
	case 1 << 29:
	case 1 << 30:  //   1073741824
		arithmetic_overflow = exponent >= 2;
		break;
	//   2147483648 does not fit 32 bit two's complement.
// 	default:
// 		ASSERT_LOG(false, "unreachable code executed, at `prevent_invalid_collapse_in_zero(base: " << base << ", exponent: " << exponent << ")`");
// 		break;
	}
	ASSERT_LOG(!arithmetic_overflow,
			"prevented arithmetic overflow, at `prevent_invalid_collapse_in_zero`, performing `" << base << " ^ " << exponent << '`');
}

variant variant::operator^(const variant& v) const
{
	//for the common case of exponentiation by a positive integer,
	//we use our own fixed-point impl. Otherwise we just use the
	//system pow(). TODO: eventually we want to always use fixed-point.
	if(v.type_ == VARIANT_TYPE_INT && v.as_int() >= 1) {
		int num = v.as_int();
		if (type_ == VARIANT_TYPE_INT) {
			prevent_invalid_collapse_in_zero(as_int(), num);
		}
		variant result = *this;
		const variant integer_zero(0);
		const bool this_is_positive = * this > integer_zero;
		const bool this_is_negative = * this < integer_zero;
		while(num > 1) {
			const bool result_was_positive_at_iteration_start =
					result > integer_zero;
			const bool result_was_negative_at_iteration_start =
					result < integer_zero;
			result = result * *this;
			const bool result_is_positive_at_iteration_end =
					result > integer_zero;
			const bool result_is_negative_at_iteration_end =
					result < integer_zero;
			bool arithmetic_overflow = false;
			if (this_is_positive) {
				if (result_was_positive_at_iteration_start &&
						result_is_negative_at_iteration_end) {
					arithmetic_overflow = true;
				}
			}
			if (this_is_negative) {
				if (result_was_positive_at_iteration_start &&
						result_is_positive_at_iteration_end) {
					arithmetic_overflow = true;
				}
				if (result_was_negative_at_iteration_start &&
						result_is_negative_at_iteration_end) {
					arithmetic_overflow = true;
				}
			}
			ASSERT_LOG(!arithmetic_overflow,
					"prevented arithmetic overflow, at `operator^`, performing `" << * this << " ^ " << v << '`'
					<< " (additional debug info: {" <<
					"variant_type_to_string(type_): " << variant_type_to_string(type_) << ", " <<
					"result: " << result << ", " <<
					"num: " << num << ", " <<
					"this_is_positive: " << this_is_positive << ", " <<
					"this_is_negative: " << this_is_negative << ", " <<
					"result_was_positive_at_iteration_start: " << result_was_positive_at_iteration_start << ", " <<
					"result_was_negative_at_iteration_start: " << result_was_negative_at_iteration_start << ", " <<
					"result_is_positive_at_iteration_end: " << result_is_positive_at_iteration_end << ", " <<
					"result_is_negative_at_iteration_end: " << result_is_negative_at_iteration_end << '}' <<
					')');
			--num;
		}

		return result;
	}

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
			if(!is_numeric() || !v.is_numeric()) {
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

	case VARIANT_TYPE_ENUM: {
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
		if((type_ == VARIANT_TYPE_DECIMAL && v.is_numeric()) ||
		   (v.type_ == VARIANT_TYPE_DECIMAL && is_numeric())) {
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

	case VARIANT_TYPE_ENUM: {
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
	if(this == &g_variant_thread_info->UnfoundInMapNullVariant) {
		const debug_info* info = g_variant_thread_info->last_failed_query_map.get_debug_info();
		if(info) {
			generate_error(formatter() << "In object at " << *info->filename << " " << info->line << " (column " << info->column << ") did not find attribute " << g_variant_thread_info->last_failed_query_key << " which was expected to be a " << variant_type_to_string(t));
		} else if(g_variant_thread_info->last_failed_query_map.get_source_expression()) {
			generate_error(formatter() << "Map object generated in FFL was expected to have key '" << g_variant_thread_info->last_failed_query_key << "' of type " << variant_type_to_string(t) << " but this key wasn't found. The map was generated by this expression:\n" << g_variant_thread_info->last_failed_query_map.get_source_expression()->debugPinpointLocation());
		}
	}

	if(g_variant_thread_info->last_query_map.is_map() && g_variant_thread_info->last_query_map.get_debug_info()) {
		for(std::map<variant,variant>::const_iterator i = g_variant_thread_info->last_query_map.map_->elements.begin(); i != g_variant_thread_info->last_query_map.map_->elements.end(); ++i) {
			if(this == &i->second) {
				const debug_info* info = i->first.get_debug_info();
				if(info == nullptr) {
					info = g_variant_thread_info->last_query_map.get_debug_info();
				}
				generate_error(formatter() << "In object at " << *info->filename << " " << info->line << " (column " << info->column << ") attribute for " << i->first << " was " << *this << ", which is a " << variant_type_to_string(type_) << ", must be a " << variant_type_to_string(t));
				
			}
		}
	} else if(g_variant_thread_info->last_query_map.is_map() && g_variant_thread_info->last_query_map.get_source_expression()) {
		for(std::map<variant,variant>::const_iterator i = g_variant_thread_info->last_query_map.map_->elements.begin(); i != g_variant_thread_info->last_query_map.map_->elements.end(); ++i) {
			if(this == &i->second) {
				std::ostringstream expression;
				if(g_variant_thread_info->last_failed_query_map.get_source_expression()) {
					expression << " The map was generated by this expression:\n" << g_variant_thread_info->last_failed_query_map.get_source_expression()->debugPinpointLocation();
				}

				generate_error(formatter() << "Map object generated in FFL was expected to have key '" << g_variant_thread_info->last_failed_query_key << "' of type " << variant_type_to_string(t) << " but this key was of type " << variant_type_to_string(i->second.type_) << " instead." << expression.str());
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

void variant::serializeToString(std::string& str) const
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
	case VARIANT_TYPE_ENUM:
		str += "enum " + g_enum_vector[int_value_];
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
		const game_logic::WmlSerializableFormulaCallable* obj = try_convert<game_logic::WmlSerializableFormulaCallable>();
		if(obj) {
			//we have an object that is to be serialized into WML. However,
			//it might be present in the level or a reference to it held
			//from multiple objects. So we record the address of it and
			//register it to be recorded seperately.
			char buf[256];
			sprintf(buf, "deserialize('%s')", write_uuid(obj->uuid()).c_str());
			str += buf;
			return;
		}

		callable_->serialize(str);
		break;
	}
	case VARIANT_TYPE_LIST: {
		str += "[";
		bool first_time = true;
		for(size_t i=0; list_ != nullptr && i < list_->size(); ++i) {
			const variant& var = list_->begin[i];
			if(!first_time) {
				str += ",";
			}
			first_time = false;
			var.serializeToString(str);
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
			i->first.serializeToString(str);
			str += ": ";
			i->second.serializeToString(str);
		}
		str += "}";
		break;
	}
	case VARIANT_TYPE_STRING: {
		if( !string_->str.empty() ) {
			if(string_->str[0] == '~' && string_->str[string_->str.length()-1] == '~') {
				str += string_->str;
			} else {
				if(strchr(string_->str.c_str(), '\'')) {
					str += "q(";
					str += string_->str;
					str += ")";
				} else {
					str += "'";
					str += string_->str;
					str += "'";
				}
			}
		}
		break;
	}
	case VARIANT_TYPE_FUNCTION:
		if(fn_->fn) {
			LOG_ERROR("ATTEMPT TO SERIALIZE FUNCTION: " << fn_->fn->str());
		}
		assert(false);
	default:
		assert(false);
	}
}

void variant::serialize_from_string(const std::string& str)
{
	try {
		*this = game_logic::Formula(variant(str)).execute();
	} catch(...) {
		*this = variant(str);
	}
}

variant variant::create_variant_under_construction(boost::uuids::uuid id)
{
	variant v;
	if(game_logic::wmlFormulaCallableReadScope::try_load_object(id, v)) {
		return v;
	}

	v.type_ = VARIANT_TYPE_CALLABLE_LOADING;
	v.callable_loading_ = new variant_uuid(id);
	v.callable_loading_->add_reference();
	return v;
}

int variant::refcount() const
{

	switch(type_) {
	case VARIANT_TYPE_LIST:
		if(list_ == nullptr) {
			return 1;
		}

		return list_->refcount();
		break;
	case VARIANT_TYPE_STRING:
		return string_->refcount;
		break;
	case VARIANT_TYPE_MAP:
		return map_->refcount();
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
		if(list_ == nullptr) {
			return;
		}

		list_->dec_reference();
		list_ = new variant_list(*list_);
		list_->add_reference();
		for(variant& v : list_->elements) {
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

		map_->dec_reference();

		variant_map* vm = new variant_map;
		vm->add_reference();
		vm->info = map_->info;
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
	case VARIANT_TYPE_ENUM:
		return "enum " + g_enum_vector[int_value_];
	case VARIANT_TYPE_DECIMAL: {
		std::string res;
		serializeToString(res);
		return res;
	}
	case VARIANT_TYPE_CALLABLE_LOADING:
		return "(object loading)";
	case VARIANT_TYPE_CALLABLE:
		return "(object)";
	case VARIANT_TYPE_LIST: {
		std::string res = "";
		for(size_t i=0; list_ && i < list_->size(); ++i) {
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

std::string variant::to_debug_string(std::vector<const game_logic::FormulaCallable*>* seen) const
{
	std::vector<const game_logic::FormulaCallable*> seen_stack;
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
	case VARIANT_TYPE_ENUM:
		s << "enum " << g_enum_vector[int_value_];
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
		sprintf(buf, "(loading %s)", write_uuid(callable_loading_->uuid).c_str());
		s << buf;
		break;
	}

	case VARIANT_TYPE_CALLABLE: {
		ToDebugStringDepthContext depth_tracker;
		if(depth_tracker.isTooDeep()) {
			s << "(...)";
			break;
		}

		std::string str = callable_->toDebugString();
		if(str.empty() == false) {
			s << str;
			break;
		}

		char buf[64];
		sprintf(buf, "(object at address %p)", callable_);
		s << buf << "{";
		if(std::find(seen->begin(), seen->end(), callable_) == seen->end()) {
			seen->push_back(callable_);

			const variant_type_ptr type = get_variant_type_from_value(*this);
			const game_logic::FormulaCallableDefinition* def = type->getDefinition();
			if(def) {
				bool first = true;
				for(int slot = 0; slot < def->getNumSlots(); ++slot) {
					if(!first) {
						s << ",\n";
					}

					variant value;

					try {
						const assert_recover_scope scope(SilenceAsserts);
						if(def->supportsSlotLookups()) {
							value = callable_->queryValueBySlot(slot);
						} else {
							value = callable_->queryValue(def->getEntry(slot)->id);
						}
					} catch(...) {
						value = variant("(Unknown)");
					}

					first = false;
					s << def->getEntry(slot)->id << ": " << value.to_debug_string(seen);
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

		if(fn_->fn) {
			s << fn_->fn->str();
		}
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
		for(const variant& v : multi_fn_->functions) {
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

std::string variant::write_json(bool pretty, unsigned int flags) const
{
	std::ostringstream s;
	if(pretty) {
		write_json_pretty(s, "", flags);
	} else {
		write_json(s, flags);
	}
	return s.str();
}

void variant::write_json(std::ostream& s, unsigned int flags) const
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
	case VARIANT_TYPE_ENUM: {
		s << "\"@eval enum " << g_enum_vector[int_value_] << "\"";
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

				if (str.size() >= 7 && std::equal(str.begin(), str.begin() + 7, "\"@eval ")) {
					s << str << ":";
				}
				else {
					boost::replace_all(str, "\"", "\\\"");
					s << "\"@eval " << str << "\":";
				}
			}

			i->second.write_json(s, flags);
		}

		s << "}";
		return;
	}
	case VARIANT_TYPE_LIST: {
		s << "[";

		if(list_ != nullptr) {
			for(std::vector<variant>::const_iterator i = list_->begin;
			    i != list_->end; ++i) {
				if(i != list_->begin) {
					s << ',';
				}
	
				i->write_json(s, flags);
			}
		}

		s << "]";
		return;
	}
	case VARIANT_TYPE_STRING: {
		const std::string& str = string_->translated_from.empty() ? string_->str : string_->translated_from;
		const char delim = string_->translated_from.empty() ? '"' : '~';
		if(std::count(str.begin(), str.end(), '\\') 
			|| std::count(str.begin(), str.end(), delim) 
			|| ((flags&JSON_COMPLIANT) && std::count(str.begin(), str.end(), '\n'))) {
			//escape the string
			s << delim;
			for(std::string::const_iterator i = str.begin(); i != str.end(); ++i) {
				if(*i == '\\' || *i == delim) {
					s << '\\';
				}

				if((flags&JSON_COMPLIANT) && *i == '\n') {
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
		serializeToString(str);
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
		LOG_ERROR("Illegal type to serialize: " << to_debug_string());
		s << "q(ILLEGAL TYPE TO SERIALIZE: " << to_debug_string() << ")";
		return;
	}
}

void variant::write_function(std::ostream& s) const
{
	assert(fn_->fn);

	//Serialize the closure along with the object, if we can.
	const bool serialize_closure = fn_->callable && dynamic_cast<const game_logic::WmlSerializableFormulaCallable*>(fn_->callable.get());
	if(serialize_closure) {
		s << "delay_until_end_of_loading(q(bind_closure(";
	}
	
	s << "def(";
	const int default_base = static_cast<int>(fn_->type->arg_names.size()) - static_cast<int>(fn_->type->default_args.size());
	for(std::vector<std::string>::const_iterator p = fn_->type->arg_names.begin(); p != fn_->type->arg_names.end(); ++p) {
		if(p != fn_->type->arg_names.begin()) {
			s << ",";
		}

		s << *p;
		
		const int index = static_cast<int>(p - fn_->type->arg_names.begin());
		if(index >= default_base) {
			variant v = fn_->type->default_args[index - default_base];
			std::string str;
			v.serializeToString(str);
			s << "=" << str;
		}
	}

	s << ") " << fn_->fn->str();

	if(serialize_closure) {
		std::string str;
		variant(fn_->callable.get()).serializeToString(str);
		s << "," << str << ")))";
	}
}

void variant::write_json_pretty(std::ostream& s, std::string indent, unsigned int flags) const
{
	switch(type_) {
	case VARIANT_TYPE_MAP: {
		s << "{";
		indent += "\t";
		for(std::map<variant,variant>::const_iterator i = map_->elements.begin(); i != map_->elements.end(); ++i) {
			if(i != map_->elements.begin()) {
				s << ',';
			}

			s << "\n" << indent;
			if(i->first.is_string()) {
				s << '"' << i->first.string_cast() << "\": ";
			}
			else {
				std::string str = i->first.write_json(true, flags);

				if (str.size() >= 7 && std::equal(str.begin(), str.begin() + 7, "\"@eval ")) {
					s << str << ": ";
				}
				else {
					boost::replace_all(str, "\"", "\\\"");
					s << "\"@eval " << str << "\": ";
				}
			}

			i->second.write_json_pretty(s, indent, flags);
		}
		indent.resize(indent.size()-1);

		s << "\n" << indent << "}";
		return;
	}
	case VARIANT_TYPE_LIST: {
		bool found_non_scalar = false;
		if(list_ != nullptr) {
			for(std::vector<variant>::const_iterator i = list_->begin;
			    i != list_->end; ++i) {
				if(i->is_list() || i->is_map()) {
					found_non_scalar = true;
					break;
				}
			}
		}

		const bool expanded = list_ && list_->begin != list_->end && (flags&EXPANDED_LISTS);
		if(!found_non_scalar && !expanded) {
			write_json(s, flags);
			return;
		}


		s << "[";

		indent += "\t";
		if(list_ != nullptr) {
			for(std::vector<variant>::const_iterator i = list_->begin;
			    i != list_->end; ++i) {
				if(i != list_->begin) {
					s << ',';
				}
	
				s << "\n" << indent;

				i->write_json_pretty(s, indent, flags);
			}
		}

		indent.resize(indent.size()-1);

		if(list_ && list_->size() > 0) {
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

void variant::add_formula_using_this(const game_logic::Formula* f)
{
	if(is_string()) {
		string_->formulae_using_this.push_back(f);
	}
}

void variant::remove_formula_using_this(const game_logic::Formula* f)
{
	if(is_string()) {
		string_->formulae_using_this.erase(std::remove(string_->formulae_using_this.begin(), string_->formulae_using_this.end(), f), string_->formulae_using_this.end());
	}
}

const std::vector<const game_logic::Formula*>* variant::formulae_using_this() const
{
	if(is_string()) {
		return &string_->formulae_using_this;
	} else {
		return nullptr;
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

#ifdef DEBUG_GARBAGE_COLLECTOR
std::set<variant*>& get_all_global_variants()
{
	static std::set<variant*>* result = new std::set<variant*>;
	return *result;
}

void registerGlobalVariant(variant* v)
{
	get_all_global_variants().insert(v);
}

void unregisterGlobalVariant(variant* v)
{
	get_all_global_variants().erase(v);
}
#endif

UNIT_TEST(variant_decimal)
{
	variant d(9876000, variant::DECIMAL_VARIANT);
	variant d2(4000, variant::DECIMAL_VARIANT);
	variant zero_decimal(0, variant::DECIMAL_VARIANT);
	CHECK_EQ(d.as_decimal().value(), 9876000);
	CHECK_EQ(d.as_int(), 9);
	CHECK_EQ(d.string_cast(), "9.876");
	CHECK_EQ((d + d2).as_decimal().value(), 9880000);
	CHECK_NE(zero_decimal, variant());
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

/**  Log (debug) unit test variable name and value. */
#define LOG_DEBUG_UT_VAR(test_name, variable_suffix, variable_name)         \
	LOG_DEBUG(                                                          \
			"test__" << test_name << "__" << variable_suffix    \
			<< ": " << variable_name)

/**
 *   Name `name` a pow unit test where must be true that:
 *
 *     `base ^ exponent - expected_pow == 0`
 */
#define VARIANT_EXACT_POW_UNIT_TEST(                                          \
		name, base, exponent, expected_pow) UNIT_TEST (name) {        \
	const variant test__##name__base = variant(base);                     \
	LOG_DEBUG_UT_VAR(#name, "base", test__##name__base);                  \
	const variant test__##name__exponent = variant(exponent);             \
	LOG_DEBUG_UT_VAR(#name, "exponent", test__##name__exponent);          \
	const variant test__##name__expected_pow = variant(expected_pow);     \
	LOG_DEBUG_UT_VAR(#name, "expected_pow", test__##name__expected_pow);  \
	const variant test__##name__actual_pow =                              \
			test__##name__base ^ test__##name__exponent;          \
	LOG_DEBUG_UT_VAR(#name, "actual_pow", test__##name__actual_pow);      \
	CHECK_EQ(test__##name__expected_pow, test__##name__actual_pow); }

/**
 *   Name `name` a pow unit test where must be true that:
 *
 *     `abs(base ^ exponent - expected_pow) <= error`
 */
#define VARIANT_APPROXIMATE_POW_UNIT_TEST(                                    \
		name, base, exponent, expected_pow, error) UNIT_TEST (name) { \
	const variant test__##name__base = variant(base);                     \
	LOG_DEBUG_UT_VAR(#name, "base", test__##name__base);                  \
	const variant test__##name__exponent = variant(exponent);             \
	LOG_DEBUG_UT_VAR(#name, "exponent", test__##name__exponent);          \
	const variant test__##name__expected_pow = variant(expected_pow);     \
	LOG_DEBUG_UT_VAR(#name, "expected_pow", test__##name__expected_pow);  \
	const variant test__##name__error = variant(error);                   \
	LOG_DEBUG_UT_VAR(#name, "error", test__##name__error);                \
	const variant test__##name__actual_pow =                              \
			test__##name__base ^ test__##name__exponent;          \
	LOG_DEBUG_UT_VAR(#name, "actual_pow", test__##name__actual_pow);      \
	const variant test__##name__diff = test__##name__actual_pow -         \
			test__##name__expected_pow;                           \
		const variant zero(0);                                        \
	const variant test__##name__abs_diff = test__##name__diff > zero ?    \
			test__##name__diff : - test__##name__diff;            \
	LOG_DEBUG_UT_VAR(#name, "abs_diff", test__##name__abs_diff);          \
		ASSERT_LOG(                                                   \
			test__##name__abs_diff <= test__##name__error,        \
				"math imprecision error happened" <<          \
				", expected error less than or equal to " <<  \
			test__##name__error << ", but actual error is " <<    \
			test__##name__abs_diff <<                             \
			", rerun " <<                                         \
				"setting log level to DEBUG for finer " <<    \
				"grain messages (--log-level=debug)"); }

VARIANT_EXACT_POW_UNIT_TEST(pow_test_00, 0, 1, 0)

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_01, 0, 0, 1, decimal::from_string("0.000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_02a0a, 3, 0, 1, decimal::from_string("0.000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_02a1, decimal::from_string("3.0"),
		decimal::from_string("0.0"), 1,
		decimal::from_string("0.000001"))

VARIANT_EXACT_POW_UNIT_TEST(pow_test_02b0, 3, 1, 3)

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_02b1, decimal::from_string("3.0"),
		decimal::from_string("1.0"), 3,
		decimal::from_string("0.000001"))

VARIANT_EXACT_POW_UNIT_TEST(pow_test_02c0a, 3, 2, 9)

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_02c0b, 3, decimal::from_string("2.0"),
		decimal::from_string("9.0"), decimal::from_string("0.000001"))

VARIANT_EXACT_POW_UNIT_TEST(
		pow_test_02c1, decimal::from_string("3.0"), 2, 9)

VARIANT_EXACT_POW_UNIT_TEST(pow_test_02d0a, 3, 3, 27)

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_02d1, decimal::from_string("3.0"),
		decimal::from_string("3.0"), 27,
		decimal::from_string("0.000001"))

VARIANT_EXACT_POW_UNIT_TEST(pow_test_02e0, 3, 4, 81)

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_02e1, decimal::from_string("3.0"),
		decimal::from_string("4.0"), 81,
		decimal::from_string("0.000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_03a0, -3, 0, 1, decimal::from_string("0.000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_03a1, decimal::from_string("-3.0"),
		decimal::from_string("0.0"), 1,
		decimal::from_string("0.000001"))

VARIANT_EXACT_POW_UNIT_TEST(pow_test_03b0, -3, 1, -3)

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_03b1, decimal::from_string("-3.0"),
		decimal::from_string("1.0"), -3,
		decimal::from_string("0.000001"))

VARIANT_EXACT_POW_UNIT_TEST(pow_test_03c0, -3, 2, 9)

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_03c1, decimal::from_string("-3.0"),
		decimal::from_string("2.0"), 9,
		decimal::from_string("0.000001"))

VARIANT_EXACT_POW_UNIT_TEST(pow_test_03d0, -3, 3, -27)

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_03d1, decimal::from_string("-3.0"),
		decimal::from_string("3.0"), -27,
		decimal::from_string("0.000001"))

VARIANT_EXACT_POW_UNIT_TEST(pow_test_03e0, -3, 4, 81)

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_03e1, decimal::from_string("-3.0"),
		decimal::from_string("4.0"), 81,
		decimal::from_string("0.000001"))

VARIANT_EXACT_POW_UNIT_TEST(pow_test_04a0, -3, 5, -243)
VARIANT_EXACT_POW_UNIT_TEST(pow_test_04a1, -3, 5, decimal::from_string("-243.0"))
VARIANT_APPROXIMATE_POW_UNIT_TEST(pow_test_04a2, -3, decimal::from_string("5.0"), -243, decimal::from_string("0.000001"))
VARIANT_APPROXIMATE_POW_UNIT_TEST(pow_test_04a3, -3, decimal::from_string("5.0"), decimal::from_string("-243.0"), decimal::from_string("0.000001"))
VARIANT_EXACT_POW_UNIT_TEST(pow_test_04a4, decimal::from_string("-3.0"), 5, -243)
VARIANT_EXACT_POW_UNIT_TEST(pow_test_04a5, decimal::from_string("-3.0"), 5, decimal::from_string("-243.0"))
VARIANT_APPROXIMATE_POW_UNIT_TEST(pow_test_04a6, decimal::from_string("-3.0"), decimal::from_string("5.0"), -243, decimal::from_string("0.000001"))
VARIANT_APPROXIMATE_POW_UNIT_TEST(pow_test_04a7, decimal::from_string("-3.0"), decimal::from_string("5.0"), decimal::from_string("-243.0"), decimal::from_string("0.000001"))

VARIANT_EXACT_POW_UNIT_TEST(pow_test_04b0, -3, 5, -243)
VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_04b1, decimal::from_string("-3.0"),
		decimal::from_string("5.0"), -243,
		decimal::from_string(".000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_04c, decimal::from_string("-3.0"),
		decimal::from_string("5.0"), decimal::from_string("-243.0"),
		decimal::from_string("0.000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_05a, decimal::from_string("2.001"), 16,
		decimal::from_string("66062.258674"),
		decimal::from_string("0.000631"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_05b, decimal::from_string("2.001"),
		decimal::from_string("16.0"),
		decimal::from_string("66062.258674"),
		decimal::from_string("0.000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_06a, -333, 0, 1, decimal::from_string("0.000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_06b, -333, decimal::from_string("0.0"), 1,
		decimal::from_string("0.000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_06c, decimal::from_string("-333.0"),
		decimal::from_string("0.0"), 1,
		decimal::from_string("0.000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_07a, decimal::from_string("-442.001"), 2,
		decimal::from_string("195364.884"),
		decimal::from_string("0.000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_07b, decimal::from_string("-442.001"),
		decimal::from_string("2.0"),
		decimal::from_string("195364.884"),
		decimal::from_string("0.000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_08a, decimal::from_string("-442.001"), 3,
		decimal::from_string("-86351474.093326"),
		decimal::from_string("0.000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_08b, decimal::from_string("-442.001"),
		decimal::from_string("3.0"),
		decimal::from_string("-86351474.093326"),
		decimal::from_string("0.000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_09a, decimal::from_string("1.001"),
		decimal::from_string("9999.0"),
		decimal::from_string("21894.786552"),
		decimal::from_string("0.000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_09b, decimal::from_string("1.001"), 9999,
		decimal::from_string("21894.786552"),
		decimal::from_string("10.8566"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_10a, decimal::from_string("-1.021"), 939,
		decimal::from_string("-298656395.733370"),
		decimal::from_string("7265.158963"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_10b, decimal::from_string("-1.021"),
		decimal::from_string("939.0"),
		decimal::from_string("-298656395.733370"),
		decimal::from_string("7265.158963"))

//VARIANT_APPROXIMATE_POW_UNIT_TEST(
//		pow_test_11a, decimal::from_string("-1.021"), 1300,
//		decimal::from_string("541333262032.771060"),
//		decimal::from_string("13168551.833481"))

//VARIANT_APPROXIMATE_POW_UNIT_TEST(
//		pow_test_11b, decimal::from_string("-1.021"),
//		decimal::from_string("1300.0"),
//		decimal::from_string("541333262032.771060"),
//		decimal::from_string("0.07"))

//VARIANT_APPROXIMATE_POW_UNIT_TEST(
//		pow_test_12, decimal::from_string("-1.023"), 1399,
//		decimal::from_string("-65465360432130.221993"),
//
//// 		"Inf" // XXX ???
//// 		"Infinity" // XXX ???
//// 		decimal::from_string("Inf") // XXX ???
//// 		decimal::from_string("Infinity") // XXX ???
//		decimal::from_string("999999999999.999999")
//
//		)

//   Some builds can have down to only 0.000001 error here! Some hosts
// might be yielding 0, others would yield `0.000021`.
//VARIANT_APPROXIMATE_POW_UNIT_TEST(
//		pow_test_13, 36, -3, decimal::from_string("0.000021"),
//		decimal::from_string("0.000021"))

//   Some builds can have down to only 0.000001 error here! Some hosts
// might be yielding `0.341279`, others would yield `0.340881`.
//VARIANT_APPROXIMATE_POW_UNIT_TEST(
//		pow_test_14, 36, decimal::from_string("-.3"),
//		decimal::from_string("0.341279"),
//		decimal::from_string("0.000399"))

VARIANT_EXACT_POW_UNIT_TEST(pow_test_15, 2, -3, 0)

VARIANT_EXACT_POW_UNIT_TEST(pow_test_16, 3, -5, 0)

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_17, 2, decimal::from_string("-3.0"),
		decimal::from_string("0.125"),
		decimal::from_string("0.000001"))

VARIANT_APPROXIMATE_POW_UNIT_TEST(
		pow_test_18, 3, decimal::from_string("-5.0"),
		decimal::from_string("0.004115"),
		decimal::from_string("0.000010"))

UNIT_TEST(good_variant_exponentiation_0) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffffffe);  //   -2
	variant b(min_32_bit_integer);
	b = a ^ variant(31);
	const variant expected(min_32_bit_integer);
	CHECK_EQ(expected, b);
}

//   `-2 ^ 32 = 4294967296`.
//
//   At the current implementation, `-2 ^ 32` will be transformed into
// `(-2 ^ 31) * (-2)`. `-2 ^ 31 = -2147483648`. `-2147483648` is
// represented as `0x80000000`, and `-2` as `0xfffffffe`.
//   The multiplication collapses to zero, see `-128 * -2` as a simpler
// example of the phenomenon:
//
//                         10000000  // 0x80 // -128
//                       x 11111110  // 0xfe // -2
//                       -+--------
//                        |00000000
//                       1|0000000
//                      10|000000
//                     100|00000
//                    1000|0000
//                   10000|000
//                  100000|00
//                 1000000|0
//                 -------+--------
//         excess  1111111|00000000  returned
//
//   Once it collapsed to zero, any further exponentiation step results
// in zero again.
UNIT_TEST(bad_variant_exponentiation_0) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffffffe);  //   -2
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(32);
		} catch (validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-2`, hosted in a 32 bit signed integer, raised to the power of `32`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_1) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffffffc);  //   -4
	variant b(min_32_bit_integer);
	b = a ^ variant(15);
	const variant expected((int_fast32_t) 0xc0000000);  //   -1073741824
	CHECK_EQ(expected, b);
}

//   `-4 ^ 16 = 4294967296`.
//
//   At the current implementation, `-4 ^ 16` will be transformed into
// `(-4 ^ 15) * (-4)`. `-4 ^ 15 = -1073741824`. `-1073741824` is
// represented as `0xc0000000`, and `-4` as `0xfffffffc`.
//   The multiplication collapses to zero, see `-64 * -4` as a simpler
// example of the phenomenon:
//
//                         11000000  // 0xc0 // -64
//                       x 11111100  // 0xfc // -4
//                       -+--------
//                        |00000000
//                       0|0000000
//                      11|000000
//                     110|00000
//                    1100|0000
//                   11000|000
//                  110000|00
//                 1100000|0
//                 -------+--------
//        excess  10111101|00000000  returned
//
//   Once it collapsed to zero, any further exponentiation step results
// in zero again.
UNIT_TEST(bad_variant_exponentiation_1) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffffffffc);  //   -4
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(16);
		} catch (validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-4`, hosted in a 32 bit signed integer, raised to the power of `16`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_2) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffffff8);  //   -8
	variant b(min_32_bit_integer);
	b = a ^ variant(10);
	const variant expected((int_fast32_t) 0x40000000);  //   1073741824
	CHECK_EQ(expected, b);
}

//   `-8 ^ 11 = 8589934592`.
//
//   At the current implementation, `-8 ^ 11` will be transformed into
// `(-8 ^ 10) * (-8)`. `-8 ^ 10 = 1073741824`. `1073741824` is
// represented as `0x40000000`, and `-8` as `0xfffffff8`.
//   The multiplication collapses to zero, see `64 * -8` as a simpler
// example of the phenomenon:
//
//                         01000000  // 0x40 // 64
//                       x 11111000  // 0xf8 // -8
//                       -+--------
//                        |00000000
//                       0|0000000
//                      00|000000
//                     010|00000
//                    0100|0000
//                   01000|000
//                  010000|00
//                 0100000|0
//                 -------+--------
//         excess  0111110|00000000  returned
//
//   Once it collapsed to zero, any further exponentiation step results
// in zero again.
UNIT_TEST(bad_variant_exponentiation_2) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffffff8);  //   -8
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(11);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-8`, hosted in a 32 bit signed integer, raised to the power of `11`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_3) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffffff0);  //   -16
	variant b(min_32_bit_integer);
	b = a ^ variant(7);
	const variant expected((int_fast32_t) 0xf0000000);  //   -268435456
	CHECK_EQ(expected, b);
}

//   `-16 ^ 8 = 4294967296`.
//
//   At the current implementation, `-16 ^ 8` will be transformed into
// `(-16 ^ 7) * (-16)`. `-16 ^ 7 = -268435456`. `-268435456` is
// represented as `0xf0000000`, and `-16` as `0xfffffff0`.
//   The multiplication collapses to zero, see `-16 * -16` as a simpler
// example of the phenomenon:
//
//                         11110000  // 0xf0 // -16
//                       x 11110000  // 0xf0 // -16
//                       -+--------
//                        |00000000
//                       0|0000000
//                      00|000000
//                     000|00000
//                    1111|0000
//                   11110|000
//                  111100|00
//                 1111000|0
//                 -------+--------
//        excess  11011001|00000000  returned
//
//   Once it collapsed to zero, any further exponentiation step results
// in zero again.
UNIT_TEST(bad_variant_exponentiation_3) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffffff0);  //   -16
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(8);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-16`, hosted in a 32 bit signed integer, raised to the power of `8`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_4) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffffffe0);  //   -32
	variant b(min_32_bit_integer);
	b = a ^ variant(6);
	const variant expected((int_fast32_t) 0x40000000);  //   1073741824
	CHECK_EQ(expected, b);
}

//   `-32 ^ 7 = -34359738368`.
//
//   At the current implementation, `-32 ^ 7` will be transformed into
// `(-32 ^ 6) * (-32)`. `-32 ^ 6 = 1073741824`. `1073741824` is
// represented as `0x40000000`, and `-32` as `0xffffffe0`.
//   The multiplication collapses to zero, see `64 * -32` as a simpler
// example of the phenomenon:
//
//                         01000000  // 0x40 // 64
//                       x 11100000  // 0xe0 // -32
//                       -+--------
//                        |00000000
//                       0|0000000
//                      00|000000
//                     000|00000
//                    0000|0000
//                   01000|000
//                  010000|00
//                 0100000|0
//                 -------+--------
//         excess  0111000|00000000  returned
//
//   Once it collapsed to zero, any further exponentiation step results
// in zero again.
UNIT_TEST(bad_variant_exponentiation_4) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffffffe0);  //   -32
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(7);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-32`, hosted in a 32 bit signed integer, raised to the power of `7`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_5) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffffffc0);  //   -64
	variant b(min_32_bit_integer);
	b = a ^ variant(5);
	const variant expected((int_fast32_t) 0xc0000000);  //   -1073741824
	CHECK_EQ(expected, b);
}

//   `-64 ^ 6 = -68719476736`.
//
//   At the current implementation, `-64 ^ 6` will be transformed into
// `(-64 ^ 5) * (-64)`. `-64 ^ 5 = -1073741824`. `-1073741824` is
// represented as `0xc0000000`, and `-64` as `0xffffffc0`.
//   The multiplication collapses to zero, see `-64 * -64` as a simpler
// example of the phenomenon:
//
//                         11000000  // 0xc0 // -64
//                       x 11000000  // 0xc0 // -64
//                       -+--------
//                        |00000000
//                       0|0000000
//                      00|000000
//                     000|00000
//                    0000|0000
//                   00000|000
//                  110000|00
//                 1100000|0
//                 -------+--------
//        excess  10010000|00000000  returned
//
//   Once it collapsed to zero, any further exponentiation step results
// in zero again.
UNIT_TEST(bad_variant_exponentiation_5) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffffffc0);  //   -64
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(6);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-64`, hosted in a 32 bit signed integer, raised to the power of `6`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_6) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffffff80);  //   -128
	variant b(min_32_bit_integer);
	b = a ^ variant(4);
	const variant expected((int_fast32_t) 0x10000000);  //   268435456
	CHECK_EQ(expected, b);
}

//   `-128 ^ 5 = -34359738368`.
//
//   At the current implementation, `-128 ^ 5` will be transformed into
// `(-128 ^ 4) * (-128)`. `-128 ^ 4 = 268435456`. `268435456` is
// represented as `0x10000000`, and `-128` as `0xffffff80`.
//   The multiplication collapses to zero, see `-16 * -128` as a simpler
// example of the phenomenon:
//
//                         00010000  // 0x10 // 16
//                       x 10000000  // 0x80 // -128
//                       -+--------
//                        |00000000
//                       0|0000000
//                      00|000000
//                     000|00000
//                    0000|0000
//                   00000|000
//                  000000|00
//                 0001000|0
//                 -------+--------
//         excess  0001000|00000000  returned
//
//   Once it collapsed to zero, any further exponentiation step results
// in zero again.
UNIT_TEST(bad_variant_exponentiation_6) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffffff80);  //   -128
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(5);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-128`, hosted in a 32 bit signed integer, raised to the power of `5`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_7) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffffff00);  //   -256
	variant b(min_32_bit_integer);
	b = a ^ variant(3);
	const variant expected((int_fast32_t) 0xff000000);  //   -16777216
	CHECK_EQ(expected, b);
}

UNIT_TEST(bad_variant_exponentiation_7) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffffff00);  //   -256
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(4);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-256`, hosted in a 32 bit signed integer, raised to the power of `4`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_8) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffffe00);  //   -512
	variant b(min_32_bit_integer);
	b = a ^ variant(3);
	const variant expected((int_fast32_t) 0xf8000000);  //   -134217728
	CHECK_EQ(expected, b);
}

UNIT_TEST(bad_variant_exponentiation_8) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffffe00);  //   -512
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(4);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-512`, hosted in a 32 bit signed integer, raised to the power of `4`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_9) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffffc00);  //   -1024
	variant b(min_32_bit_integer);
	b = a ^ variant(3);
	const variant expected((int_fast32_t) 0xc0000000);  //   -1073741824
	CHECK_EQ(expected, b);
}

UNIT_TEST(bad_variant_exponentiation_9) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffffc00);  //   -1024
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(4);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-1024`, hosted in a 32 bit signed integer, raised to the power of `4`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_10) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffff800);  //   -2048
	variant b(min_32_bit_integer);
	b = a ^ variant(2);
	const variant expected((int_fast32_t) 0x00400000);  //   4194304
	CHECK_EQ(expected, b);
}

UNIT_TEST(bad_variant_exponentiation_10) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffff800);  //   -2048
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(3);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-2048`, hosted in a 32 bit signed integer, raised to the power of `3`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_11) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffff000);  //   -4096
	variant b(min_32_bit_integer);
	b = a ^ variant(2);
	const variant expected((int_fast32_t) 0x01000000);  //   16777216
	CHECK_EQ(expected, b);
}

UNIT_TEST(bad_variant_exponentiation_11) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffff000);  //   -4096
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(3);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-4096`, hosted in a 32 bit signed integer, raised to the power of `3`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_12) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffffe000);  //   -8192
	variant b(min_32_bit_integer);
	b = a ^ variant(2);
	const variant expected((int_fast32_t) 0x04000000);  //   67108864
	CHECK_EQ(expected, b);
}

UNIT_TEST(bad_variant_exponentiation_12) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffffe000);  //   -8192
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(3);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-8192`, hosted in a 32 bit signed integer, raised to the power of `3`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_13) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffffc000);  //   -16384
	variant b(min_32_bit_integer);
	b = a ^ variant(2);
	const variant expected((int_fast32_t) 0x10000000);  //   268435456
	CHECK_EQ(expected, b);
}

UNIT_TEST(bad_variant_exponentiation_13) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffffc000);  //   -16384
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(3);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-16384`, hosted in a 32 bit signed integer, raised to the power of `3`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_14) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffff8000);  //   -32768
	variant b(min_32_bit_integer);
	b = a ^ variant(2);
	const variant expected((int_fast32_t) 0x40000000);  //   1073741824
	CHECK_EQ(expected, b);
}

UNIT_TEST(bad_variant_exponentiation_14) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffff8000);  //   -32768
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(3);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-32768`, hosted in a 32 bit signed integer, raised to the power of `3`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_15) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffff0000);  //   -65536
	variant b(min_32_bit_integer);
	b = a ^ variant(1);
	const variant expected((int_fast32_t) 0xffff0000);  //   -65536
	CHECK_EQ(expected, b);
}

UNIT_TEST(bad_variant_exponentiation_15) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xffff0000);  //   -65536
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(2);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-65536`, hosted in a 32 bit signed integer, raised to the power of `2`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_16) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffe0000);  //   -131072
	variant b(min_32_bit_integer);
	b = a ^ variant(1);
	const variant expected((int_fast32_t) 0xfffe0000);  //   -131072
	CHECK_EQ(expected, b);
}

UNIT_TEST(bad_variant_exponentiation_16) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffe0000);  //   -131072
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(2);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-131072`, hosted in a 32 bit signed integer, raised to the power of `2`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(good_variant_exponentiation_17) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffc0000);  //   -262144
	variant b(min_32_bit_integer);
	b = a ^ variant(1);
	const variant expected((int_fast32_t) 0xfffc0000);  //   -262144
	CHECK_EQ(expected, b);
}

UNIT_TEST(bad_variant_exponentiation_17) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a((int_fast32_t) 0xfffc0000);  //   -262144
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(2);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-262144`, hosted in a 32 bit signed integer, raised to the power of `2`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(variant_exponentiation_18) {}

UNIT_TEST(variant_exponentiation_19) {}

UNIT_TEST(variant_exponentiation_20) {
	//   -2097152
}

UNIT_TEST(variant_exponentiation_21) {}

UNIT_TEST(variant_exponentiation_22) {}

UNIT_TEST(variant_exponentiation_23) {
	//   -16777216
}

UNIT_TEST(variant_exponentiation_24) {}

UNIT_TEST(variant_exponentiation_25) {}

UNIT_TEST(variant_exponentiation_26) {
	//   -134217728
}

UNIT_TEST(variant_exponentiation_27) {}

UNIT_TEST(variant_exponentiation_28) {}

UNIT_TEST(variant_exponentiation_29) {
	//   -1073741824
}

UNIT_TEST(good_variant_exponentiation_30) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a(min_32_bit_integer);
	variant b(min_32_bit_integer);
	b = a ^ variant(1);
	const variant expected(min_32_bit_integer);
	CHECK_EQ(expected, b);
}

//   `-2147483648 ^ 2 = 4611686018427387904`.
//
//   At the current implementation, `-2147483648 ^ 2` will be
// transformed into `(-2147483648 ^ 1) * (-2147483648)`.
// `-2147483648 ^ 1 = -2147483648`. `-2147483648` is represented as
// `0x80000000`.
//   The multiplication collapses to zero, see `-128 * -128` as a simpler
// example of the phenomenon:
//
//                         10000000  // 0x80 // -128
//                       x 10000000  // 0x80 // -128
//                       -+--------
//                        |00000000
//                       0|0000000
//                      00|000000
//                     000|00000
//                    0000|0000
//                   00000|000
//                  000000|00
//                 1000000|0
//                 -------+--------
//         excess  1000000|00000000  returned
//
//   Once it collapsed to zero, any further exponentiation step results
// in zero again.
UNIT_TEST(bad_variant_exponentiation_30) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a(min_32_bit_integer);
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ variant(2);
		} catch (validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-2147483648`, hosted in a 32 bit signed integer, raised to the power of `2`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(bad_variant_exponentiation_31) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a(11);
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ a;
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `11`. hosted in a 32 bit signed integer, raised to the power of `11`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(bad_variant_exponentiation_32) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a(-11);
	const variant c(9);
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ c;
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-11`. hosted in a 32 bit signed integer, raised to the power of `9`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(bad_variant_exponentiation_33) {
	const int_fast32_t min_32_bit_integer = 0x80000000;  //   -2147483648
	const variant a(-6);
	const variant c(12);
	variant b(min_32_bit_integer);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			b = a ^ c;
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
	ASSERT_LOG(excepted, "test expectation failed; `-6`. hosted in a 32 bit signed integer, raised to the power of `12`, should raise an exception, in order to prevent an arithmetic overflow");
}

UNIT_TEST(variant_from_null_char_array) {
	char * null_plain_old_char_array = nullptr;
	const variant variant_from_null_plain_old_char_array(
			null_plain_old_char_array);
	check::type_is_null(variant_from_null_plain_old_char_array);
	//   Logging the null char array is unsafe. However, it's safe to log
	// the variant created out of it.
	LOG_INFO(variant_from_null_plain_old_char_array);
}

UNIT_TEST(create_translated_string) {
	const std::string string = "Hello, world!";
	LOG_DEBUG(string);
	const std::string expected_translated_string = string;
	LOG_DEBUG(expected_translated_string);
	const variant actual_translated_string_variant =
			variant::create_translated_string(string);
	LOG_DEBUG(actual_translated_string_variant);
	check::type_is_string(actual_translated_string_variant);
	const std::string actual_translated_string =
			actual_translated_string_variant.as_string();
	LOG_DEBUG(actual_translated_string);
	CHECK_EQ(expected_translated_string, actual_translated_string);
}

UNIT_TEST(unable_to_create_variant_out_of_dictionary_with_boolean_keys_FAILS) {
	std::map<variant, variant> dictionary_with_boolean_keys;
	const variant key = variant::from_bool(true);
	const std::string pi_as_string = boost::lexical_cast<std::string>(M_PI);
	const variant value(decimal::from_string(pi_as_string));
	std::pair<std::map<variant, variant>::iterator, bool> ret;
	ret = dictionary_with_boolean_keys.insert(
			std::pair<variant, variant>(key, value));
	CHECK_EQ(true, ret.second);
	//   Can not check that an exception gets raised.
	const variant actual_test_output = variant(& dictionary_with_boolean_keys);
	check::type_is_dictionary(actual_test_output);
}

//   operator[] on object variant returns self when requesting the zero index.
UNIT_TEST(operator_square_brackets_on_object_returns_self_a) {
	const std::string code = "; null";
	const variant code_variant(code);
	const game_logic::Formula code_variant_formula(code_variant);
	const variant execution_output = code_variant_formula.execute();
	check::type_is_object(execution_output);
	const variant & indexed = execution_output[0];
	LOG_DEBUG(execution_output);
	LOG_DEBUG(indexed);
	CHECK_EQ(execution_output, indexed);
	LOG_DEBUG(& execution_output);
	LOG_DEBUG(& indexed);
	CHECK_EQ(& execution_output, & indexed);
}

//   operator[] on object variant returns self when requesting the zero index
// wrapped in a variant.
UNIT_TEST(operator_square_brackets_on_object_returns_self_b) {
	const std::string code = "; null";
	const variant code_variant(code);
	const game_logic::Formula code_variant_formula(code_variant);
	const variant execution_output = code_variant_formula.execute();
	check::type_is_object(execution_output);
	const variant index_zero(0);
	const variant & indexed = execution_output[index_zero];
	LOG_DEBUG(execution_output);
	LOG_DEBUG(indexed);
	CHECK_EQ(execution_output, indexed);
	LOG_DEBUG(& execution_output);
	LOG_DEBUG(& indexed);
	CHECK_EQ(& execution_output, & indexed);
}

UNIT_TEST(list_indexing_out_of_bounds_excepts) {
	const variant zero(0);
	const variant a("a");
	const variant true_variant = variant::from_bool(true);
	std::vector<variant> list;
	std::vector<variant>::iterator it = list.begin();
	it = list.insert(it, true_variant);
	it = list.insert(it, a);
	list.insert(it, zero);
	const variant list_variant(& list);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			list_variant[3];
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
}

//   Refuse to index (using variant) any variant not of type object,
// dictionary, or list.
UNIT_TEST(refuse_to_index_a_decimal_variant) {
	const variant decimal_variant(decimal::from_string("0.0"));
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			decimal_variant[decimal_variant];
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
}

UNIT_TEST(no_keys_in_non_dictionary_variants) {
	const variant true_variant = variant::from_bool(true);
	std::vector<variant> list;
	list.push_back(true_variant);
	const variant list_variant(& list);
	const variant target_key("key");
	const bool has_the_key = list_variant.has_key(target_key);
	CHECK_EQ(has_the_key, false);
}

UNIT_TEST(no_elements_in_a_null_variant) {
	const variant null_variant;
	const uint_fast8_t expected = 0;
	const uint_fast8_t actual = null_variant.num_elements();
	CHECK_EQ(expected, actual);
}

UNIT_TEST(object_variant_has_a_single_element) {
	const std::string code = "; null";
	const variant code_variant(code);
	const game_logic::Formula code_variant_formula(code_variant);
	const variant object_variant = code_variant_formula.execute();
	check::type_is_object(object_variant);
	const uint_fast8_t expected = 1;
	const uint_fast8_t actual = object_variant.num_elements();
	CHECK_EQ(expected, actual);
}

UNIT_TEST(string_variant_num_elements_is_string_length) {
	const std::string string = "; null";
	const uint_fast8_t string_length = string.length();
	const variant string_variant(string);
	check::type_is_string(string_variant);
	const uint_fast8_t expected = 6;
	const uint_fast8_t actual = string_variant.num_elements();
	CHECK_EQ(expected, actual);
}

//   Refuse to give the number of elements in any variant not of type null,
// object, list, string, or dictionary.
UNIT_TEST(refuse_to_return_num_elements_of_decimal_variant) {
	const variant decimal_variant(decimal::from_string("0.0"));
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			decimal_variant.num_elements();
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
}

UNIT_TEST(is_str_utf8_it_is) {
	const std::string string = "\u00f1";  //   n with tilde.
	LOG_DEBUG(string);
	const variant string_variant(string);
	const bool it_is = string_variant.is_str_utf8();
	CHECK_EQ(true, it_is);
}

UNIT_TEST(is_str_utf8_it_is_not) {
	const std::string string = "nn";
	const variant string_variant(string);
	const bool it_is_not = string_variant.is_str_utf8();
	CHECK_EQ(false, it_is_not);
}

UNIT_TEST(can_request_an_empty_slice_to_any_variant_type) {
	const variant null_variant;
	const variant slice = null_variant.get_list_slice(-2147483648, -2147483648);
	check::type_is_list(slice);
	const std::vector<variant> slice_as_list = slice.as_list();
	const uint_fast8_t slice_as_list_size = slice_as_list.size();
	CHECK_EQ(0, slice_as_list_size);
}

UNIT_TEST(refuse_to_slice_lists_for_illegal_indexes) {
	std::vector<variant> list;
	const variant true_variant = variant::from_bool(true);
	list.push_back(true_variant);
	const variant list_variant(& list);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			list_variant.get_list_slice(-2147483648, 2147483647);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
}

// XXX    This feels a little bit weird. Maybe it exposes a bug, or an
// XXX  incomplete implementation. But it also could be like this on purpose
// XXX  (i.e. a function call for a generic function depends on whether the
// XXX  argument complies the expectation of the type erasure, or not).
UNIT_TEST(function_call_valid_for_generic_function_is_invalid_wtf) {
	const std::string code = "def << T >> (T t) -> T t";
	const variant code_variant(code);
	const game_logic::Formula code_variant_formula(code_variant);
	const variant execution_result = code_variant_formula.execute();
	check::type_is_generic_function(execution_result);
	const variant generic_function = execution_result;
	const std::vector<variant> args;
	std::string message;
	message.resize(128);
	const bool call_valid = generic_function.function_call_valid(
			args, & message, false);
	CHECK_EQ(false, call_valid);
	CHECK_EQ("Not a function", message);
}

UNIT_TEST(function_call_valid_passing_excess_arguments_is_invalid) {
	const std::string code = "def (any a) -> any a";
	const variant code_variant(code);
	const game_logic::Formula code_variant_formula(code_variant);
	const variant execution_result = code_variant_formula.execute();
	check::type_is_function(execution_result);
	const variant function = execution_result;
	std::vector<variant> args;
	args.push_back(variant(0));
	args.push_back(variant("a"));
	std::string message;
	message.resize(128);
	const bool call_valid = function.function_call_valid(args, & message, false);
	CHECK_EQ(false, call_valid);
	CHECK_EQ("Incorrect number of arguments to function", message);
}

UNIT_TEST(function_call_valid_passing_argument_of_unexpected_type_is_invalid) {
	const std::string code = "def (int i) -> int i";
	const variant code_variant(code);
	const game_logic::Formula code_variant_formula(code_variant);
	const variant execution_result = code_variant_formula.execute();
	check::type_is_function(execution_result);
	const variant function = execution_result;
	std::vector<variant> args;
	args.push_back(variant(decimal::from_string("0.32993")));
	std::string message;
	message.resize(128);
	const bool call_valid = function.function_call_valid(args, & message, false);
	CHECK_EQ(false, call_valid);
	CHECK_EQ("Argument 1 does not match. Expects int but found 0.32993", message);
}

UNIT_TEST(function_call_passing_wrong_number_of_arguments_generates_error) {
	const std::string code = "def (int i, int j) -> int i + j";
	const variant code_variant(code);
	const game_logic::Formula code_variant_formula(code_variant);
	const variant execution_result = code_variant_formula.execute();
	check::type_is_function(execution_result);
	const variant function = execution_result;
	std::vector<variant> args;
	args.push_back(variant(0));
	args.push_back(variant(1));
	args.push_back(variant(decimal::from_string("0.32993")));
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			function(& args);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
}

UNIT_TEST(bool_variant_as_bool_with_default) {
	const variant bool_variant(variant::from_bool(false));
	const bool bool_variant_as_bool = bool_variant.as_bool(false);
	CHECK_EQ(false, bool_variant_as_bool);
}

UNIT_TEST(int_variant_as_bool_with_default) {
	const variant int_variant(2);
	const bool int_variant_as_bool = int_variant.as_bool(false);
	CHECK_EQ(true, int_variant_as_bool);
}

UNIT_TEST(null_variant_as_bool_without_default) {
	const variant null_variant;
	const bool null_variant_as_bool = null_variant.as_bool();
	CHECK_EQ(false, null_variant_as_bool);
}

UNIT_TEST(decimal_variant_as_bool_without_default) {
	const variant decimal_variant(decimal::from_string("0.32993"));
	const bool decimal_variant_as_bool = decimal_variant.as_bool();
	CHECK_EQ(true, decimal_variant_as_bool);
}

//   Object as bool is true unless nullptr.
UNIT_TEST(object_variant_as_bool_without_default) {
	const std::string code = "; null";
	const variant code_variant(code);
	const game_logic::Formula code_variant_formula(code_variant);
	const variant execution = code_variant_formula.execute();
	check::type_is_object(execution);
	const variant object_variant = execution;
	const bool object_variant_as_bool = object_variant.as_bool();
	CHECK_EQ(true, object_variant_as_bool);
}

//   Empty list as bool is false.
UNIT_TEST(list_as_bool_without_default) {
	std::vector<variant> empty_vector;
	const variant empty_list(& empty_vector);
	check::type_is_list(empty_list);
	const bool empty_list_as_bool = empty_list.as_bool();
	CHECK_EQ(false, empty_list_as_bool);
}

UNIT_TEST(function_as_bool_without_default) {
	const std::string code = "def () -> null null";
	const variant code_variant(code);
	const game_logic::Formula code_variant_formula(code_variant);
	const variant execution = code_variant_formula.execute();
	check::type_is_function(execution);
	const variant function_variant = execution;
	const bool function_variant_as_bool = function_variant.as_bool();
	CHECK_EQ(true, function_variant_as_bool);
}

UNIT_TEST(generic_function_as_bool_without_default_is_false_wtf_FAILS) {
	const std::string code = "def << T >> (T t) -> T t";
	const variant code_variant(code);
	const game_logic::Formula code_variant_formula(code_variant);
	const variant execution = code_variant_formula.execute();
	check::type_is_generic_function(execution);
	const variant generic_function_variant = execution;
	const bool generic_function_variant_as_bool = generic_function_variant.as_bool();
	CHECK_EQ(false, generic_function_variant_as_bool);
}

//   This one segfaults.
UNIT_TEST(empty_list_variant_as_list_ref_FAILS) {
	std::vector<variant> empty_vector;
	const variant empty_list(& empty_vector);
	check::type_is_list(empty_list);
	const std::vector<variant> vector = empty_list.as_list_ref();
	const uint_fast8_t vector_size = vector.size();
	CHECK_EQ(0, vector_size);
}

UNIT_TEST(list_variant_as_list_ref) {
	std::vector<variant> variants_vector;
	variants_vector.push_back(variant(32993));
	const variant empty_list(& variants_vector);
	check::type_is_list(empty_list);
	const std::vector<variant> vector = empty_list.as_list_ref();
	const uint_fast8_t vector_size = vector.size();
	CHECK_EQ(1, vector_size);
	const variant element = vector[0];
	check::type_is_int(element);
	const int_fast32_t element_as_int = element.as_int();
	CHECK_EQ(32993, element_as_int);
}

UNIT_TEST(null_variant_as_list_optional) {
	const variant null_variant;
	const std::vector<variant> as_list_optional = null_variant.as_list_optional();
	const uint_fast8_t as_list_optional_size = as_list_optional.size();
	CHECK_EQ(0, as_list_optional_size);
}

UNIT_TEST(list_variant_as_list_optional) {
	std::vector<variant> empty_variants_vector;
	const variant empty_list_variant(& empty_variants_vector);
	const std::vector<variant> as_list_optional = empty_list_variant.as_list_optional();
	const uint_fast8_t as_list_optional_size = as_list_optional.size();
	CHECK_EQ(0, as_list_optional_size);
}

UNIT_TEST(any_non_list_as_list_results_boxed_in_list) {
	const variant int_variant(32993);
	const std::vector<variant> int_variant_as_list = int_variant.as_list();
	const uint_fast8_t int_variant_as_list_size = int_variant_as_list.size();
	CHECK_EQ(1, int_variant_as_list_size);
	const variant int_variant_as_list_boxed_list_element = int_variant_as_list[0];
	CHECK_EQ(int_variant, int_variant_as_list_boxed_list_element);
}

UNIT_TEST(as_list_string) {
	std::vector<variant> variants_vector;

	const variant string_variant("null");
	variants_vector.push_back(string_variant);
	variants_vector.push_back(string_variant);
	variants_vector.push_back(string_variant);
	const variant list_variant(& variants_vector);

	std::vector<std::string> expectation;
	expectation.push_back("null");
	expectation.push_back("null");
	expectation.push_back("null");

	const std::vector<std::string> list_variant_as_list_string =
			list_variant.as_list_string();
	const uint_fast8_t list_variant_as_list_string_size =
			list_variant_as_list_string.size();
	CHECK_EQ(expectation.size(), list_variant_as_list_string_size);
	for (uint_fast8_t i = 0; i < list_variant_as_list_string_size; i++) {
		const std::string expected_element = expectation[i];
		LOG_DEBUG(expected_element);
		const std::string actual_element =
				list_variant_as_list_string[i];
		LOG_DEBUG(actual_element);
		CHECK_EQ(expected_element, actual_element);
	}
}

UNIT_TEST(string_variant_plus_dictionary_variant_excepts) {
	const std::string string = "null";
	const variant string_variant(string);
	std::map<variant, variant> empty_variants_map;
	const variant empty_dictionary_variant(& empty_variants_map);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			string_variant + empty_dictionary_variant;
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
}

UNIT_TEST(string_variant_plus_int_variant) {
	const std::string string = "null";
	const variant string_variant(string);
	const variant int_variant(329932);
	const variant sum = string_variant + int_variant;
	check::type_is_string(sum);
	const std::string sum_as_string = sum.as_string();
	CHECK_EQ("null329932", sum_as_string);
}

UNIT_TEST(list_variant_plus_string_variant) {
	std::vector<variant> variants_vector;
	variants_vector.push_back(variant(1));
	variants_vector.push_back(variant(2));
	const variant list_variant(& variants_vector);
	const std::string string = "null";
	const variant string_variant(string);
	const variant sum = list_variant + string_variant;
	check::type_is_string(sum);
	const std::string sum_as_string = sum.as_string();
	CHECK_EQ("[1,2]null", sum_as_string);
}

UNIT_TEST(int_variant_plus_bool_variant) {
	const variant int_variant(32993);
	const variant bool_variant(variant::from_bool(true));
	const variant sum = int_variant + bool_variant;
	check::type_is_int(sum);
	const int_fast32_t sum_as_int = sum.as_int();
	CHECK_EQ(32994, sum_as_int);
}

UNIT_TEST(bool_variant_plus_bool_variant) {
	const variant bool_variant(variant::from_bool(true));
	const variant bool_variant_(variant::from_bool(true));
	const variant sum = bool_variant + bool_variant_;
	check::type_is_int(sum);
	const int_fast32_t sum_as_int = sum.as_int();
	CHECK_EQ(2, sum_as_int);
}

UNIT_TEST(null_plus_anything_returns_anything) {
	const variant null_variant;
	const variant int_variant(32993);
	const variant sum = null_variant + int_variant;
	check::type_is_int(sum);
	const int_fast32_t sum_as_int = sum.as_int();
	CHECK_EQ(32993, sum_as_int);
}

UNIT_TEST(anything_plus_null_returns_anything) {
	std::vector<variant> variants_vector;
	const variant list_variant(& variants_vector);
	const variant null_variant;
	const variant sum = list_variant + null_variant;
	check::type_is_list(sum);
	const std::vector<variant> sum_as_list = sum.as_list();
	CHECK_EQ(0, sum_as_list.size());
}

UNIT_TEST(empty_list_plus_empty_list_returns_empty_list) {
	std::vector<variant> variants_vector;
	const variant list_variant(& variants_vector);
	std::vector<variant> variants_vector_;
	const variant list_variant_(& variants_vector_);
	const variant sum = list_variant + list_variant_;
	check::type_is_list(sum);
	const std::vector<variant> sum_as_list = sum.as_list();
	CHECK_EQ(0, sum_as_list.size());
}

/*   Disabled this unit test because it seems like `M_PI` is somehow set
 * to `3????` @ImUseless#2172 's AMD A8-9600 RADEON R7, 10 COMPUTE CORES 4C+6G,
 * instead of the normal 3.1415... so at her/his end `value_as_decimal` ends
 * up set to `3.0` instead of the usual `3.141592`, and fails to assert the
 * `CHECK_EQ` check:
 *
 *             const decimal value_as_decimal = value.as_decimal();
 *             const decimal expected = decimal::from_string("3.141592");
 * -->         CHECK_EQ(expected, value_as_decimal);                        <-- here
 *     } else {
 *             ASSERT_LOG(key_type == variant::TYPE::VARIANT_TYPE_INT, "unexpected map key/s");
 *             check::type_is_bool(value);
 */
/*
UNIT_TEST(dictionary_plus_dictionary_returns_dictionary) {
	std::map<variant, variant> variants_map;
	std::map<variant, variant> variants_map_;
	const variant key("TT");
	const variant key_(32993);
	const std::string pi_as_string = boost::lexical_cast<std::string>(M_PI);
	const variant value(decimal::from_string(pi_as_string));
	const variant value_(variant::from_bool(true));
	std::pair<std::map<variant, variant>::iterator, bool> ret;
	ret = variants_map.insert(std::pair<variant, variant>(key, value));
	CHECK_EQ(true, ret.second);
	ret = variants_map_.insert(std::pair<variant, variant>(key_, value_));
	CHECK_EQ(true, ret.second);
	const variant dictionary_variant(& variants_map);
	const variant dictionary_variant_(& variants_map_);
	const variant sum = dictionary_variant + dictionary_variant_;
	check::type_is_dictionary(sum);
	const std::map<variant, variant> sum_as_map = sum.as_map();
	CHECK_EQ(2, sum_as_map.size());
	for (auto const & entry : sum_as_map) {
		const variant key = entry.first;
		const variant value = entry.second;
		const variant::TYPE key_type = key.type();
		if (key_type == variant::TYPE::VARIANT_TYPE_STRING) {
			check::type_is_decimal(value);
			const decimal value_as_decimal = value.as_decimal();
			const decimal expected = decimal::from_string("3.141592");
			CHECK_EQ(expected, value_as_decimal);
		} else {
			ASSERT_LOG(key_type == variant::TYPE::VARIANT_TYPE_INT, "unexpected map key/s");
			check::type_is_bool(value);
			const bool value_as_bool = value.as_bool();
			CHECK_EQ(true, value_as_bool);
		}
	}
}
*/

UNIT_TEST(empty_list_times_negative_int) {
	std::vector<variant> empty_variants_vector;
	const variant empty_list_variant(& empty_variants_vector);
	const variant int_variant(-32993);
	const variant product = empty_list_variant * int_variant;
	LOG_DEBUG(product);
}

UNIT_TEST(divide_by_zero_decimal_excepts) {
	const variant null_variant;
	const variant decimal_zero_variant(decimal::from_string("0.0"));
	bool excepted = false;
	{
        const assert_recover_scope unit_test_exception_expected;
		try {
			null_variant / decimal_zero_variant;
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
}

UNIT_TEST(decimal_variants_division_standard_case) {
	const variant decimal_variant_n(decimal::from_string("6.0"));
	const variant decimal_variant_d(decimal::from_string("3.0"));
	const variant division = decimal_variant_n / decimal_variant_d;
	check::type_is_decimal(division);
	const decimal division_as_decimal = division.as_decimal();
	const decimal expected(decimal::from_string("2.0"));
	CHECK_EQ(expected, division_as_decimal);
}

UNIT_TEST(divide_by_zero_integer_excepts) {
	const variant null_variant;
	const variant integer_zero_variant(0);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			null_variant / integer_zero_variant;
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
}

UNIT_TEST(remainder_of_divide_by_zero_integer_excepts) {
	const variant null_variant;
	const variant integer_zero_variant(0);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			null_variant % integer_zero_variant;
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	CHECK_EQ(true, excepted);
}

UNIT_TEST(generic_function_variants_equality) {
	const std::string code = "def << T >> (T t) -> T t";
	const variant code_variant(code);
	const game_logic::Formula code_variant_formula(code_variant);
	const variant execution = code_variant_formula.execute();
	check::type_is_generic_function(execution);
	const variant generic_function = execution;
	const std::string code_ = "def << T >> (T t) -> T t";
	const variant code_variant_(code_);
	const game_logic::Formula code_variant_formula_(code_variant_);
	const variant execution_ = code_variant_formula_.execute();
	check::type_is_generic_function(execution_);
	const variant generic_function_ = execution_;
	const bool equal_different_objects = generic_function == generic_function_;
	CHECK_EQ(false, equal_different_objects);
	const bool equal_same_object = generic_function == generic_function;
	CHECK_EQ(true, equal_same_object);
}

UNIT_TEST(serialize_null_variant_to_string) {
	const variant null_variant;
	std::string serialization = "";
	null_variant.serializeToString(serialization);
	CHECK_EQ("null", serialization);
}

UNIT_TEST(serialize_bool_variant_to_string) {
	const variant bool_variant(variant::from_bool(true));
	std::string serialization = "";
	bool_variant.serializeToString(serialization);
	CHECK_EQ("true", serialization);
}

UNIT_TEST(serialize_enum_variant_to_string) {
	const std::string code = "enum a";
	const variant code_variant(code);
	const game_logic::Formula code_variant_formula(code_variant);
	const variant execution = code_variant_formula.execute();
	check::type_is_enum(execution);
	const variant enum_variant = execution;
	std::string serialization = "";
	enum_variant.serializeToString(serialization);
	CHECK_EQ(code, serialization);
}

//   Dictionary variants can have integers as keys. I don't know if the Boost
// property tree can be configured to be compatible with relaxed JSON key
// types.
UNIT_TEST(serialize_dictionary_variant_to_string) {
	std::map<variant, variant> variants_map;
	const variant key("TT");
// 	const variant key_(32993);
	const variant key_("32993");
	const std::string pi_as_string = "3.141592";
	const variant value(decimal::from_string(pi_as_string));
	const variant value_(variant::from_bool(true));
	std::pair<std::map<variant, variant>::iterator, bool> ret;
	ret = variants_map.insert(std::pair<variant, variant>(key, value));
	CHECK_EQ(true, ret.second);
	ret = variants_map.insert(std::pair<variant, variant>(key_, value_));
	CHECK_EQ(true, ret.second);
	const variant dictionary_variant(& variants_map);
	std::string serialization = "";
	dictionary_variant.serializeToString(serialization);
	LOG_DEBUG(serialization);
// 	serialization = "{\"TT\":3.141592,\"32993\":true}";
// 	LOG_DEBUG(serialization);
	ASSERT_LOG(serialization.find("'") != std::string::npos, "unexpected serialization results");
	std::replace(serialization.begin(), serialization.end(), '\'', '"');
	LOG_DEBUG(serialization);
	boost::property_tree::ptree property_tree;
	std::istringstream iss(serialization);
	boost::property_tree::read_json(iss, property_tree);
	const std::string tt = property_tree.get<std::string>("TT");
	CHECK_EQ("3.141592", tt);
// 	const std::string number = property_tree.get<std::string>(32993);
	const std::string number = property_tree.get<std::string>("32993");
	CHECK_EQ("true", number);
}

UNIT_TEST(serialize_string_variant_to_string_a) {
	const std::string string = "~32993";
	const variant string_variant(string);
	std::string serialization = "";
	string_variant.serializeToString(serialization);
	LOG_DEBUG(serialization);
	CHECK_EQ("'~32993'", serialization);
}

UNIT_TEST(serialize_string_variant_to_string_b) {
	const std::string string = "~32993~";
	const variant string_variant(string);
	std::string serialization = "";
	string_variant.serializeToString(serialization);
	LOG_DEBUG(serialization);
	CHECK_EQ("~32993~", serialization);
}

UNIT_TEST(serialize_string_variant_to_string_c) {
	const std::string string = "-='=-";
	const variant string_variant(string);
	std::string serialization = "";
	string_variant.serializeToString(serialization);
	LOG_DEBUG(serialization);
	CHECK_EQ("q(-='=-)", serialization);
}

//   This exposes a possible bug.
UNIT_TEST(serialize_string_variant_to_string_d) {
	const std::string string = "-)=)'(=(-";
	const variant string_variant(string);
	std::string serialization = "";
	string_variant.serializeToString(serialization);
	LOG_DEBUG(serialization);
	CHECK_EQ("q(-)=)'(=(-)", serialization);
}

UNIT_TEST(deserialize_bool_variant_from_bool_in_string) {
	const std::string string = "true";
	variant variant_;
	variant_.serialize_from_string(string);
	check::type_is_bool(variant_);
	const variant from_deserializing_bool_in_string = variant_;
	const bool deserialized = from_deserializing_bool_in_string.as_bool();
	CHECK_EQ(true, deserialized);
}

UNIT_TEST(deserialize_string_variant_from_crashy_formula_in_string) {
	const std::string string = "(-2147483648) ^ ";
	variant variant_;
	{
		const assert_recover_scope expecting_exception;
		variant_.serialize_from_string(string);
	}
	check::type_is_string(variant_);
	const variant from_crashy_formula_deserialization = variant_;
	const std::string from_crashy_formula_deserialization_as_string =
			from_crashy_formula_deserialization.as_string();
	LOG_DEBUG(from_crashy_formula_deserialization);
	LOG_DEBUG(from_crashy_formula_deserialization_as_string);
	CHECK_EQ(string, from_crashy_formula_deserialization_as_string);
}

UNIT_TEST(list_variant_refcount_0) {
	std::vector<variant> variants_vector;
	variants_vector.push_back(variant("tdfgdsfgdfsdfgdsfgdsf33gg"));
	variants_vector.push_back(variant(2534564567452436457586222.0));
	const variant list_variant(& variants_vector);
	const uint_fast8_t refs_count = list_variant.refcount();
	CHECK_EQ(1, refs_count);
}

UNIT_TEST(list_variant_refcount_1) {
	std::vector<variant> variants_vector;
	variants_vector.push_back(variant("3454536-----'sdfasdfasdf3"));
	const variant list_variant(& variants_vector);
	const uint_fast8_t refs_count = list_variant.refcount();
	CHECK_EQ(1, refs_count);
	const variant list_variant_(list_variant);
	const uint_fast8_t refs_count_ = list_variant_.refcount();
	CHECK_EQ(2, refs_count_);
}

UNIT_TEST(null_variant_string_cast) {
	const variant variant_;
	check::type_is_null(variant_);
	const variant null_variant = variant_;
	const std::string null_variant_string_cast = null_variant.string_cast();
	CHECK_EQ("null", null_variant_string_cast);
}

UNIT_TEST(bool_variant_string_cast) {
	const variant variant_ = variant::from_bool(true);
	check::type_is_bool(variant_);
	const variant bool_variant = variant_;
	const std::string bool_variant_string_cast = bool_variant.string_cast();
	CHECK_EQ("true", bool_variant_string_cast);
}

UNIT_TEST(enum_variant_string_cast) {
	const std::string code = "enum a";
	const variant code_variant(code);
	const game_logic::Formula code_variant_formula(code_variant);
	const variant execution = code_variant_formula.execute();
	check::type_is_enum(execution);
	const variant enum_variant = execution;
	const std::string enum_variant_string_cast = enum_variant.string_cast();
	CHECK_EQ("enum a", enum_variant_string_cast);
}

UNIT_TEST(dictionary_variant_string_cast) {
	std::map<variant, variant> variants_map;
	const variant key("TT");
	const variant key_("32993");
	const std::string pi_as_string = "3.141592";
	const variant value(decimal::from_string(pi_as_string));
	const variant value_(variant::from_bool(true));
	std::pair<std::map<variant, variant>::iterator, bool> ret;
	ret = variants_map.insert(std::pair<variant, variant>(key, value));
	CHECK_EQ(true, ret.second);
	ret = variants_map.insert(std::pair<variant, variant>(key_, value_));
	CHECK_EQ(true, ret.second);
	const variant variant_(& variants_map);
	check::type_is_dictionary(variant_);
	const variant dictionary_variant = variant_;
	const std::string dictionary_variant_string_cast =
			dictionary_variant.string_cast();
	LOG_DEBUG(dictionary_variant_string_cast);
	if ("32993: true,TT: 3.141592" != dictionary_variant_string_cast &&
			"TT: 3.141592,32993: true" != dictionary_variant_string_cast) {
		CHECK_EQ(false, true);
	}
}
