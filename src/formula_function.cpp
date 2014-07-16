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
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <boost/lexical_cast.hpp>
#include <boost/uuid/sha1.hpp>
#include <boost/algorithm/string.hpp>
#include <iomanip>
#include <iostream>
#include <iomanip>
#include <stack>
#include <cmath>
#if defined(_MSC_VER)
#include <boost/math/special_functions/round.hpp>
#define bmround	boost::math::round
#else
#define bmround	round
#endif

#include "kre/Geometry.hpp"

#include "array_callable.hpp"
#include "asserts.hpp"
#include "base64.hpp"
#include "code_editor_dialog.hpp"
#include "compress.hpp"
#include "custom_object.hpp"
#include "dialog.hpp"
#include "debug_console.hpp"
#include "draw_primitive.hpp"
#include "formatter.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "formula_callable_utils.hpp"
#include "formula_function.hpp"
#include "formula_function_registry.hpp"
#include "formula_object.hpp"
#include "hex_map.hpp"
#include "hex_object.hpp"
#include "lua_iface.hpp"
#include "md5.hpp"
#include "module.hpp"
#include "rectangle_rotator.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"
#include "variant_callable.hpp"
#include "controls.hpp"
#include "pathfinding.hpp"
#include "preferences.hpp"
#include "random.hpp"
#include "level.hpp"
#include "json_parser.hpp"
#include "uuid.hpp"
#include "variant_utils.hpp"
#include "voxel_model.hpp"

#include <boost/regex.hpp>
#if defined(_WINDOWS)
#include <boost/math/special_functions/asinh.hpp>
#include <boost/math/special_functions/acosh.hpp>
#include <boost/math/special_functions/atanh.hpp>
#define asinh boost::math::asinh
#define acosh boost::math::acosh
#define atanh boost::math::atanh
#endif

extern variant g_auto_update_info;

namespace 
{
	const std::string FunctionModule = "core";

	const float radians_to_degrees = 57.29577951308232087f;
	const std::string EmptyStr;

	using namespace game_logic;
	std::string read_identifier_expression(const FormulaExpression& expr) {
		variant literal;
		expr.isLiteral(literal);
		if(literal.is_string()) {
			return literal.as_string();
		} else {
			std::string result;
			if(expr.isIdentifier(&result)) {
				return result;
			}

			ASSERT_LOG(false, "Expected identifier, found " << expr.str() << expr.debugPinpointLocation());
			return "";
		}
	}
}

namespace game_logic 
{
FormulaExpression::FormulaExpression(const char* name) : name_(name), begin_str_(EmptyStr.begin()), end_str_(EmptyStr.end()), ntimes_called_(0)
{}

std::vector<ConstExpressionPtr> FormulaExpression::queryChildren() const {
	std::vector<ConstExpressionPtr> result = getChildren();
	result.erase(std::remove(result.begin(), result.end(), ConstExpressionPtr()), result.end());
	return result;
}

std::vector<ConstExpressionPtr> FormulaExpression::queryChildrenRecursive() const {
	std::vector<ConstExpressionPtr> result;
	result.push_back(ConstExpressionPtr(this));
	for(ConstExpressionPtr child : queryChildren()) {
		if(child.get() != this) {
			std::vector<ConstExpressionPtr> items = child->queryChildrenRecursive();
			result.insert(result.end(), items.begin(), items.end());
		}
	}

	return result;
}

void FormulaExpression::copyDebugInfoFrom(const FormulaExpression& o)
{
	setDebugInfo(o.parent_formula_, o.begin_str_, o.end_str_);
}

void FormulaExpression::setDebugInfo(const variant& parent_formula,
                                        std::string::const_iterator begin_str,
                                        std::string::const_iterator end_str)
{
	parent_formula_ = parent_formula;
	begin_str_ = begin_str;
	end_str_ = end_str;
	str_ = std::string(begin_str, end_str);
}

bool FormulaExpression::hasDebugInfo() const
{
	return parent_formula_.is_string() && parent_formula_.get_debug_info();
}

ConstFormulaCallableDefinitionPtr FormulaExpression::getTypeDefinition() const
{
	variant_type_ptr type = queryVariantType();
	if(type) {
		return ConstFormulaCallableDefinitionPtr(type->getDefinition());
	}

	return NULL;
}

std::string pinpoint_location(variant v, std::string::const_iterator begin)
{
	return pinpoint_location(v, begin, begin);
}

std::string pinpoint_location(variant v, std::string::const_iterator begin,
                                         std::string::const_iterator end,
						                 PinpointedLoc* pos_info)
{
	std::string str(begin, end);
	if(!v.is_string() || !v.get_debug_info()) {
		return "Unknown location (" + str + ")\n";
	}

	int line_num = v.get_debug_info()->line;
	int begin_line_base = v.get_debug_info()->column;

	std::string::const_iterator begin_line = v.as_string().begin();
	while(std::find(begin_line, begin, '\n') != begin) {
		begin_line_base = 0;
		begin_line = std::find(begin_line, begin, '\n')+1;
		++line_num;
	}

	//this is the real start of the line. begin_line will advance
	//to the first non-whitespace character.
	std::string::const_iterator real_start_of_line = begin_line;

	while(begin_line != begin && util::c_isspace(*begin_line)) {
		++begin_line;
	}

	std::string::const_iterator end_line = std::find(begin_line, v.as_string().end(), '\n');

	std::string line(begin_line, end_line);
	int pos = begin - begin_line;

	if(pos_info) {
		const int col = (begin - real_start_of_line) + begin_line_base;
		pos_info->begin_line = line_num;
		pos_info->begin_col = col+1;

		int end_line = line_num;
		int end_col = col+1;
		for(std::string::const_iterator itor = begin; itor != end; ++itor) {
			if(*itor == '\n') {
				end_col = 1;
				end_line++;
			} else {
				end_col++;
			}
		}

		pos_info->end_line = end_line;
		pos_info->end_col = end_col;
	}

	if(pos > 40) {
		line.erase(line.begin(), line.begin() + pos - 40);
		pos = 40;
		std::fill(line.begin(), line.begin() + 3, '.');
	}

	if(line.size() > 78) {
		line.resize(78);
		std::fill(line.end() - 3, line.end(), '.');
	}

	std::ostringstream s;
	s << "At " << *v.get_debug_info()->filename << " " << line_num << ":\n";
	s << line << "\n";
	for(int n = 0; n != pos; ++n) {
		s << " ";
	}
	s << "^";

	if(end > begin && static_cast<unsigned>(pos + (end - begin)) < line.size()) {
		for(int n = 0; n < (end - begin)-1; ++n) {
			s << "-";
		}
		s << "^";
	}

	s << "\n";


	return s.str();
}

std::string FormulaExpression::debugPinpointLocation(PinpointedLoc* loc) const
{
	if(!hasDebugInfo()) {
		return "Unknown Location (" + str_ + ")\n";
	}

	return pinpoint_location(parent_formula_, begin_str_, end_str_, loc);
}

std::pair<int,int> FormulaExpression::debugLocInFile() const
{
	if(!hasDebugInfo()) {
		return std::pair<int,int>(-1,-1);
	}
	return std::pair<int,int>(begin_str_ - parent_formula_.as_string().begin(),
	                          end_str_ - parent_formula_.as_string().begin());
}

variant FormulaExpression::executeMember(const FormulaCallable& variables, std::string& id, variant* variant_id) const
{
	Formula::failIfStaticContext();
	ASSERT_LOG(false, "Trying to set illegal value: " << str_ << "\n" << debugPinpointLocation());
	return variant();
}

namespace {

variant split_variant_if_str(const variant& s)
{
	if(!s.is_string()) {
		return s;
	}

	std::vector<std::string> v = util::split(s.as_string(), "");
	std::vector<variant> res;
	res.reserve(v.size());
	for(const std::string& str : v) {
		res.push_back(variant(str));
	}

	return variant(&res);
}

class ffl_cache : public FormulaCallable
{
public:
	explicit ffl_cache(int max_entries) : max_entries_(max_entries)
	{}
	const variant* get(const variant& key) const {
		std::map<variant, variant>::const_iterator i = cache_.find(key);
		if(i != cache_.end()) {
			return &i->second;
		} else {
			return NULL;
		}
	}

	void store(const variant& key, const variant& value) const {
		if(cache_.size() == max_entries_) {
			cache_.clear();
		}

		cache_[key] = value;
	}
private:
	variant getValue(const std::string& key) const {
		return variant();
	}

	mutable std::map<variant, variant> cache_;
	int max_entries_;
};

FUNCTION_DEF(overload, 1, -1, "overload(fn...): makes an overload of functions")
	std::vector<variant> functions;
	for(ExpressionPtr expression : args()) {
		functions.push_back(expression->evaluate(variables));
		ASSERT_LOG(functions.back().is_function(), "CALL TO overload() WITH NON-FUNCTION VALUE " << functions.back().write_json());
	}

	return variant::create_function_overload(functions);

FUNCTION_TYPE_DEF
	int min_args = -1;
	std::vector<std::vector<variant_type_ptr> > arg_types;
	std::vector<variant_type_ptr> return_types;
	std::vector<variant_type_ptr> function_types;
	for(int n = 0; n != args().size(); ++n) {
		variant_type_ptr t = args()[n]->queryVariantType();
		function_types.push_back(t);
		std::vector<variant_type_ptr> a;
		variant_type_ptr return_type;
		int nargs = -1;
		if(t->is_function(&a, &return_type, &nargs) == false) {
			ASSERT_LOG(false, "CALL to overload() with non-function type: " << args()[n]->debugPinpointLocation());
		}

		return_types.push_back(return_type);
		if(min_args == -1 || nargs < min_args) {
			min_args = nargs;
		}

		for(unsigned m = 0; m != a.size(); ++m) {
			if(arg_types.size() <= m) {
				arg_types.resize(m+1);
			}

			arg_types[m].push_back(a[m]);
		}
	}

	if(min_args < 0) {
		min_args = 0;
	}

	variant_type_ptr return_union = variant_type::get_union(return_types);
	std::vector<variant_type_ptr> arg_union;
	for(int n = 0; n != arg_types.size(); ++n) {
		arg_union.push_back(variant_type::get_union(arg_types[n]));
	}

	return variant_type::get_function_overload_type(variant_type::get_function_type(arg_union, return_union, min_args), function_types);
END_FUNCTION_DEF(overload)

FUNCTION_DEF(addr, 1, 1, "addr(obj): Provides the address of the given object as a string. Useful for distinguishing objects")
	
	variant v = args()[0]->evaluate(variables);
	FormulaCallable* addr = NULL;
	if(!v.is_null()) {
		addr = v.convert_to<FormulaCallable>();
	}
	char buf[128];
	sprintf(buf, "%p", addr);
	return variant(std::string(buf));

FUNCTION_ARGS_DEF
	ARG_TYPE("object|null");
	RETURN_TYPE("string");
END_FUNCTION_DEF(addr)

FUNCTION_DEF(create_cache, 0, 1, "create_cache(max_entries=4096): makes an FFL cache object")
	Formula::failIfStaticContext();
	int max_entries = 4096;
	if(args().size() >= 1) {
		max_entries = args()[0]->evaluate(variables).as_int();
	}
	return variant(new ffl_cache(max_entries));
FUNCTION_ARGS_DEF
	ARG_TYPE("int");
	RETURN_TYPE("object");
END_FUNCTION_DEF(create_cache)

FUNCTION_DEF(global_cache, 0, 1, "create_cache(max_entries=4096): makes an FFL cache object")
	int max_entries = 4096;
	if(args().size() >= 1) {
		max_entries = args()[0]->evaluate(variables).as_int();
	}
	return variant(new ffl_cache(max_entries));
FUNCTION_ARGS_DEF
	ARG_TYPE("int");
	RETURN_TYPE("object");
END_FUNCTION_DEF(global_cache)

FUNCTION_DEF(query_cache, 3, 3, "query_cache(ffl_cache, key, expr): ")
	const variant key = args()[1]->evaluate(variables);

	const ffl_cache* cache = args()[0]->evaluate(variables).try_convert<ffl_cache>();
	ASSERT_LOG(cache != NULL, "ILLEGAL CACHE ARGUMENT TO query_cache");
	
	const variant* result = cache->get(key);
	if(result != NULL) {
		return *result;
	}

	const variant value = args()[2]->evaluate(variables);
	cache->store(key, value);
	return value;

FUNCTION_TYPE_DEF
	return args()[2]->queryVariantType();
END_FUNCTION_DEF(query_cache)

FUNCTION_DEF(md5, 1, 1, "md5(string) ->string")
	return variant(md5::sum(args()[0]->evaluate(variables).as_string()));
FUNCTION_ARGS_DEF
	ARG_TYPE("string");
	RETURN_TYPE("string");
END_FUNCTION_DEF(md5)

	class if_function : public FunctionExpression {
	public:
		explicit if_function(const args_list& args)
			: FunctionExpression("if", args, 2, -1)
		{}

		ExpressionPtr optimize() const {
			variant v;
			if(args().size() <= 3 && args()[0]->canReduceToVariant(v)) {
				if(v.as_bool()) {
					return args()[1];
				} else {
					if(args().size() == 3) {
						return args()[2];
					} else {
						return ExpressionPtr(new VariantExpression(variant()));
					}
				}
			}

			return ExpressionPtr();
		}

	private:
		variant execute(const FormulaCallable& variables) const {
			const int nargs = args().size();
			for(int n = 0; n < nargs-1; n += 2) {
				const bool result = args()[n]->evaluate(variables).as_bool();
				if(result) {
					return args()[n+1]->evaluate(variables);
				}
			}

			if(nargs%2 == 0) {
				return variant();
			}

			return args()[nargs-1]->evaluate(variables);
		}


		variant_type_ptr getVariantType() const {
			std::vector<variant_type_ptr> types;
			types.push_back(args()[1]->queryVariantType());
			const int nargs = args().size();
			for(int n = 1; n < nargs; n += 2) {
				types.push_back(args()[n]->queryVariantType());
			}

			if(nargs%2 == 1) {
				types.push_back(args()[nargs-1]->queryVariantType());
			} else {
				types.push_back(variant_type::get_type(variant::VARIANT_TYPE_NULL));
			}

			return variant_type::get_union(types);
		}
	};

class bound_command : public game_logic::CommandCallable
{
public:
	bound_command(variant target, const std::vector<variant>& args)
	  : target_(target), args_(args)
	{}
	virtual void execute(game_logic::FormulaCallable& ob) const {
		ob.executeCommand(target_(args_));
	}
private:
	variant target_;
	std::vector<variant> args_;
};

FUNCTION_DEF(bind, 1, -1, "bind(fn, args...)")
	variant fn = args()[0]->evaluate(variables);

	std::vector<variant> arg_values;
	for(int n = 1; n != args().size(); ++n) {
		arg_values.push_back(args()[n]->evaluate(variables));
	}

	return fn.bind_args(arg_values);
FUNCTION_TYPE_DEF
	variant_type_ptr type = args()[0]->queryVariantType();

	std::vector<variant_type_ptr> fn_args;
	variant_type_ptr return_type;
	int min_args = 0;

	if(type->is_function(&fn_args, &return_type, &min_args)) {
		const int nargs = args().size()-1;
		min_args = std::max<int>(0, min_args - nargs);
		if(fn_args.size() <= static_cast<unsigned>(nargs)) {
			fn_args.erase(fn_args.begin(), fn_args.begin() + nargs);
		} else {
			ASSERT_LOG(false, "bind called with too many arguments");
		}

		return variant_type::get_function_type(fn_args, return_type, min_args);
	} else {
		return variant_type::get_type(variant::VARIANT_TYPE_FUNCTION);
	}

FUNCTION_ARGS_DEF
	ARG_TYPE("function");
END_FUNCTION_DEF(bind)

FUNCTION_DEF(bind_command, 1, -1, "bind_command(fn, args..)")
	variant fn = args()[0]->evaluate(variables);
	if(fn.type() != variant::VARIANT_TYPE_MULTI_FUNCTION) {
		fn.must_be(variant::VARIANT_TYPE_FUNCTION);
	}
	std::vector<variant> args_list;
	for(int n = 1; n != args().size(); ++n) {
		args_list.push_back(args()[n]->evaluate(variables));
	}

	std::string message;
	ASSERT_LOG(fn.function_call_valid(args_list, &message), "Error in bind_command: functions args do not match: " << message);
	
	return variant(new bound_command(fn, args_list));

FUNCTION_ARGS_DEF
	ARG_TYPE("function");
FUNCTION_TYPE_DEF
	return variant_type::get_commands();
END_FUNCTION_DEF(bind_command)

FUNCTION_DEF(bind_closure, 2, 2, "bind_closure(fn, obj): binds the given lambda fn to the given object closure")
	variant fn = args()[0]->evaluate(variables);
	return fn.bind_closure(args()[1]->evaluate(variables).as_callable());

FUNCTION_ARGS_DEF
	ARG_TYPE("function");
END_FUNCTION_DEF(bind_closure)

FUNCTION_DEF(singleton, 1, 1, "singleton(string typename): create a singleton object with the given typename")
	variant type = args()[0]->evaluate(variables);

	static std::map<variant, boost::intrusive_ptr<formula_object> > cache;
	if(cache.count(type)) {
		return variant(cache[type].get());
	}

	boost::intrusive_ptr<formula_object> obj(formula_object::create(type.as_string(), variant()));
	cache[type] = obj;
	return variant(obj.get());
FUNCTION_TYPE_DEF
	variant literal;
	args()[0]->isLiteral(literal);
	if(literal.is_string()) {
		return variant_type::get_class(literal.as_string());
	} else {
		return variant_type::get_any();
	}
END_FUNCTION_DEF(singleton)

FUNCTION_DEF(construct, 1, 2, "construct(string typename, arg): construct an object with the given typename")
	Formula::failIfStaticContext();
	variant type = args()[0]->evaluate(variables);
	variant arg;
	if(args().size() >= 2) {
		arg = args()[1]->evaluate(variables);
	}

	boost::intrusive_ptr<formula_object> obj(formula_object::create(type.as_string(), arg));
	return variant(obj.get());
FUNCTION_TYPE_DEF
	variant literal;
	args()[0]->isLiteral(literal);
	if(literal.is_string()) {
		return variant_type::get_class(literal.as_string());
	} else {
		return variant_type::get_any();
	}
END_FUNCTION_DEF(construct)

class update_object_command : public game_logic::CommandCallable
{
	boost::intrusive_ptr<formula_object> target_, src_;
public:
	update_object_command(boost::intrusive_ptr<formula_object> target,
	                      boost::intrusive_ptr<formula_object> src)
	  : target_(target), src_(src)
	{}
	virtual void execute(game_logic::FormulaCallable& ob) const {
		target_->update(*src_);
	}
};

FUNCTION_DEF(update_object, 2, 2, "update_object(target_instance, src_instance)")

	boost::intrusive_ptr<formula_object> target = args()[0]->evaluate(variables).convert_to<formula_object>();
	boost::intrusive_ptr<formula_object> src = args()[1]->evaluate(variables).convert_to<formula_object>();
	return variant(new update_object_command(target, src));

FUNCTION_TYPE_DEF
	return variant_type::get_commands();
END_FUNCTION_DEF(update_object)

FUNCTION_DEF(delay_until_end_of_loading, 1, 1, "delay_until_end_of_loading(string): delays evaluation of the enclosed until loading is finished")
	Formula::failIfStaticContext();
	variant s = args()[0]->evaluate(variables);
	ConstFormulaPtr f(Formula::create_optional_formula(s));
	if(!f) {
		return variant();
	}

	ConstFormulaCallablePtr callable(&variables);

	return variant::create_delayed(f, callable);
END_FUNCTION_DEF(delay_until_end_of_loading)

#if defined(USE_LUA)
FUNCTION_DEF(eval_lua, 1, 1, "eval_lua(str)")
	Formula::failIfStaticContext();
	variant value = args()[0]->evaluate(variables);

	return variant(new FnCommandCallableArg([=](FormulaCallable* callable) {
		lua::LuaContext context;
		context.execute(value, callable);
	}));

FUNCTION_ARGS_DEF
	ARG_TYPE("string|builtin lua_compiled");
FUNCTION_TYPE_DEF
	return variant_type::get_commands();
END_FUNCTION_DEF(eval_lua)

FUNCTION_DEF(compile_lua, 1, 1, "compile_lua(str)")
	Formula::failIfStaticContext();
	const std::string s = args()[0]->evaluate(variables).as_string();

	lua::LuaContext ctx;
	return variant(ctx.compile("", s).get());
FUNCTION_ARGS_DEF
	ARG_TYPE("string");
	RETURN_TYPE("builtin lua_compiled");
END_FUNCTION_DEF(compile_lua)
#endif

FUNCTION_DEF(eval_no_recover, 1, 2, "eval_no_recover(str, [arg]): evaluate the given string as FFL")
	ConstFormulaCallablePtr callable(&variables);

	if(args().size() > 1) {
		const variant v = args()[1]->evaluate(variables);
		if(v.is_map()) {
			callable = map_into_callable(v);
		} else {
			callable.reset(v.try_convert<const FormulaCallable>());
			ASSERT_LOG(callable.get() != NULL, "COULD NOT CONVERT TO CALLABLE: " << v.string_cast());
		}
	}

	variant s = args()[0]->evaluate(variables);

	static std::map<std::string, ConstFormulaPtr> cache;
	ConstFormulaPtr& f = cache[s.as_string()];
	if(!f) {
		f = ConstFormulaPtr(Formula::create_optional_formula(s));
	}

	ASSERT_LOG(f.get() != NULL, "ILLEGAL FORMULA GIVEN TO eval: " << s.as_string());
	return f->execute(*callable);

FUNCTION_ARGS_DEF
	ARG_TYPE("string");
END_FUNCTION_DEF(eval_no_recover)

FUNCTION_DEF(eval, 1, 2, "eval(str, [arg]): evaluate the given string as FFL")
	ConstFormulaCallablePtr callable(&variables);

	if(args().size() > 1) {
		const variant v = args()[1]->evaluate(variables);
		if(v.is_map()) {
			callable = map_into_callable(v);
		} else {
			callable.reset(v.try_convert<const FormulaCallable>());
			ASSERT_LOG(callable.get() != NULL, "COULD NOT CONVERT TO CALLABLE: " << v.string_cast());
		}
	}

	variant s = args()[0]->evaluate(variables);
	try {
		static std::map<std::string, ConstFormulaPtr> cache;
		const assert_recover_scope recovery_scope;

		ConstFormulaPtr& f = cache[s.as_string()];
		if(!f) {
			f = ConstFormulaPtr(Formula::create_optional_formula(s));
		}

		if(!f) {
			return variant();
		}

		return f->execute(*callable);
	} catch(type_error&) {
	} catch(validation_failure_exception&) {
	}
	LOG_ERROR("ERROR IN EVAL");
	return variant();

FUNCTION_ARGS_DEF
	ARG_TYPE("string");
END_FUNCTION_DEF(eval)

FUNCTION_DEF(handle_errors, 2, 2, "handle_errors(expr, failsafe): evaluates 'expr' and returns it. If expr has fatal errors in evaluation, return failsafe instead. 'failsafe' is an expression which receives 'error_msg' and 'context' as parameters.")
	const assert_recover_scope recovery_scope;
	try {
		return args()[0]->evaluate(variables);
	} catch(validation_failure_exception& e) {
		boost::intrusive_ptr<MapFormulaCallable> callable(new MapFormulaCallable(&variables));
		callable->add("context", variant(&variables));
		callable->add("error_msg", variant(e.msg));
		return args()[1]->evaluate(*callable);
	}
FUNCTION_TYPE_DEF
	return args()[0]->queryVariantType();
END_FUNCTION_DEF(handle_errors)

FUNCTION_DEF(switch, 3, -1, "switch(value, case1, result1, case2, result2 ... casen, resultn, default) -> value: returns resultn where value = casen, or default otherwise.")
	variant var = args()[0]->evaluate(variables);
	for(size_t n = 1; n < args().size()-1; n += 2) {
		variant val = args()[n]->evaluate(variables);
		if(val == var) {
			return args()[n+1]->evaluate(variables);
		}
	}

	if((args().size()%2) == 0) {
		return args().back()->evaluate(variables);
	} else {
		return variant();
	}
FUNCTION_TYPE_DEF
	std::vector<variant_type_ptr> types;
	for(unsigned n = 2; n < args().size(); ++n) {
		if(n%2 == 0 || n == args().size()-1) {
			types.push_back(args()[n]->queryVariantType());
		}
	}

	return variant_type::get_union(types);
END_FUNCTION_DEF(switch)

FUNCTION_DEF(query, 2, 2, "query(object, str): evaluates object.str")
	variant callable = args()[0]->evaluate(variables);
	variant str = args()[1]->evaluate(variables);
	return callable.as_callable()->queryValue(str.as_string());
END_FUNCTION_DEF(query)

FUNCTION_DEF(call, 2, 2, "call(fn, list): calls the given function with 'list' as the arguments")
	variant fn = args()[0]->evaluate(variables);
	variant a = args()[1]->evaluate(variables);
	return fn(a.as_list());
FUNCTION_TYPE_DEF
	variant_type_ptr fn_type = args()[0]->queryVariantType();
	variant_type_ptr return_type;
	if(fn_type->is_function(NULL, &return_type, NULL)) {
		return return_type;
	}

	return variant_type_ptr();

FUNCTION_ARGS_DEF
	ARG_TYPE("function");
	ARG_TYPE("list");
END_FUNCTION_DEF(call)


FUNCTION_DEF(abs, 1, 1, "abs(value) -> value: evaluates the absolute value of the value given")
	variant v = args()[0]->evaluate(variables);
	if(v.is_decimal()) {
		const decimal d = v.as_decimal();
		return variant(d >= 0 ? d : -d);
	} else {
		const int n = v.as_int();
		return variant(n >= 0 ? n : -n);
	}

FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return args()[0]->queryVariantType();
END_FUNCTION_DEF(abs)

FUNCTION_DEF(sign, 1, 1, "sign(value) -> value: evaluates to 1 if positive, -1 if negative, and 0 if 0")
	const decimal n = args()[0]->evaluate(variables).as_decimal();
	if(n > 0) {
		return variant(1);
	} else if(n < 0) {
		return variant(-1);
	} else {
		return variant(0);
	}

FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_INT);
END_FUNCTION_DEF(sign)

FUNCTION_DEF(median, 1, -1, "median(args...) -> value: evaluates to the median of the given arguments. If given a single argument list, will evaluate to the median of the member items.")
	if(args().size() == 3) {
		//special case for 3 arguments since it's a common case.
		variant a = args()[0]->evaluate(variables);
		variant b = args()[1]->evaluate(variables);
		variant c = args()[2]->evaluate(variables);
		if(a < b) {
			if(b < c) {
				return b;
			} else if(a < c) {
				return c;
			} else {
				return a;
			}
		} else {
			if(a < c) {
				return a;
			} else if(b < c) {
				return c;
			} else {
				return b;
			}
		}
	}

	std::vector<variant> items;
	if(args().size() != 1) {
		items.reserve(args().size());
	}

	for(size_t n = 0; n != args().size(); ++n) {
		const variant v = args()[n]->evaluate(variables);
		if(args().size() == 1 && v.is_list()) {
			items = v.as_list();
		} else {
			items.push_back(v);
		}
	}

	std::sort(items.begin(), items.end());
	if(items.empty()) {
		return variant();
	} else if(items.size()&1) {
		return items[items.size()/2];
	} else {
		return (items[items.size()/2-1] + items[items.size()/2])/variant(2);
	}
FUNCTION_TYPE_DEF
	if(args().size() == 1) {
		return args()[0]->queryVariantType()->is_list_of();
	} else {
		std::vector<variant_type_ptr> types;
		for(int n = 0; n != args().size(); ++n) {
			types.push_back(args()[n]->queryVariantType());
		}
        
		return variant_type::get_union(types);
	}
END_FUNCTION_DEF(median)

FUNCTION_DEF(min, 1, -1, "min(args...) -> value: evaluates to the minimum of the given arguments. If given a single argument list, will evaluate to the minimum of the member items.")

	bool found = false;
	variant res;
	for(size_t n = 0; n != args().size(); ++n) {
		const variant v = args()[n]->evaluate(variables);
		if(v.is_list() && args().size() == 1) {
			for(size_t m = 0; m != v.num_elements(); ++m) {
				if(!found || v[m] < res) {
					res = v[m];
					found = true;
				}
			}
		} else {
			if(!found || v < res) {
				res = v;
				found = true;
			}
		}
	}

	return res;
FUNCTION_TYPE_DEF
	if(args().size() == 1) {
		return args()[0]->queryVariantType()->is_list_of();
	} else {
		std::vector<variant_type_ptr> types;
		for(int n = 0; n != args().size(); ++n) {
			types.push_back(args()[n]->queryVariantType());
		}

		return variant_type::get_union(types);
	}
END_FUNCTION_DEF(min)

FUNCTION_DEF(max, 1, -1, "max(args...) -> value: evaluates to the maximum of the given arguments. If given a single argument list, will evaluate to the maximum of the member items.")

	bool found = false;
	variant res;
	for(size_t n = 0; n != args().size(); ++n) {
		const variant v = args()[n]->evaluate(variables);
		if(v.is_list() && args().size() == 1) {
			for(size_t m = 0; m != v.num_elements(); ++m) {
				if(!found || v[m] > res) {
					res = v[m];
					found = true;
				}
			}
		} else {
			if(!found || v > res) {
				res = v;
				found = true;
			}
		}
	}

	return res;
FUNCTION_TYPE_DEF
	if(args().size() == 1) {
		std::vector<variant_type_ptr> items;
		variant_type_ptr result = args()[0]->queryVariantType()->is_list_of();
		items.push_back(result);
		items.push_back(variant_type::get_type(variant::VARIANT_TYPE_NULL));
		return variant_type::get_union(items);
	} else {
		std::vector<variant_type_ptr> types;
		for(int n = 0; n != args().size(); ++n) {
			types.push_back(args()[n]->queryVariantType());
		}

		return variant_type::get_union(types);
	}
END_FUNCTION_DEF(max)

	UNIT_TEST(min_max_decimal) {
		CHECK(game_logic::Formula(variant("max(1,1.4)")).execute() == game_logic::Formula(variant("1.4")).execute(), "test failed");
	}

FUNCTION_DEF(mix, 3, 3, "mix(x, y, ratio): equal to x*(1-ratio) + y*ratio")
	decimal ratio = args()[2]->evaluate(variables).as_decimal();
	return variant(args()[0]->evaluate(variables).as_decimal() * (decimal::from_int(1) - ratio) + args()[1]->evaluate(variables).as_decimal() * ratio);

FUNCTION_ARGS_DEF
	ARG_TYPE("decimal");
	ARG_TYPE("decimal");
	ARG_TYPE("decimal");

FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
	
END_FUNCTION_DEF(mix)

FUNCTION_DEF(keys, 1, 1, "keys(map) -> list: gives the keys for a map")
	const variant map = args()[0]->evaluate(variables);
	if(map.is_callable()) {
		std::vector<variant> v;
		const std::vector<FormulaInput> inputs = map.as_callable()->inputs();
		for(const FormulaInput& in : inputs) {
			v.push_back(variant(in.name));
		}

		return variant(&v);
	}

	return map.getKeys();

FUNCTION_ARGS_DEF
	ARG_TYPE("map");
FUNCTION_TYPE_DEF
	return variant_type::get_list(args()[0]->queryVariantType()->is_map_of().first);
END_FUNCTION_DEF(keys)

FUNCTION_DEF(values, 1, 1, "values(map) -> list: gives the values for a map")
	const variant map = args()[0]->evaluate(variables);
	return map.getValues();
FUNCTION_ARGS_DEF
	ARG_TYPE("map");
FUNCTION_TYPE_DEF
	return variant_type::get_list(args()[0]->queryVariantType()->is_map_of().second);
END_FUNCTION_DEF(values)

FUNCTION_DEF(wave, 1, 1, "wave(int) -> int: a wave with a period of 1000 and height of 1000")
	const int value = args()[0]->evaluate(variables).as_int()%1000;
	const double angle = 2.0*3.141592653589*(static_cast<double>(value)/1000.0);
	return variant(static_cast<int>(sin(angle)*1000.0));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_INT);
END_FUNCTION_DEF(wave)

FUNCTION_DEF(decimal, 1, 1, "decimal(value) -> decimal: converts the value to a decimal")
	return variant(args()[0]->evaluate(variables).as_decimal());
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(decimal)

FUNCTION_DEF(int, 1, 1, "int(value) -> int: converts the value to an integer")
	variant v = args()[0]->evaluate(variables);
	if(v.is_string()) {
		try {
			return variant(boost::lexical_cast<int>(v.as_string()));
		} catch(...) {
			ASSERT_LOG(false, "Could not parse string as integer: " << v.write_json());
		}
	}
	return variant(v.as_int());
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_INT);
END_FUNCTION_DEF(int)

FUNCTION_DEF(bool, 1, 1, "bool(value) -> bool: converts the value to a boolean")
	variant v = args()[0]->evaluate(variables);
	return variant::from_bool(v.as_bool());
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
END_FUNCTION_DEF(bool)

FUNCTION_DEF(sin, 1, 1, "sin(x): Standard sine function.")
	const float angle = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<decimal>(sin(angle/radians_to_degrees)));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(sin)

FUNCTION_DEF(cos, 1, 1, "cos(x): Standard cosine function.")
	const float angle = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<decimal>(cos(angle/radians_to_degrees)));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(cos)

FUNCTION_DEF(tan, 1, 1, "tan(x): Standard tangent function.")
	const float angle = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<decimal>(tan(angle/radians_to_degrees)));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(tan)

FUNCTION_DEF(asin, 1, 1, "asin(x): Standard arc sine function.")
	const float ratio = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<decimal>(asin(ratio)*radians_to_degrees));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(asin)

FUNCTION_DEF(acos, 1, 1, "acos(x): Standard arc cosine function.")
	const float ratio = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<decimal>(acos(ratio)*radians_to_degrees));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(acos)

FUNCTION_DEF(atan, 1, 1, "atan(x): Standard arc tangent function.")
	const float ratio = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<decimal>(atan(ratio)*radians_to_degrees));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(atan)

FUNCTION_DEF(sinh, 1, 1, "sinh(x): Standard hyperbolic sine function.")
	const float angle = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<decimal>(sinh(angle)));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(sinh)

FUNCTION_DEF(cosh, 1, 1, "cosh(x): Standard hyperbolic cosine function.")
	const float angle = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<decimal>(cosh(angle)));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(cosh)

FUNCTION_DEF(tanh, 1, 1, "tanh(x): Standard hyperbolic tangent function.")
	const float angle = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<decimal>(tanh(angle)));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(tanh)

FUNCTION_DEF(asinh, 1, 1, "asinh(x): Standard arc hyperbolic sine function.")
	const float ratio = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<decimal>(asinh(ratio)));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(asinh)

FUNCTION_DEF(acosh, 1, 1, "acosh(x): Standard arc hyperbolic cosine function.")
	const float ratio = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<decimal>(acosh(ratio)));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(acosh)

FUNCTION_DEF(atanh, 1, 1, "atanh(x): Standard arc hyperbolic tangent function.")
	const float ratio = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<decimal>(atanh(ratio)));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(atanh)

FUNCTION_DEF(sqrt, 1, 1, "sqrt(x): Returns the square root of x.")
	const double value = args()[0]->evaluate(variables).as_double();
	ASSERT_LOG(value >= 0, "We don't support the square root of negative numbers: " << value);
	return variant(decimal(sqrt(value)));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(sqrt)

FUNCTION_DEF(hypot, 2, 2, "hypot(x,y): Compute the hypotenuse of a triangle without the normal loss of precision incurred by using the pythagoream theorem.")
	const double x = args()[0]->evaluate(variables).as_double();
	const double y = args()[1]->evaluate(variables).as_double();
	return variant(hypot(x,y));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(hypot)
	
	
FUNCTION_DEF(exp, 1, 1, "exp(x): Calculate the exponential function of x, whatever that means.")
	const float input = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<decimal>(expf(input)));
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
END_FUNCTION_DEF(exp)
    
FUNCTION_DEF(angle, 4, 4, "angle(x1, y1, x2, y2) -> int: Returns the angle, from 0°, made by the line described by the two points (x1, y1) and (x2, y2).")
	const float a = args()[0]->evaluate(variables).as_float();
	const float b = args()[1]->evaluate(variables).as_float();
	const float c = args()[2]->evaluate(variables).as_float();
	const float d = args()[3]->evaluate(variables).as_float();
	return variant(static_cast<int64_t>(bmround((atan2(a-c, b-d)*radians_to_degrees+90)*VARIANT_DECIMAL_PRECISION)*-1), variant::DECIMAL_VARIANT);
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
	ARG_TYPE("int|decimal");
	ARG_TYPE("int|decimal");
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_INT);
END_FUNCTION_DEF(angle)

FUNCTION_DEF(angle_delta, 2, 2, "angle_delta(a, b) -> int: Given two angles, returns the smallest rotation needed to make a equal to b.")
	int a = args()[0]->evaluate(variables).as_int();
	int b = args()[1]->evaluate(variables).as_int();
	while(abs(a - b) > 180) {
		if(a < b) {
			a += 360;
		} else {
			b += 360;
		}
	}

	return variant(b - a);
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_INT);
END_FUNCTION_DEF(angle_delta)

FUNCTION_DEF(orbit, 4, 4, "orbit(x, y, angle, dist) -> [x,y]: Returns the point as a list containing an x/y pair which is dist away from the point as defined by x and y passed in, at the angle passed in.")
	const float x = args()[0]->evaluate(variables).as_float();
	const float y = args()[1]->evaluate(variables).as_float();
	const float ang = args()[2]->evaluate(variables).as_float();
	const float dist = args()[3]->evaluate(variables).as_float();
	
	const float u = (dist * cos(ang/radians_to_degrees)) + x;   //TODO Find out why whole number decimals are returned.
	const float v = (dist * sin(ang/radians_to_degrees)) + y;

	std::vector<variant> result;
	result.reserve(2);
	result.push_back(variant(decimal(u)));
	result.push_back(variant(decimal(v)));
	
	return variant(&result);
FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal");
	ARG_TYPE("int|decimal");
	ARG_TYPE("int|decimal");
	ARG_TYPE("int|decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_list(variant_type::get_type(variant::VARIANT_TYPE_DECIMAL));
END_FUNCTION_DEF(orbit)


FUNCTION_DEF(floor, 1, 1, "Returns the smaller near integer. 3.9 -> 3, 3.3 -> 3, 3 -> 3")
	const float a = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<int>(floor(a)));
FUNCTION_ARGS_DEF
	ARG_TYPE("decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_INT);
END_FUNCTION_DEF(floor)

FUNCTION_DEF(round, 1, 1, "Returns the smaller near integer. 3.9 -> 3, 3.3 -> 3, 3 -> 3")
	const double a = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<int>(bmround(a)));
FUNCTION_ARGS_DEF
	ARG_TYPE("decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_INT);
END_FUNCTION_DEF(round)

FUNCTION_DEF(ceil, 1, 1, "Returns the smaller near integer. 3.9 -> 3, 3.3 -> 3, 3 -> 3")
	const float a = args()[0]->evaluate(variables).as_float();
	return variant(static_cast<int>(ceil(a)));
FUNCTION_ARGS_DEF
	ARG_TYPE("decimal");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_INT);
END_FUNCTION_DEF(ceil)


FUNCTION_DEF(regex_replace, 3, 3, "regex_replace(string, string, string) -> string: Unknown.")
	const std::string str = args()[0]->evaluate(variables).as_string();
	const boost::regex re(args()[1]->evaluate(variables).as_string());
	const std::string value = args()[2]->evaluate(variables).as_string();
	return variant(boost::regex_replace(str, re, value));
FUNCTION_ARGS_DEF
	ARG_TYPE("string");
	ARG_TYPE("string");
	ARG_TYPE("string");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_STRING);
END_FUNCTION_DEF(regex_replace)

FUNCTION_DEF(regex_match, 2, 2, "regex_match(string, re_string) -> string: returns null if not found, else returns the whole string or a list of sub-strings depending on whether blocks were demarcated.")
	const std::string str = args()[0]->evaluate(variables).as_string();
	const boost::regex re(args()[1]->evaluate(variables).as_string());
	 boost::match_results<std::string::const_iterator> m;
	if(boost::regex_match(str, m, re) == false) {
		return variant();
	}
	if(m.size() == 1) {
		return variant(std::string(m[0].first, m[0].second));
	} 
	std::vector<variant> v;
	for(size_t i = 1; i < m.size(); i++) {
		v.push_back(variant(std::string(m[i].first, m[i].second)));
	}
	return variant(&v);
FUNCTION_ARGS_DEF
	ARG_TYPE("string");
	ARG_TYPE("string");
FUNCTION_TYPE_DEF
	std::vector<variant_type_ptr> types;
	types.push_back(variant_type::get_list(variant_type::get_type(variant::VARIANT_TYPE_STRING)));
	types.push_back(variant_type::get_type(variant::VARIANT_TYPE_STRING));
	types.push_back(variant_type::get_type(variant::VARIANT_TYPE_NULL));
	return variant_type::get_union(types);
END_FUNCTION_DEF(regex_match)

namespace {
class variant_comparator : public FormulaCallable {
	//forbid these so they can't be passed by value.
	variant_comparator(const variant_comparator&);
	void operator=(const variant_comparator&);

	ExpressionPtr expr_;
	const FormulaCallable* fallback_;
	mutable variant a_, b_;
	variant getValue(const std::string& key) const {
		if(key == "a") {
			return a_;
		} else if(key == "b") {
			return b_;
		} else {
			return fallback_->queryValue(key);
		}
	}

	variant getValueBySlot(int slot) const {
		if(slot == 0) {
			return a_;
		} else if(slot == 1) {
			return b_;
		}

		return fallback_->queryValueBySlot(slot - 2);
	}

	void getInputs(std::vector<FormulaInput>* inputs) const {
		fallback_->getInputs(inputs);
	}
public:
	variant_comparator(const ExpressionPtr& expr, const FormulaCallable& fallback) : FormulaCallable(false), expr_(expr), fallback_(&fallback)
	{}

	bool operator()(const variant& a, const variant& b) const {
		a_ = a;
		b_ = b;
		return expr_->evaluate(*this).as_bool();
	}

	variant eval(const variant& a, const variant& b) const {
		a_ = a;
		b_ = b;
		return expr_->evaluate(*this);
	}
};

class variant_comparator_definition : public FormulaCallableDefinition
{
public:
	variant_comparator_definition(ConstFormulaCallableDefinitionPtr base, variant_type_ptr type)
	  : base_(base), type_(type)
	{
		for(int n = 0; n != 2; ++n) {
			const std::string name = (n == 0) ? "a" : "b";
			entries_.push_back(Entry(name));
			entries_.back().setVariantType(type_);
		}
	}

	int getSlot(const std::string& key) const {
		if(key == "a") { return 0; }
		if(key == "b") { return 1; }

		if(base_) {
			int result = base_->getSlot(key);
			if(result >= 0) {
				result += 2;
			}

			return result;
		} else {
			return -1;
		}
	}

	Entry* getEntry(int slot) override {
		if(slot < 0) {
			return NULL;
		}

		if(static_cast<unsigned>(slot) < entries_.size()) {
			return &entries_[slot];
		}

		if(base_) {
			return const_cast<FormulaCallableDefinition*>(base_.get())->getEntry(slot - entries_.size());
		}

		return NULL;
	}

	const Entry* getEntry(int slot) const override {
		if(slot < 0) {
			return NULL;
		}

		if(static_cast<unsigned>(slot) < entries_.size()) {
			return &entries_[slot];
		}

		if(base_) {
			return base_->getEntry(slot - entries_.size());
		}

		return NULL;
	}

	int getNumSlots() const {
		return 2 + (base_ ? base_->getNumSlots() : 0);
	}

private:
	ConstFormulaCallableDefinitionPtr base_;
	variant_type_ptr type_;

	std::vector<Entry> entries_;
};
}

FUNCTION_DEF(fold, 2, 3, "fold(list, expr, [default]) -> value")
	variant list = args()[0]->evaluate(variables);
	const int size = list.num_elements();
	if(size == 0) {
		if(args().size() >= 3) {
			return args()[2]->evaluate(variables);
		} else {
			return variant();
		}
	} else if(size == 1) {
		return list[0];
	}

	boost::intrusive_ptr<variant_comparator> callable(new variant_comparator(args()[1], variables));

	variant a = list[0];
	for(unsigned n = 1; n < list.num_elements(); ++n) {
		a = callable->eval(a, list[n]);
	}

	return a;
FUNCTION_ARGS_DEF
	ARG_TYPE("list");
FUNCTION_TYPE_DEF
	std::vector<variant_type_ptr> types;
	types.push_back(args()[1]->queryVariantType());
	if(args().size() > 2) {
		types.push_back(args()[2]->queryVariantType());
	}

	return variant_type::get_union(types);
END_FUNCTION_DEF(fold)

FUNCTION_DEF(unzip, 1, 1, "unzip(list of lists) -> list of lists: Converts [[1,4],[2,5],[3,6]] -> [[1,2,3],[4,5,6]]")
	variant item1 = args()[0]->evaluate(variables);
	ASSERT_LOG(item1.is_list(), "unzip function arguments must be a list");

	// Calculate breadth and depth of new list.
	const int depth = item1.num_elements();
	size_t breadth = 0;
	for(size_t n = 0; n < item1.num_elements(); ++n) {
		ASSERT_LOG(item1[n].is_list(), "Item " << n << " on list isn't list");
		breadth = std::max(item1[n].num_elements(), breadth);
	}

	std::vector<std::vector<variant> > v;
	for(size_t n = 0; n < breadth; ++n) {
		std::vector<variant> e1;
		e1.resize(depth);
		v.push_back(e1);
	}

	for(size_t n = 0; n < item1.num_elements(); ++n) {
		for(size_t m = 0; m < item1[n].num_elements(); ++m) {
			v[m][n] = item1[n][m];
		}
	}

	std::vector<variant> vl;
	for(size_t n = 0; n < v.size(); ++n) {
		vl.push_back(variant(&v[n]));
	}
	return variant(&vl);
FUNCTION_ARGS_DEF
	ARG_TYPE("[list]");
END_FUNCTION_DEF(unzip)

FUNCTION_DEF(zip, 2, 3, "zip(list1, list2, expr=null) -> list")
	const variant item1 = args()[0]->evaluate(variables);
	const variant item2 = args()[1]->evaluate(variables);

	ASSERT_LOG(item1.type() == item2.type(), "zip function arguments must both be the same type.");
	ASSERT_LOG(item1.is_list() || item1.is_map(), "zip function arguments must be either lists or maps");

	boost::intrusive_ptr<variant_comparator> callable;
	
	if(args().size() > 2) {
		callable.reset(new variant_comparator(args()[2], variables));
	}
	const int size = std::min(item1.num_elements(), item2.num_elements());

	if(item1.is_list()) {
		std::vector<variant> result;
		// is list
		for(int n = 0; n < size; ++n) {
			if(callable) {
				result.push_back(callable->eval(item1[n], item2[n]));
			} else {
				result.push_back(item1[n] + item2[n]);
			}
		}
		return variant(&result);
	} else {
		std::map<variant,variant> retMap(item1.as_map());
		variant keys = item2.getKeys();
		for(int n = 0; n != keys.num_elements(); n++) {
			if(retMap[keys[n]].is_null() == false) {
				if(callable) {
					retMap[keys[n]] = callable->eval(retMap[keys[n]], item2[keys[n]]);
				} else {
					retMap[keys[n]] = retMap[keys[n]] + item2[keys[n]];
				}
			} else {
				retMap[keys[n]] = item2[keys[n]];
			}
		}
		return variant(&retMap);
	}
	return variant();
FUNCTION_ARGS_DEF
	ARG_TYPE("list|map");
	ARG_TYPE("list|map");
FUNCTION_TYPE_DEF
	variant_type_ptr type_a = args()[0]->queryVariantType();
	variant_type_ptr type_b = args()[1]->queryVariantType();

	if(args().size() <= 2) {
		std::vector<variant_type_ptr> v;
		v.push_back(type_a);
		v.push_back(type_b);
		return variant_type::get_union(v);
	}

	if(type_a->is_specific_list() && type_b->is_specific_list()) {
		std::vector<variant_type_ptr> types;
		const int num_elements = std::min(type_a->is_specific_list()->size(), type_b->is_specific_list()->size());
		const variant_type_ptr type = args()[2]->queryVariantType();
		for(int n = 0; n != num_elements; ++n) {
			types.push_back(type);
		}

		return variant_type::get_specific_list(types);
	} else if(type_a->is_list_of()) {
		return variant_type::get_list(args()[2]->queryVariantType());
	} else {
		std::pair<variant_type_ptr,variant_type_ptr> map_a = type_a->is_map_of();
		std::pair<variant_type_ptr,variant_type_ptr> map_b = type_b->is_map_of();
		if(map_a.first && map_b.first) {
			std::vector<variant_type_ptr> key;
			key.push_back(map_a.first);
			key.push_back(map_b.first);
			return variant_type::get_map(variant_type::get_union(key), args()[2]->queryVariantType());
		}
	}

	return variant_type::get_any();
END_FUNCTION_DEF(zip)

FUNCTION_DEF(float_array, 1, 2, "float_array(list, (opt) num_elements) -> callable: Converts a list of floating point values into an efficiently accessible object.")
	game_logic::Formula::failIfStaticContext();
	variant f = args()[0]->evaluate(variables);
	int num_elems = args().size() == 1 ? 1 : args()[1]->evaluate(variables).as_int();
	std::vector<float> floats;
	for(size_t n = 0; n < f.num_elements(); ++n) {
		floats.push_back(f[n].as_float());
	}
	return variant(new FloatArrayCallable(&floats, num_elems));
FUNCTION_ARGS_DEF
	ARG_TYPE("[decimal|int]");
	ARG_TYPE("int");
END_FUNCTION_DEF(float_array)

FUNCTION_DEF(short_array, 1, 2, "short_array(list) -> callable: Converts a list of integer values into an efficiently accessible object.")
	game_logic::Formula::failIfStaticContext();
	variant s = args()[0]->evaluate(variables);
	int num_elems = args().size() == 1 ? 1 : args()[1]->evaluate(variables).as_int();
	std::vector<short> shorts;
	for(size_t n = 0; n < s.num_elements(); ++n) {
		shorts.push_back(static_cast<short>(s[n].as_int()));
	}
	return variant(new ShortArrayCallable(&shorts, num_elems));
FUNCTION_ARGS_DEF
	ARG_TYPE("[int]");
END_FUNCTION_DEF(short_array)

FUNCTION_DEF(generate_uuid, 0, 0, "generate_uuid() -> string: generates a unique string")
	game_logic::Formula::failIfStaticContext();
	return variant(write_uuid(generate_uuid()));
FUNCTION_ARGS_DEF
	RETURN_TYPE("string directed_graph")
END_FUNCTION_DEF(generate_uuid)

/* XXX Krista to be reworked
FUNCTION_DEF(update_controls, 1, 1, "update_controls(map) : Updates the controls based on a list of id:string, pressed:bool pairs")
	const variant map = args()[0]->evaluate(variables);
	for(const auto& p : map.as_map()) {
		LOG_INFO("Button: " << p.first.as_string() << " " << (p.second.as_bool() ? "Pressed" : "Released"));
		controls::update_control_state(p.first.as_string(), p.second.as_bool());
	}
	return variant();
END_FUNCTION_DEF(update_controls)

FUNCTION_DEF(map_controls, 1, 1, "map_controls(map) : Creates or updates the mapping on controls to keys")
	const variant map = args()[0]->evaluate(variables);
	for(const auto& p : map.as_map()) {
		controls::set_mapped_key(p.first.as_string(), static_cast<SDL_Keycode>(p.second.as_int()));
	}
	return variant();
END_FUNCTION_DEF(map_controls)*/

FUNCTION_DEF(directed_graph, 2, 2, "directed_graph(list_of_vertexes, adjacent_expression) -> a directed graph")
	variant vertices = args()[0]->evaluate(variables);
	pathfinding::graph_edge_list edges;
	
	std::vector<variant> vertex_list;
	boost::intrusive_ptr<MapFormulaCallable> callable(new MapFormulaCallable(&variables));
	variant& a = callable->addDirectAccess("v");
	for(variant v : vertices.as_list()) {
		a = v;
		edges[v] = args()[1]->evaluate(*callable).as_list();
		vertex_list.push_back(v);
	}
	pathfinding::directed_graph* dg = new pathfinding::directed_graph(&vertex_list, &edges);
	return variant(dg);
FUNCTION_ARGS_DEF
	ARG_TYPE("list")
	ARG_TYPE("any")
	RETURN_TYPE("builtin directed_graph")
END_FUNCTION_DEF(directed_graph)

FUNCTION_DEF(weighted_graph, 2, 2, "weighted_graph(directed_graph, weight_expression) -> a weighted directed graph")
        variant graph = args()[0]->evaluate(variables);		
        pathfinding::directed_graph_ptr dg = boost::intrusive_ptr<pathfinding::directed_graph>(graph.try_convert<pathfinding::directed_graph>());
        ASSERT_LOG(dg, "Directed graph given is not of the correct type. " /*<< variant::variant_type_to_string(graph.type())*/);
        pathfinding::edge_weights w;
 
        boost::intrusive_ptr<variant_comparator> callable(new variant_comparator(args()[1], variables));
 
        for(auto edges = dg->get_edges()->begin();
                edges != dg->get_edges()->end();
                edges++) {
                for(auto e2 : edges->second) {
                        variant v = callable->eval(edges->first, e2);
                        if(v.is_null() == false) {
                                w[pathfinding::graph_edge(edges->first, e2)] = v.as_decimal();
                        }
                }
        }
        return variant(new pathfinding::weighted_directed_graph(dg, &w));
FUNCTION_ARGS_DEF
        ARG_TYPE("builtin directed_graph")
        RETURN_TYPE("builtin weighted_directed_graph")
END_FUNCTION_DEF(weighted_graph)

FUNCTION_DEF(a_star_search, 4, 4, "a_star_search(weighted_directed_graph, src_node, dst_node, heuristic) -> A list of nodes which represents the 'best' path from src_node to dst_node.")
	variant graph = args()[0]->evaluate(variables);
	pathfinding::weighted_directed_graph_ptr wg = graph.try_convert<pathfinding::weighted_directed_graph>();
	ASSERT_LOG(wg, "Weighted graph given is not of the correct type.");
	variant src_node = args()[1]->evaluate(variables);
	variant dst_node = args()[2]->evaluate(variables);
	ExpressionPtr heuristic = args()[3];
	boost::intrusive_ptr<MapFormulaCallable> callable(new MapFormulaCallable(&variables));
	return pathfinding::a_star_search(wg, src_node, dst_node, heuristic, callable);
FUNCTION_ARGS_DEF
	ARG_TYPE("builtin weighted_directed_graph")
	ARG_TYPE("any")
	ARG_TYPE("any")
	ARG_TYPE("any")
	RETURN_TYPE("list")
END_FUNCTION_DEF(a_star_search)

FUNCTION_DEF(path_cost_search, 3, 3, "path_cost_search(weighted_directed_graph, src_node, max_cost) -> A list of all possible points reachable from src_node within max_cost.")
	variant graph = args()[0]->evaluate(variables);
	pathfinding::weighted_directed_graph_ptr wg = graph.try_convert<pathfinding::weighted_directed_graph>();
	ASSERT_LOG(wg, "Weighted graph given is not of the correct type.");
	variant src_node = args()[1]->evaluate(variables);
	decimal max_cost(args()[2]->evaluate(variables).as_decimal());
	return pathfinding::path_cost_search(wg, src_node, max_cost);
FUNCTION_ARGS_DEF
	ARG_TYPE("builtin weighted_directed_graph")
	ARG_TYPE("any")
	ARG_TYPE("decimal|int")
	RETURN_TYPE("list")
END_FUNCTION_DEF(path_cost_search)

FUNCTION_DEF(create_graph_from_level, 1, 3, "create_graph_from_level(level, (optional) tile_size_x, (optional) tile_size_y) -> directed graph : Creates a directed graph based on the current level.")
	int tile_size_x = TileSize;
	int tile_size_y = TileSize;
	if(args().size() == 2) {
		tile_size_y = tile_size_x = args()[1]->evaluate(variables).as_int();
	} else if(args().size() == 3) {
		tile_size_x = args()[1]->evaluate(variables).as_int();
		tile_size_y = args()[2]->evaluate(variables).as_int();
	}
	ASSERT_LOG((tile_size_x%2)==0 && (tile_size_y%2)==0, "The tile_size_x and tile_size_y values *must* be even. (" << tile_size_x << "," << tile_size_y << ")");
	variant curlevel = args()[0]->evaluate(variables);
	LevelPtr lvl = curlevel.try_convert<Level>();
	ASSERT_LOG(lvl, "The level parameter passed to the function was couldn't be converted.");
	rect b = lvl->boundaries();
	b.from_coordinates(b.x() - b.x()%tile_size_x, 
		b.y() - b.y()%tile_size_y, 
		b.x2()+(tile_size_x-b.x2()%tile_size_x), 
		b.y2()+(tile_size_y-b.y2()%tile_size_y));

	pathfinding::graph_edge_list edges;
	std::vector<variant> vertex_list;
	const rect& b_rect = Level::current().boundaries();

	for(int y = b.y(); y < b.y2(); y += tile_size_y) {
		for(int x = b.x(); x < b.x2(); x += tile_size_x) {
			if(!lvl->solid(x, y, tile_size_x, tile_size_y)) {
				variant l(pathfinding::point_as_variant_list(point(x,y)));
				vertex_list.push_back(l);
				std::vector<variant> e;
				point po(x,y);
				for(const point& p : pathfinding::get_neighbours_from_rect(po, tile_size_x, tile_size_y, b_rect)) {
					if(!lvl->solid(p.x, p.y, tile_size_x, tile_size_y)) {
						e.push_back(pathfinding::point_as_variant_list(p));
					}
				}
				edges[l] = e;
			}
		}
	}
	return variant(new pathfinding::directed_graph(&vertex_list, &edges));
END_FUNCTION_DEF(create_graph_from_level)

FUNCTION_DEF(plot_path, 6, 9, "plot_path(level, from_x, from_y, to_x, to_y, heuristic, (optional) weight_expr, (optional) tile_size_x, (optional) tile_size_y) -> list : Returns a list of points to get from (from_x, from_y) to (to_x, to_y)")
	int tile_size_x = TileSize;
	int tile_size_y = TileSize;
	ExpressionPtr weight_expr = ExpressionPtr();
	variant curlevel = args()[0]->evaluate(variables);
	LevelPtr lvl = curlevel.try_convert<Level>();
	if(args().size() > 6) {
		weight_expr = args()[6];
	}
	if(args().size() == 8) {
		tile_size_y = tile_size_x = args()[6]->evaluate(variables).as_int();
	} else if(args().size() == 9) {
		tile_size_x = args()[6]->evaluate(variables).as_int();
		tile_size_y = args()[7]->evaluate(variables).as_int();
	}
	ASSERT_LOG((tile_size_x%2)==0 && (tile_size_y%2)==0, "The tile_size_x and tile_size_y values *must* be even. (" << tile_size_x << "," << tile_size_y << ")");
	point src(args()[1]->evaluate(variables).as_int(), args()[2]->evaluate(variables).as_int());
	point dst(args()[3]->evaluate(variables).as_int(), args()[4]->evaluate(variables).as_int());
	ExpressionPtr heuristic = args()[4];
	boost::intrusive_ptr<MapFormulaCallable> callable(new MapFormulaCallable(&variables));
	return variant(pathfinding::a_star_find_path(lvl, src, dst, heuristic, weight_expr, callable, tile_size_x, tile_size_y));
END_FUNCTION_DEF(plot_path)

FUNCTION_DEF(sort, 1, 2, "sort(list, criteria): Returns a nicely-ordered list. If you give it an optional formula such as 'a>b' it will sort it according to that. This example favours larger numbers first instead of the default of smaller numbers first.")
	variant list = args()[0]->evaluate(variables);
	std::vector<variant> vars;
	vars.reserve(list.num_elements());
	for(size_t n = 0; n != list.num_elements(); ++n) {
		vars.push_back(list[n]);
	}

	if(args().size() == 1) {
		std::sort(vars.begin(), vars.end());
	} else {
		boost::intrusive_ptr<variant_comparator> comparator(new variant_comparator(args()[1], variables));
		std::sort(vars.begin(), vars.end(), [=](const variant& a, const variant& b) { return (*comparator)(a,b); });
	}

	return variant(&vars);
FUNCTION_ARGS_DEF
	ARG_TYPE("list");
FUNCTION_TYPE_DEF
	return args()[0]->queryVariantType();
END_FUNCTION_DEF(sort)

namespace {
//our own shuffle, to guarantee consistency across different machines.
template<typename RnIt>
void myshuffle(RnIt i1, RnIt i2)
{
	while(i2 - i1 > 1) {
		std::swap(*(i2-1), i1[rng::generate()%(i2-i1)]);
		--i2;
	}
}
}

FUNCTION_DEF(shuffle, 1, 1, "shuffle(list) - Returns a shuffled version of the list. Like shuffling cards.")
	variant list = args()[0]->evaluate(variables);
	boost::intrusive_ptr<FloatArrayCallable> f = list.try_convert<FloatArrayCallable>();
	if(f != NULL) {
		std::vector<float> floats(f->floats().begin(), f->floats().end());
		myshuffle(floats.begin(), floats.end());
		return variant(new FloatArrayCallable(&floats));
	}
	
	boost::intrusive_ptr<ShortArrayCallable> s = list.try_convert<ShortArrayCallable>();
	if(s != NULL) {
		std::vector<short> shorts(s->shorts().begin(), s->shorts().end());
		myshuffle(shorts.begin(), shorts.end());
		return variant(new ShortArrayCallable(&shorts));
	}

	std::vector<variant> vars;
	vars.reserve(list.num_elements());
	for(size_t n = 0; n != list.num_elements(); ++n) {
		vars.push_back(list[n]);
	}

	myshuffle(vars.begin(), vars.end());

	return variant(&vars);
FUNCTION_ARGS_DEF
	ARG_TYPE("list");
FUNCTION_TYPE_DEF
	return args()[0]->queryVariantType();
END_FUNCTION_DEF(shuffle)

FUNCTION_DEF(remove_from_map, 2, 2, "remove_from_map(map, key): Removes the given key from the map and returns it.")
	variant m = args()[0]->evaluate(variables);
	ASSERT_LOG(m.is_map(), "ARG PASSED TO remove_from_map() IS NOT A MAP");
	variant key = args()[1]->evaluate(variables);
	return m.remove_attr(key);
FUNCTION_ARGS_DEF
	ARG_TYPE("map");
FUNCTION_TYPE_DEF
	return args()[0]->queryVariantType();
END_FUNCTION_DEF(remove_from_map)
	
namespace {
	void flatten_items( variant items, std::vector<variant>* output){
		for(size_t n = 0; n != items.num_elements(); ++n) {
			
			if( items[n].is_list() ){
				flatten_items(items[n], output);
			} else {
				output->push_back(items[n]);
			}
			
		}
	}

	variant_type_ptr flatten_type(variant_type_ptr type) {

		const std::vector<variant_type_ptr>* items = type->is_union();
		if(items) {
			std::vector<variant_type_ptr> result;
			for(auto item : *items) {
				result.push_back(flatten_type(item));
			}

			return variant_type::get_union(result);
		}

		variant_type_ptr result = type->is_list_of();
		if(result) {
			return flatten_type(result);
		} else {
			return type;
		}
	}
	
}

FUNCTION_DEF(flatten, 1, 1, "flatten(list): Returns a list with a depth of 1 containing the elements of any list passed in.")
	variant input = args()[0]->evaluate(variables);
	std::vector<variant> output;
	flatten_items(input, &output);
	return variant(&output);
FUNCTION_TYPE_DEF
	return variant_type::get_list(flatten_type(args()[0]->queryVariantType()));
END_FUNCTION_DEF(flatten)

enum MAP_CALLABLE_SLOT { MAP_CALLABLE_VALUE, MAP_CALLABLE_INDEX, MAP_CALLABLE_CONTEXT, MAP_CALLABLE_KEY, NUM_MAP_CALLABLE_SLOTS };
static const std::string MapCallableFields[] = { "value", "index", "context", "key" };

class MapCallableDefinition : public FormulaCallableDefinition
{
public:
	MapCallableDefinition(ConstFormulaCallableDefinitionPtr base, variant_type_ptr key_type, variant_type_ptr value_type, const std::string& value_name)
	  : base_(base), key_type_(key_type), value_type_(value_type)
	{
		for(int n = 0; n != NUM_MAP_CALLABLE_SLOTS; ++n) {
			entries_.push_back(Entry(MapCallableFields[n]));
			std::string class_name;
			switch(n) {
			case MAP_CALLABLE_VALUE:
				if(!value_name.empty()) {
					entries_.back().id = value_name;
				}

				if(value_type_) {
					entries_.back().variant_type = value_type_;
					if(entries_.back().variant_type->is_class(&class_name)) {
						entries_.back().type_definition = get_class_definition(class_name);
					}
				}
				break;
			case MAP_CALLABLE_INDEX:
				entries_.back().variant_type = variant_type::get_type(variant::VARIANT_TYPE_INT);
				break;
			case MAP_CALLABLE_CONTEXT:
				entries_.back().variant_type = variant_type::get_type(variant::VARIANT_TYPE_CALLABLE);
				entries_.back().type_definition = base;
				break;
			case MAP_CALLABLE_KEY:
				if(key_type_) {
					entries_.back().variant_type = key_type_;
					if(key_type_->is_class(&class_name)) {
						entries_.back().type_definition = get_class_definition(class_name);
					}
				}
				break;
			}
		}
	}

	int getSlot(const std::string& key) const {
		for(int n = 0; n != entries_.size(); ++n) {
			if(entries_[n].id == key) {
				return n;
			}
		}

		if(base_) {
			int result = base_->getSlot(key);
			if(result >= 0) {
				result += NUM_MAP_CALLABLE_SLOTS;
			}

			return result;
		} else {
			return -1;
		}
	}

	Entry* getEntry(int slot) {
		if(slot < 0) {
			return NULL;
		}

		if(static_cast<unsigned>(slot) < entries_.size()) {
			return &entries_[slot];
		}

		if(base_) {
			return const_cast<FormulaCallableDefinition*>(base_.get())->getEntry(slot - NUM_MAP_CALLABLE_SLOTS);
		}

		return NULL;
	}

	const Entry* getEntry(int slot) const {
		if(slot < 0) {
			return NULL;
		}

		if(static_cast<unsigned>(slot) < entries_.size()) {
			return &entries_[slot];
		}

		if(base_) {
			return base_->getEntry(slot - NUM_MAP_CALLABLE_SLOTS);
		}

		return NULL;
	}

	int getNumSlots() const {
		return NUM_MAP_CALLABLE_SLOTS + (base_ ? base_->getNumSlots() : 0);
	}

private:
	ConstFormulaCallableDefinitionPtr base_;
	variant_type_ptr key_type_, value_type_;

	std::vector<Entry> entries_;
};

class map_callable : public FormulaCallable {
	public:
		explicit map_callable(const FormulaCallable& backup)
		: backup_(&backup)
		{}

		void setValue_name(const std::string& name) { value_name_ = name; }

		void set(const variant& v, int i)
		{
			value_ = v;
			index_ = i;
		}

		void set(const variant& k, const variant& v, int i)
		{
			key_ = k;
			value_ = v;
			index_ = i;
		}
	private:
		variant getValue(const std::string& key) const {
			if(value_name_.empty() && key == "value" ||
			   !value_name_.empty() && key == value_name_) {
				return value_;
			} else if(key == "index") {
				return variant(index_);
			} else if(key == "context") {
				return variant(backup_.get());
			} else if(key == "key") {
				return key_;
			} else {
				return backup_->queryValue(key);
			}
		}

		variant getValueBySlot(int slot) const {
			ASSERT_LOG(slot >= 0, "BAD SLOT VALUE: " << slot);
			if(slot < NUM_MAP_CALLABLE_SLOTS) {
				switch(slot) {
					case MAP_CALLABLE_VALUE: return value_;
					case MAP_CALLABLE_INDEX: return variant(index_);
					case MAP_CALLABLE_CONTEXT: return variant(backup_.get());
					case MAP_CALLABLE_KEY: return key_;
					default: ASSERT_LOG(false, "BAD GET VALUE BY SLOT");
				}
			} else if(backup_) {
				return backup_->queryValueBySlot(slot - NUM_MAP_CALLABLE_SLOTS);
			} else {
				ASSERT_LOG(false, "COULD NOT FIND VALUE FOR SLOT: " << slot);
			}
		}

		const ConstFormulaCallablePtr backup_;
		variant key_;
		variant value_;
		int index_;

		std::string value_name_;
};

FUNCTION_DEF(count, 2, 2, "count(list, expr): Returns an integer count of how many items in the list 'expr' returns true for.")
	const variant items = split_variant_if_str(args()[0]->evaluate(variables));
	if(items.is_map()) {
		int res = 0;
		boost::intrusive_ptr<map_callable> callable(new map_callable(variables));
		int index = 0;
		for(const auto& p : items.as_map()) {
			callable->set(p.first, p.second, index);
			const variant val = args().back()->evaluate(*callable);
			if(val.as_bool()) {
				++res;
			}

			++index;
		}

		return variant(res);
	} else {
		int res = 0;
		boost::intrusive_ptr<map_callable> callable(new map_callable(variables));
		for(size_t n = 0; n != items.num_elements(); ++n) {
			callable->set(items[n], n);
			const variant val = args().back()->evaluate(*callable);
			if(val.as_bool()) {
				++res;
			}
		}

		return variant(res);
	}

FUNCTION_ARGS_DEF
	ARG_TYPE("list|map");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_INT);
END_FUNCTION_DEF(count)

class filter_function : public FunctionExpression {
public:
	explicit filter_function(const args_list& args)
		: FunctionExpression("filter", args, 2, 3)
	{
		if(args.size() == 3) {
			identifier_ = read_identifier_expression(*args[1]);
		}
	}
private:
	std::string identifier_;
	variant execute(const FormulaCallable& variables) const {
		std::vector<variant> vars;
		const variant items = args()[0]->evaluate(variables);
		if(args().size() == 2) {

			if(items.is_map()) {
				boost::intrusive_ptr<map_callable> callable(new map_callable(variables));
				std::map<variant,variant> m;
				int index = 0;
				for(const variant_pair& p : items.as_map()) {
					callable->set(p.first, p.second, index);
					const variant val = args().back()->evaluate(*callable);
					if(val.as_bool()) {
						m[p.first] = p.second;
					}

					++index;
				}

				return variant(&m);
			} else {
				boost::intrusive_ptr<map_callable> callable(new map_callable(variables));
				for(size_t n = 0; n != items.num_elements(); ++n) {
					callable->set(items[n], n);
					const variant val = args().back()->evaluate(*callable);
					if(val.as_bool()) {
						vars.push_back(items[n]);
					}
				}
			}
		} else {
			boost::intrusive_ptr<map_callable> callable(new map_callable(variables));
			const std::string self = identifier_.empty() ? args()[1]->evaluate(variables).as_string() : identifier_;
			callable->setValue_name(self);

			for(size_t n = 0; n != items.num_elements(); ++n) {
				callable->set(items[n], n);
				const variant val = args().back()->evaluate(*callable);
				if(val.as_bool()) {
					vars.push_back(items[n]);
				}
			}
		}

		return variant(&vars);
	}

	variant_type_ptr getVariantType() const {
		variant_type_ptr list_type = args()[0]->queryVariantType();
		ConstFormulaCallableDefinitionPtr def = args()[1]->getDefinitionUsedByExpression();
		if(def) {
			def = args()[1]->queryModifiedDefinitionBasedOnResult(true, def);
			if(def) {
				const game_logic::FormulaCallableDefinition::Entry* value_entry = def->getEntryById("value");
				if(value_entry != NULL && value_entry->variant_type && list_type->is_list_of()) {
					return variant_type::get_list(value_entry->variant_type);
				}
			}
		}

		if(list_type->is_list_of()) {
			return variant_type::get_list(list_type->is_list_of());
		} else if(list_type->is_map_of().first) {
			return variant_type::get_map(list_type->is_map_of().first, list_type->is_map_of().second);
		} else {
			std::vector<variant_type_ptr> v;
			v.push_back(variant_type::get_type(variant::VARIANT_TYPE_LIST));
			v.push_back(variant_type::get_type(variant::VARIANT_TYPE_MAP));
			return variant_type::get_union(v);
		}
	}

	void staticErrorAnalysis() const {
		bool found_valid_expr = false;
		std::vector<ConstExpressionPtr> expressions = args().back()->queryChildrenRecursive();
		for(ConstExpressionPtr expr : expressions) {
			const std::string& s = expr->str();
			if(s == "value" || s == "key" || s == "index" || s == identifier_) {
				found_valid_expr = true;
				break;
			}
		}

		ASSERT_LOG(found_valid_expr, "Last argument to filter() function does not contain 'value' or 'index' " << debugPinpointLocation());
	}
};

FUNCTION_DEF(unique, 1, 1, "unique(list): returns unique elements of list")
	std::vector<variant> v = args()[0]->evaluate(variables).as_list();
	std::sort(v.begin(), v.end());
	v.erase(std::unique(v.begin(), v.end()), v.end());
	return variant(&v);
FUNCTION_ARGS_DEF
	ARG_TYPE("list");
FUNCTION_TYPE_DEF
	variant_type_ptr list_type = args()[0]->queryVariantType();
	if(list_type->is_list_of()) {
		return variant_type::get_list(list_type->is_list_of());
	} else {
		return variant_type::get_type(variant::VARIANT_TYPE_LIST);
	}
END_FUNCTION_DEF(unique)
	
FUNCTION_DEF(mapping, -1, -1, "mapping(x): Turns the args passed in into a map. The first arg is a key, the second a value, the third a key, the fourth a value and so on and so forth.")
	MapFormulaCallable* callable = new MapFormulaCallable;
	for(size_t n = 0; n < args().size()-1; n += 2) {
		callable->add(args()[n]->evaluate(variables).as_string(),
					args()[n+1]->evaluate(variables));
	}
	
	return variant(callable);
END_FUNCTION_DEF(mapping)

class find_function : public FunctionExpression {
public:
	explicit find_function(const args_list& args)
		: FunctionExpression("find", args, 2, 3)
	{
		if(args.size() == 3) {
			identifier_ = read_identifier_expression(*args[1]);
		}
	}

private:
	std::string identifier_;
	variant execute(const FormulaCallable& variables) const {
		const variant items = args()[0]->evaluate(variables);

		if(args().size() == 2) {
			boost::intrusive_ptr<map_callable> callable(new map_callable(variables));
			for(size_t n = 0; n != items.num_elements(); ++n) {
				callable->set(items[n], n);
				const variant val = args().back()->evaluate(*callable);
				if(val.as_bool()) {
					return items[n];
				}
			}
		} else {
			boost::intrusive_ptr<map_callable> callable(new map_callable(variables));

			const std::string self = identifier_.empty() ? args()[1]->evaluate(variables).as_string() : identifier_;
			callable->setValue_name(self);

			for(size_t n = 0; n != items.num_elements(); ++n) {
				callable->set(items[n], n);
				const variant val = args().back()->evaluate(*callable);
				if(val.as_bool()) {
					return items[n];
				}
			}
		}

		return variant();
	}

	variant_type_ptr getVariantType() const {
		std::string value_str = "value";
		if(args().size() > 2) {
			variant literal;
			args()[1]->isLiteral(literal);
			if(literal.is_string()) {
				value_str = literal.as_string();
			} else if(args()[1]->isIdentifier(&value_str) == false) {
				ASSERT_LOG(false, "find function requires a literal as its second argument");
			}
		}

		ConstFormulaCallableDefinitionPtr def = args().back()->getDefinitionUsedByExpression();
		if(def) {
			ConstFormulaCallableDefinitionPtr modified = args().back()->queryModifiedDefinitionBasedOnResult(true, def);
			if(modified) {
				def = modified;
			}

			const game_logic::FormulaCallableDefinition::Entry* value_entry = def->getEntryById(value_str);
			if(value_entry != NULL && value_entry->variant_type) {
				std::vector<variant_type_ptr> types;
				types.push_back(variant_type::get_type(variant::VARIANT_TYPE_NULL));
				types.push_back(value_entry->variant_type);
				return variant_type::get_union(types);
			}
		}

		return variant_type::get_any();
	}

	void staticErrorAnalysis() const {
		bool found_valid_expr = false;
		std::vector<ConstExpressionPtr> expressions = args().back()->queryChildrenRecursive();
		for(ConstExpressionPtr expr : expressions) {
			const std::string& s = expr->str();
			if(s == "value" || s == "key" || s == "index" || s == identifier_) {
				found_valid_expr = true;
				break;
			}
		}

		ASSERT_LOG(found_valid_expr, "Last argument to find() function does not contain 'value' or 'index' " << debugPinpointLocation());
	}
};

class find_or_die_function : public FunctionExpression {
public:
	explicit find_or_die_function(const args_list& args)
		: FunctionExpression("find_or_die", args, 2, 3)
	{
		if(args.size() == 3) {
			identifier_ = read_identifier_expression(*args[1]);
		}
	}

private:
	std::string identifier_;
	variant execute(const FormulaCallable& variables) const {
		const variant items = args()[0]->evaluate(variables);

		if(args().size() == 2) {
			boost::intrusive_ptr<map_callable> callable(new map_callable(variables));
			for(size_t n = 0; n != items.num_elements(); ++n) {
				callable->set(items[n], n);
				const variant val = args().back()->evaluate(*callable);
				if(val.as_bool()) {
					return items[n];
				}
			}
		} else {
			boost::intrusive_ptr<map_callable> callable(new map_callable(variables));

			const std::string self = identifier_.empty() ? args()[1]->evaluate(variables).as_string() : identifier_;
			callable->setValue_name(self);

			for(size_t n = 0; n != items.num_elements(); ++n) {
				callable->set(items[n], n);
				const variant val = args().back()->evaluate(*callable);
				if(val.as_bool()) {
					return items[n];
				}
			}
		}

		ASSERT_LOG(false, "Failed to find expected item. List has: " << items.write_json());
	}

	variant_type_ptr getVariantType() const {
		ConstFormulaCallableDefinitionPtr def = args()[1]->getDefinitionUsedByExpression();
		if(def) {
			ConstFormulaCallableDefinitionPtr modified = args()[1]->queryModifiedDefinitionBasedOnResult(true, def);
			if(modified) {
				def = modified;
			}

			const game_logic::FormulaCallableDefinition::Entry* value_entry = def->getEntryById("value");
			if(value_entry != NULL && value_entry->variant_type) {
				return value_entry->variant_type;
			}
		}

		return variant_type::get_any();
	}

	void staticErrorAnalysis() const {
		bool found_valid_expr = false;
		std::vector<ConstExpressionPtr> expressions = args().back()->queryChildrenRecursive();
		for(ConstExpressionPtr expr : expressions) {
			const std::string& s = expr->str();
			if(s == "value" || s == "key" || s == "index" || s == identifier_) {
				found_valid_expr = true;
				break;
			}
		}

		ASSERT_LOG(found_valid_expr, "Last argument to find_or_die() function does not contain 'value' or 'index' " << debugPinpointLocation());
	}
};

	class transform_callable : public FormulaCallable {
	public:
		explicit transform_callable(const FormulaCallable& backup)
		: backup_(backup)
		{}

		void set(const variant& v, const variant& i)
		{
			value_ = v;
			index_ = i;
		}
	private:
		variant getValue(const std::string& key) const {
			if(key == "v") {
				return value_;
			} else if(key == "i") {
				return index_;
			} else {
				return backup_.queryValue(key);
			}
		}

		variant getValueBySlot(int slot) const {
			return backup_.queryValueBySlot(slot);
		}

		const FormulaCallable& backup_;
		variant value_, index_;
	};

FUNCTION_DEF(transform, 2, 2, "transform(list,ffl): calls the ffl for each item on the given list, returning a list of the results. Inside the transform v is the value of the list item and i is the index. e.g. transform([1,2,3], v+2) = [3,4,5] and transform([1,2,3], i) = [0,1,2]")
	std::vector<variant> vars;
	const variant items = args()[0]->evaluate(variables);

	vars.reserve(items.num_elements());

	transform_callable* callable = new transform_callable(variables);
	variant v(callable);

	const int nitems = items.num_elements();
	for(size_t n = 0; n != nitems; ++n) {
		callable->set(items[n], variant(unsigned(n)));
		const variant val = args().back()->evaluate(*callable);
		vars.push_back(val);
	}

	return variant(&vars);
END_FUNCTION_DEF(transform)

namespace {
void visit_objects(variant v, std::vector<variant>& res) {
	if(v.is_map()) {
		res.push_back(v);
		for(const variant_pair& value : v.as_map()) {
			visit_objects(value.second, res);
		}
	} else if(v.is_list()) {
		for(const variant& value : v.as_list()) {
			visit_objects(value, res);
		}
	} else if(v.try_convert<variant_callable>()) {
		res.push_back(v);
		variant keys = v.try_convert<variant_callable>()->getValue().getKeys();
		for(variant k : keys.as_list()) {
			visit_objects(v.try_convert<variant_callable>()->queryValue(k.as_string()), res);
		}
	}
}
}

class visit_objects_function : public FunctionExpression 
{
public:
	explicit visit_objects_function(const args_list& args)
		: FunctionExpression("visit_objects", args, 1, 1)
	{}
private:
	variant execute(const FormulaCallable& variables) const {
		const variant v = args()[0]->evaluate(variables);
		std::vector<variant> result;
		visit_objects(v, result);
		return variant(&result);
	}
};

FUNCTION_DEF(choose, 1, 2, "choose(list, (optional)scoring_expr) -> value: choose an item from the list according to which scores the highest according to the scoring expression, or at random by default.")

	if(args().size() == 1) {
		Formula::failIfStaticContext();
	}

	const variant items = args()[0]->evaluate(variables);
	if(items.num_elements() == 0) {
		return variant();
	}

	int max_index = -1;
	variant max_value;
	boost::intrusive_ptr<map_callable> callable(new map_callable(variables));
	for(size_t n = 0; n != items.num_elements(); ++n) {
		variant val;
		
		if(args().size() >= 2) {
			callable->set(items[n], n);
			val = args().back()->evaluate(*callable);
		} else {
			val = variant(rand());
		}

		if(n == 0 || val > max_value) {
			max_index = n;
			max_value = val;
		}
	}

	return items[max_index];
FUNCTION_ARGS_DEF
	ARG_TYPE("list");
FUNCTION_TYPE_DEF
	return args()[0]->queryVariantType()->is_list_of();
END_FUNCTION_DEF(choose)

class map_function : public FunctionExpression {
public:
	explicit map_function(const args_list& args)
		: FunctionExpression("map", args, 2, 3)
	{
		if(args.size() == 3) {
			identifier_ = read_identifier_expression(*args[1]);
		}
	}
private:
	std::string identifier_;

	variant execute(const FormulaCallable& variables) const {
		std::vector<variant> vars;
		const variant items = args()[0]->evaluate(variables);

		vars.reserve(items.num_elements());

		if(args().size() == 2) {

			if(items.is_map()) {
				boost::intrusive_ptr<map_callable> callable(new map_callable(variables));
				int index = 0;
				for(const variant_pair& p : items.as_map()) {
					callable->set(p.first, p.second, index);
					const variant val = args().back()->evaluate(*callable);
					vars.push_back(val);
					++index;
				}
			} else if(items.is_string()) {
				const std::string& s = items.as_string();
				boost::intrusive_ptr<map_callable> callable(new map_callable(variables));
				for(size_t n = 0; n != s.length(); ++n) {
					variant v(s.substr(n,1));
					callable->set(v, n);
					const variant val = args().back()->evaluate(*callable);
					vars.push_back(val);
				}
			} else {
				boost::intrusive_ptr<map_callable> callable(new map_callable(variables));
				for(size_t n = 0; n != items.num_elements(); ++n) {
					callable->set(items[n], n);
					const variant val = args().back()->evaluate(*callable);
					vars.push_back(val);
				}
			}
		} else {
			boost::intrusive_ptr<map_callable> callable(new map_callable(variables));
			const std::string self = identifier_.empty() ? args()[1]->evaluate(variables).as_string() : identifier_;
			callable->setValue_name(self);
			for(size_t n = 0; n != items.num_elements(); ++n) {
				callable->set(items[n], n);
				const variant val = args().back()->evaluate(*callable);
				vars.push_back(val);
			}
		}

		return variant(&vars);
	}

	variant_type_ptr getVariantType() const {
		variant_type_ptr spec_type = args()[0]->queryVariantType();
		if(spec_type->is_specific_list()) {
			std::vector<variant_type_ptr> types;
			variant_type_ptr type = args().back()->queryVariantType();
			const int num_items = spec_type->is_specific_list()->size();
			for(int n = 0; n != num_items; ++n) {
				types.push_back(type);
			}

			return variant_type::get_specific_list(types);
		}

		return variant_type::get_list(args().back()->queryVariantType());
	}

};

FUNCTION_DEF(sum, 1, 2, "sum(list[, counter]): Adds all elements of the list together. If counter is supplied, all elements of the list are added to the counter instead of to 0.")
	variant res(0);
	const variant items = args()[0]->evaluate(variables);
	if(args().size() >= 2) {
		res = args()[1]->evaluate(variables);
	}
	for(size_t n = 0; n != items.num_elements(); ++n) {
		res = res + items[n];
	}

	return res;

FUNCTION_ARGS_DEF
	ARG_TYPE("list");
FUNCTION_TYPE_DEF
	std::vector<variant_type_ptr> types;
	types.push_back(args()[0]->queryVariantType()->is_list_of());
	if(args().size() > 1) {
		types.push_back(args()[1]->queryVariantType());
	}

	return variant_type::get_union(types);

END_FUNCTION_DEF(sum)

FUNCTION_DEF(range, 1, 3, "range([start, ]finish[, step]): Returns a list containing all numbers smaller than the finish value and and larger than or equal to the start value. The start value defaults to 0.")
	int start = args().size() > 1 ? args()[0]->evaluate(variables).as_int() : 0;
	int end = args()[args().size() > 1 ? 1 : 0]->evaluate(variables).as_int();
	int step = args().size() < 3 ? 1 : args()[2]->evaluate(variables).as_int();
	ASSERT_LOG(step > 0, "ILLEGAL STEP VALUE IN RANGE: " << step);
	bool reverse = false;
	if(end < start) {
		std::swap(start, end);
		++start;
		++end;
		reverse = true;
	}
	const int nelem = end - start;

	std::vector<variant> v;

	if(nelem > 0) {
		v.reserve(nelem/step);

		for(int n = 0; n < nelem; n += step) {
			v.push_back(variant(start+n));
		}
	}

	if(reverse) {
		std::reverse(v.begin(), v.end());
	}

	return variant(&v);
FUNCTION_TYPE_DEF
	return variant_type::get_list(variant_type::get_type(variant::VARIANT_TYPE_INT));
END_FUNCTION_DEF(range)

FUNCTION_DEF(reverse, 1, 1, "reverse(list): reverses the given list")
	std::vector<variant> items = args()[0]->evaluate(variables).as_list();
	std::reverse(items.begin(), items.end());
	return variant(&items);
FUNCTION_ARGS_DEF
	ARG_TYPE("list");
FUNCTION_TYPE_DEF
	variant_type_ptr list_type = args()[0]->queryVariantType();
	if(list_type->is_list_of()) {
		return variant_type::get_list(list_type->is_list_of());
	} else {
		return variant_type::get_list(variant_type::get_any());
	}
END_FUNCTION_DEF(reverse)

FUNCTION_DEF(head, 1, 1, "head(list): gives the first element of a list, or null for an empty list")
	const variant items = args()[0]->evaluate(variables);
	if(items.num_elements() >= 1) {
		return items[0];
	} else {
		return variant();
	}
FUNCTION_ARGS_DEF
	ARG_TYPE("list");
FUNCTION_TYPE_DEF
	std::vector<variant_type_ptr> types;
	types.push_back(variant_type::get_type(variant::VARIANT_TYPE_NULL));
	types.push_back(args()[0]->queryVariantType()->is_list_of());
	return variant_type::get_union(types);
END_FUNCTION_DEF(head)

FUNCTION_DEF(head_or_die, 1, 1, "head_or_die(list): gives the first element of a list, or die for an empty list")
	const variant items = args()[0]->evaluate(variables);
	ASSERT_LOG(items.num_elements() >= 1, "head_or_die() called on empty list");
	return items[0];
FUNCTION_ARGS_DEF
	ARG_TYPE("list");
FUNCTION_TYPE_DEF
	return args()[0]->queryVariantType()->is_list_of();
END_FUNCTION_DEF(head_or_die)

FUNCTION_DEF(back, 1, 1, "back(list): gives the last element of a list, or null for an empty list")
	const variant items = args()[0]->evaluate(variables);
	if(items.num_elements() >= 1) {
		return items[items.num_elements()-1];
	} else {
		return variant();
	}
FUNCTION_ARGS_DEF
	ARG_TYPE("list");
FUNCTION_TYPE_DEF
	std::vector<variant_type_ptr> types;
	types.push_back(variant_type::get_type(variant::VARIANT_TYPE_NULL));
	types.push_back(args()[0]->queryVariantType()->is_list_of());
	return variant_type::get_union(types);
END_FUNCTION_DEF(back)

FUNCTION_DEF(back_or_die, 1, 1, "back_or_die(list): gives the last element of a list, or die for an empty list")
	const variant items = args()[0]->evaluate(variables);
	ASSERT_LOG(items.num_elements() >= 1, "back_or_die() called on empty list");
	return items[items.num_elements()-1];
FUNCTION_ARGS_DEF
	ARG_TYPE("list");
FUNCTION_TYPE_DEF
	return args()[0]->queryVariantType()->is_list_of();
END_FUNCTION_DEF(back_or_die)

FUNCTION_DEF(get_all_files_under_dir, 1, 1, "get_all_files_under_dir(path): Returns a list of all the files in and under the given directory")
	std::vector<variant> v;
	std::map<std::string, std::string> file_paths;
	module::get_unique_filenames_under_dir(args()[0]->evaluate(variables).as_string(), &file_paths);
	for(std::map<std::string, std::string>::const_iterator i = file_paths.begin(); i != file_paths.end(); ++i) {
		v.push_back(variant(i->second));
	}
	return variant(&v);
FUNCTION_ARGS_DEF
	ARG_TYPE("string");
FUNCTION_TYPE_DEF
	return variant_type::get_list(variant_type::get_type(variant::VARIANT_TYPE_STRING));
END_FUNCTION_DEF(get_all_files_under_dir)

FUNCTION_DEF(get_files_in_dir, 1, 1, "get_files_in_dir(path): Returns a list of the files in the given directory")
	std::vector<variant> v;
	std::vector<std::string> files;
	std::string dirname = args()[0]->evaluate(variables).as_string();
	if(dirname[dirname.size()-1] != '/') {
		dirname += '/';
	}
	module::get_files_in_dir(dirname, &files);
	for(std::vector<std::string>::const_iterator i = files.begin(); i != files.end(); ++i) {
		v.push_back(variant(*i));
	}
	return variant(&v);
FUNCTION_ARGS_DEF
	ARG_TYPE("string");
FUNCTION_TYPE_DEF
	return variant_type::get_list(variant_type::get_type(variant::VARIANT_TYPE_STRING));
END_FUNCTION_DEF(get_files_in_dir)

FUNCTION_DEF(dialog, 2, 2, "dialog(obj, template): Creates a dialog given an object to operate on and a template for the dialog.")
	bool modal = args().size() == 3 && args()[2]->evaluate(variables).as_bool(); 
	variant environment = args()[0]->evaluate(variables);
	variant dlg_template = args()[1]->evaluate(variables);
	FormulaCallable* e = environment.try_convert<FormulaCallable>();
	variant v;
	if(dlg_template.is_string()) {
		std::string s = dlg_template.as_string();
		if(s.length() <= 4 || s.substr(s.length()-4) != ".cfg") {
			s += ".cfg";
		}
		v = json::parse_from_file(gui::get_dialog_file(s));
	} else {
		v = dlg_template;
	}
	return variant(new gui::Dialog(v, e));
END_FUNCTION_DEF(dialog)

FUNCTION_DEF(show_modal, 1, 1, "show_modal(dialog): Displays a modal dialog on the screen.")
	variant graph = args()[0]->evaluate(variables);
	gui::DialogPtr dialog = boost::intrusive_ptr<gui::Dialog>(graph.try_convert<gui::Dialog>());
	ASSERT_LOG(dialog, "Dialog given is not of the correct type.");
	dialog->showModal();
	return variant::from_bool(dialog->cancelled() == false);
END_FUNCTION_DEF(show_modal)

FUNCTION_DEF(index, 2, 2, "index(list, value) -> index of value in list: Returns the index of the value in the list or -1 if value wasn't found in the list.")
	variant value = args()[1]->evaluate(variables);
	variant li = args()[0]->evaluate(variables);
	for(unsigned n = 0; n < li.num_elements(); n++) {
		if(value == li[n]) {
			return variant(n);
		}
	}
	return variant(-1);
FUNCTION_ARGS_DEF
	ARG_TYPE("list");
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_INT);
END_FUNCTION_DEF(index)

namespace 
{
	void evaluate_expr_for_benchmark(const FormulaExpression* expr, const FormulaCallable* variables, int ntimes)
	{
		for(int n = 0; n < ntimes; ++n) {
			expr->evaluate(*variables);
		}
	}
}

FUNCTION_DEF(benchmark, 1, 1, "benchmark(expr): Executes expr in a benchmark harness and returns a string describing its benchmark performance")
	using std::placeholders::_1;
	return variant(test::run_benchmark("benchmark", std::bind(evaluate_expr_for_benchmark, args()[0].get(), &variables, _1)));
END_FUNCTION_DEF(benchmark)

FUNCTION_DEF(compress, 1, 2, "compress(string, (optional) compression_level): Compress the given string object")
	int compression_level = -1;
	if(args().size() > 1) {
		compression_level = args()[1]->evaluate(variables).as_int();
	}
	const std::string s = args()[0]->evaluate(variables).as_string();
	return variant(new zip::CompressedData(std::vector<char>(s.begin(), s.end()), compression_level));
FUNCTION_ARGS_DEF
	ARG_TYPE("string");
END_FUNCTION_DEF(compress)

	class size_function : public FunctionExpression {
	public:
		explicit size_function(const args_list& args)
			: FunctionExpression("size", args, 1, 1)
		{}
	private:
		variant execute(const FormulaCallable& variables) const {
			const variant items = args()[0]->evaluate(variables);
			if(items.is_string()) {
				return variant(items.as_string().size());
			}
			return variant(static_cast<int>(items.num_elements()));
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		}
	};

	
	class split_function : public FunctionExpression {
	public:
		explicit split_function(const args_list& args)
		: FunctionExpression("split", args, 1, 2)
		{}
	private:
		variant execute(const FormulaCallable& variables) const {
			std::vector<std::string> chopped;
			if(args().size() >= 2) {
				const std::string thestring = args()[0]->evaluate(variables).as_string();
				const std::string delimiter = args()[1]->evaluate(variables).as_string();
				chopped = util::split(thestring, delimiter);
			} else {
				const std::string thestring = args()[0]->evaluate(variables).as_string();
				chopped = util::split(thestring);
			}
		
			std::vector<variant> res;
			for(size_t i=0; i<chopped.size(); ++i) {
				const std::string& part = chopped[i];
				res.push_back(variant(part));
			}
			
			return variant(&res);
			
		}

		variant_type_ptr getVariantType() const {
			return variant_type::get_list(args()[0]->queryVariantType());
		}
	};

	class split_any_of_function : public FunctionExpression {
	public:
		explicit split_any_of_function(const args_list& args)
		: FunctionExpression("split_any_of", args, 2, 2)
		{}
	private:
		variant execute(const FormulaCallable& variables) const {
			std::vector<std::string> chopped;
			const std::string thestring = args()[0]->evaluate(variables).as_string();
			const std::string delimiters = args()[1]->evaluate(variables).as_string();
			boost::split(chopped, thestring, boost::is_any_of(delimiters));
		
			std::vector<variant> res;
			for(auto it : chopped) {
				res.push_back(variant(it));
			}
			return variant(&res);
		}

		variant_type_ptr getVariantType() const {
			return variant_type::get_list(args()[0]->queryVariantType());
		}
	};

	class slice_function : public FunctionExpression {
	public:
		explicit slice_function(const args_list& args)
			: FunctionExpression("slice", args, 3, 3)
		{}
	private:
		variant execute(const FormulaCallable& variables) const {
			const variant list = args()[0]->evaluate(variables);
			if(list.num_elements() == 0) {
				return variant();
			}
			int begin_index = args()[1]->evaluate(variables).as_int()%(list.num_elements()+1);
			int end_index = args()[2]->evaluate(variables).as_int()%(list.num_elements()+1);
			if(end_index >= begin_index) {
				std::vector<variant> result;
				result.reserve(end_index - begin_index);
				while(begin_index != end_index) {
					result.push_back(list[begin_index++]);
				}

				return variant(&result);
			} else {
				return variant();
			}
		}
	};

	class str_function : public FunctionExpression {
	public:
		explicit str_function(const args_list& args)
			: FunctionExpression("str", args, 1, 1)
		{}
	private:
		variant execute(const FormulaCallable& variables) const {
			const variant item = args()[0]->evaluate(variables);
			if(item.is_string()) {
				//just return as-is for something that's already a string.
				return item;
			}

			std::string str;
			item.serializeToString(str);
			return variant(str);
		}

		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_STRING);
		}
	};

	class strstr_function : public FunctionExpression {
	public:
		explicit strstr_function(const args_list& args)
		: FunctionExpression("strstr", args, 2, 2)
		{}
	private:
		variant execute(const FormulaCallable& variables) const {
			const std::string haystack = args()[0]->evaluate(variables).as_string();
			const std::string needle = args()[1]->evaluate(variables).as_string();

			const size_t pos = haystack.find(needle);

			if(pos == std::string::npos) {
				return variant(0);
			} else {
				return variant(pos + 1);
			}
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		}
	};

	class null_function : public FunctionExpression {
	public:
		explicit null_function(const args_list& args)
			: FunctionExpression("null", args, 0, 0)
		{}
	private:
		variant execute(const FormulaCallable& /*variables*/) const {
			return variant();
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_NULL);
		}
	};

	class refcount_function : public FunctionExpression {
	public:
		explicit refcount_function(const args_list& args)
			: FunctionExpression("refcount", args, 1, 1)
		{}
	private:
		variant execute(const FormulaCallable& variables) const {
			return variant(args()[0]->evaluate(variables).refcount());
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		}
	};

	class deserialize_function : public FunctionExpression {
	public:
		explicit deserialize_function(const args_list& args)
		: FunctionExpression("deserialize", args, 1, 1)
		{}

	private:
		variant execute(const FormulaCallable& variables) const {
			Formula::failIfStaticContext();	
			const intptr_t id = static_cast<intptr_t>(std::stoll(args()[0]->evaluate(variables).as_string().c_str(), NULL, 16));
			return variant::create_variant_under_construction(id);
		}
	};

	class is_string_function : public FunctionExpression {
	public:
		explicit is_string_function(const args_list& args)
			: FunctionExpression("is_string", args, 1, 1)
		{}

	private:
		variant execute(const FormulaCallable& variables) const {
			return variant::from_bool(args()[0]->evaluate(variables).is_string());
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
		}
	};

	class is_null_function : public FunctionExpression {
	public:
		explicit is_null_function(const args_list& args)
			: FunctionExpression("is_null", args, 1, 1)
		{}

	private:
		variant execute(const FormulaCallable& variables) const {
			return variant::from_bool(args()[0]->evaluate(variables).is_null());
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
		}
	};

	class is_int_function : public FunctionExpression {
	public:
		explicit is_int_function(const args_list& args)
			: FunctionExpression("is_int", args, 1, 1)
		{}

	private:
		variant execute(const FormulaCallable& variables) const {
			return variant::from_bool(args()[0]->evaluate(variables).is_int());
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
		}
	};

	class is_bool_function : public FunctionExpression {
	public:
		explicit is_bool_function(const args_list& args)
			: FunctionExpression("is_bool", args, 1, 1)
		{}

	private:
		variant execute(const FormulaCallable& variables) const {
			return variant::from_bool(args()[0]->evaluate(variables).is_bool());
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
		}
	};

	class is_decimal_function : public FunctionExpression {
	public:
		explicit is_decimal_function(const args_list& args)
			: FunctionExpression("is_decimal", args, 1, 1)
		{}

	private:
		variant execute(const FormulaCallable& variables) const {
			return variant::from_bool(args()[0]->evaluate(variables).is_decimal());
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
		}
	};

	class is_number_function : public FunctionExpression { //Sometimes, you just want to make sure a thing is numeric, sod the semantics.
	public:
		explicit is_number_function(const args_list& args)
			: FunctionExpression("is_number", args, 1, 1)
		{}

	private:
		variant execute(const FormulaCallable& variables) const {
			return variant::from_bool(args()[0]->evaluate(variables).is_decimal() || args()[0]->evaluate(variables).is_int());
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
		}
	};

	class is_map_function : public FunctionExpression {
	public:
		explicit is_map_function(const args_list& args)
			: FunctionExpression("is_map", args, 1, 1)
		{}

	private:
		variant execute(const FormulaCallable& variables) const {
			return variant::from_bool(args()[0]->evaluate(variables).is_map());
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
		}
	};

	class mod_function : public FunctionExpression {
		//the standard C++ mod expression does not give correct answers for negative operands - it's "implementation-defined", which means it's not really a modulo operation the way math normally describes them.  To get the right answer, we're using the following - based on the fact that x%y is always in the range [-y+1, y-1], and thus adding y to it is both always enough to make it positive, but doesn't change the modulo value.
	public:
		explicit mod_function(const args_list& args)
		: FunctionExpression("mod", args, 2, 2)
		{}
		
	private:
		variant execute(const FormulaCallable& variables) const {
			int left = args()[0]->evaluate(variables).as_int();
			int right = args()[1]->evaluate(variables).as_int();
			
			return variant((left%right + right)%right);
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		}
	};
	
	class is_function_function : public FunctionExpression {
	public:
		explicit is_function_function(const args_list& args)
			: FunctionExpression("is_function", args, 1, 1)
		{}

	private:
		variant execute(const FormulaCallable& variables) const {
			return variant::from_bool(args()[0]->evaluate(variables).is_function());
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
		}
	};

	class is_list_function : public FunctionExpression {
	public:
		explicit is_list_function(const args_list& args)
			: FunctionExpression("is_list", args, 1, 1)
		{}

	private:
		variant execute(const FormulaCallable& variables) const {
			return variant::from_bool(args()[0]->evaluate(variables).is_list());
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
		}
	};

	class is_callable_function : public FunctionExpression {
	public:
		explicit is_callable_function(const args_list& args)
			: FunctionExpression("is_callable", args, 1, 1)
		{}

	private:
		variant execute(const FormulaCallable& variables) const {
			return variant::from_bool(args()[0]->evaluate(variables).is_callable());
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
		}
	};

	class list_str_function : public FunctionExpression {
	public:
		explicit list_str_function(const args_list& args)
			: FunctionExpression("list_str", args, 1, 1)
		{}

	private:
		variant execute(const FormulaCallable& variables) const {
			const std::string str = args()[0]->evaluate(variables).as_string();
			std::vector<variant> result;
			
			int count = 0;
			while (str[count] != 0) {
				std::string chr(1,str[count]);
				result.push_back(variant(chr));
				count++;
			}
			return variant(&result);
		}
		variant_type_ptr getVariantType() const {
			return variant_type::get_list(variant_type::get_type(variant::VARIANT_TYPE_STRING));
		}
	};

class set_command : public game_logic::CommandCallable
{
public:
	set_command(variant target, const std::string& attr, const variant& variant_attr, variant val)
	  : target_(target), attr_(attr), variant_attr_(variant_attr), val_(val)
	{}
	virtual void execute(game_logic::FormulaCallable& ob) const {
		if(target_.is_callable()) {
			ASSERT_LOG(!attr_.empty(), "ILLEGAL KEY IN SET OF CALLABLE: " << val_.write_json());
			target_.mutable_callable()->mutateValue(attr_, val_);
		} else if(target_.is_map()) {
			if(!attr_.empty()) {
				target_.add_attr_mutation(variant(attr_), val_);
			} else {
				target_.add_attr_mutation(variant_attr_, val_);
			}
		} else {
			ASSERT_LOG(!attr_.empty(), "ILLEGAL KEY IN SET OF CALLABLE: " << val_.write_json());
			ob.mutateValue(attr_, val_);
		}
	}
private:
	mutable variant target_;
	std::string attr_;
	variant variant_attr_;
	variant val_;
};

class add_command : public game_logic::CommandCallable
{
public:
	add_command(variant target, const std::string& attr, const variant& variant_attr, variant val)
	  : target_(target), attr_(attr), variant_attr_(variant_attr), val_(val)
	{}
	virtual void execute(game_logic::FormulaCallable& ob) const {
		if(target_.is_callable()) {
			ASSERT_LOG(!attr_.empty(), "ILLEGAL KEY IN ADD OF CALLABLE: " << val_.write_json());
			target_.mutable_callable()->mutateValue(attr_, target_.mutable_callable()->queryValue(attr_) + val_);
		} else if(target_.is_map()) {
			if(!attr_.empty()) {
				variant key(attr_);
				target_.add_attr_mutation(key, target_[key] + val_);
			} else {
				target_.add_attr_mutation(variant_attr_, target_[variant_attr_] + val_);
			}
		} else {
			ASSERT_LOG(!attr_.empty(), "ILLEGAL KEY IN ADD OF CALLABLE: " << val_.write_json());
			ob.mutateValue(attr_, ob.queryValue(attr_) + val_);
		}
	}
private:
	mutable variant target_;
	std::string attr_;
	variant variant_attr_;
	variant val_;
};

class set_by_slot_command : public game_logic::CommandCallable
{
public:
	set_by_slot_command(int slot, const variant& value)
	  : slot_(slot), value_(value)
	{}

	virtual void execute(game_logic::FormulaCallable& obj) const {
		obj.mutateValueBySlot(slot_, value_);
	}

	void setValue(const variant& value) { value_ = value; }

private:
	int slot_;
	variant value_;
};

class set_target_by_slot_command : public game_logic::CommandCallable
{
public:
	set_target_by_slot_command(variant target, int slot, const variant& value)
	  : target_(target.mutable_callable()), slot_(slot), value_(value)
	{
		ASSERT_LOG(target_.get(), "target of set is not a callable");
	}

	virtual void execute(game_logic::FormulaCallable& obj) const {
		target_->mutateValueBySlot(slot_, value_);
	}

	void setValue(const variant& value) { value_ = value; }

private:
	game_logic::FormulaCallablePtr target_;
	int slot_;
	variant value_;
};

class add_target_by_slot_command : public game_logic::CommandCallable
{
public:
	add_target_by_slot_command(variant target, int slot, const variant& value)
	  : target_(target.mutable_callable()), slot_(slot), value_(value)
	{
		ASSERT_LOG(target_.get(), "target of set is not a callable");
	}

	virtual void execute(game_logic::FormulaCallable& obj) const {
		target_->mutateValueBySlot(slot_, target_->queryValueBySlot(slot_) + value_);
	}

	void setValue(const variant& value) { value_ = value; }

private:
	game_logic::FormulaCallablePtr target_;
	int slot_;
	variant value_;
};

class add_by_slot_command : public game_logic::CommandCallable
{
public:
	add_by_slot_command(int slot, const variant& value)
	  : slot_(slot), value_(value)
	{}

	virtual void execute(game_logic::FormulaCallable& obj) const {
		obj.mutateValueBySlot(slot_, obj.queryValueBySlot(slot_) + value_);
	}

	void setValue(const variant& value) { value_ = value; }

private:
	int slot_;
	variant value_;
};

class set_function : public FunctionExpression {
public:
	set_function(const args_list& args, ConstFormulaCallableDefinitionPtr callable_def)
	  : FunctionExpression("set", args, 2, 3), me_slot_(-1), slot_(-1) {
		if(args.size() == 2) {
			variant literal;
			args[0]->isLiteral(literal);
			if(literal.is_string()) {
				key_ = literal.as_string();
			} else {
				args[0]->isIdentifier(&key_);
			}

			if(!key_.empty() && callable_def) {
				me_slot_ = callable_def->getSlot("me");
				if(me_slot_ != -1 && callable_def->getEntry(me_slot_)->type_definition) {
					slot_ = callable_def->getEntry(me_slot_)->type_definition->getSlot(key_);
				} else {
					slot_ = callable_def->getSlot(key_);
					if(slot_ != -1) {
						cmd_ = boost::intrusive_ptr<set_by_slot_command>(new set_by_slot_command(slot_, variant()));
					}
				}
			}

		}
	}
private:
	variant execute(const FormulaCallable& variables) const {
		if(me_slot_ != -1) {
			variant target = variables.queryValueBySlot(me_slot_);
			if(slot_ != -1) {
				return variant(new set_target_by_slot_command(target, slot_, args()[1]->evaluate(variables)));
			} else if(!key_.empty()) {
				return variant(new set_command(target, key_, variant(), args()[1]->evaluate(variables)));
			}
		} else if(slot_ != -1) {
			if(cmd_->refcount() == 1) {
				cmd_->setValue(args()[1]->evaluate(variables));
				cmd_->setExpression(this);
				return variant(cmd_.get());
			}

			cmd_ = boost::intrusive_ptr<set_by_slot_command>(new set_by_slot_command(slot_, args()[1]->evaluate(variables)));
			cmd_->setExpression(this);
			return variant(cmd_.get());
		}

		if(!key_.empty()) {
			static const std::string MeKey = "me";
			variant target = variables.queryValue(MeKey);
			set_command* cmd = new set_command(target, key_, variant(), args()[1]->evaluate(variables));
			cmd->setExpression(this);
			return variant(cmd);
		}

		if(args().size() == 2) {
			std::string member;
			variant variant_member;
			variant target = args()[0]->evaluateWithMember(variables, member, &variant_member);
			set_command* cmd = new set_command(
			  target, member, variant_member, args()[1]->evaluate(variables));
			cmd->setExpression(this);
			return variant(cmd);
		}

		variant target;
		if(args().size() == 3) {
			target = args()[0]->evaluate(variables);
		}
		const int begin_index = args().size() == 2 ? 0 : 1;
		set_command* cmd = new set_command(
		    target,
		    args()[begin_index]->evaluate(variables).as_string(), variant(),
			args()[begin_index + 1]->evaluate(variables));
		cmd->setExpression(this);
		return variant(cmd);
	}

	variant_type_ptr getVariantType() const {
		return variant_type::get_commands();
	}

	void staticErrorAnalysis() const {
		variant_type_ptr target_type = args()[0]->queryMutableType();
		if(!target_type) {
			ASSERT_LOG(false, "Writing to non-writeable value: " << args()[0]->queryVariantType()->to_string() << " in " << str() << " " << debugPinpointLocation() << "\n");
			return;
		}

		if(!variant_types_compatible(target_type, args()[1]->queryVariantType())) {
			ASSERT_LOG(false, "Writing to value with invalid type " << target_type->to_string() << " <- " << args()[1]->queryVariantType()->to_string() << " in " << str() << " " << debugPinpointLocation() << "\n");
		}
	}

	std::string key_;
	int me_slot_, slot_;
	mutable boost::intrusive_ptr<set_by_slot_command> cmd_;
};

class add_function : public FunctionExpression {
public:
	add_function(const args_list& args, ConstFormulaCallableDefinitionPtr callable_def)
	  : FunctionExpression("add", args, 2, 3), me_slot_(-1), slot_(-1) {
		if(args.size() == 2) {
			variant literal;
			args[0]->isLiteral(literal);
			if(literal.is_string()) {
				key_ = literal.as_string();
			} else {
				args[0]->isIdentifier(&key_);
			}

			if(!key_.empty() && callable_def) {
				me_slot_ = callable_def->getSlot("me");
				if(me_slot_ != -1 && callable_def->getEntry(me_slot_)->type_definition) {
					slot_ = callable_def->getEntry(me_slot_)->type_definition->getSlot(key_);
				} else {
					slot_ = callable_def->getSlot(key_);
					if(slot_ != -1) {
						cmd_ = boost::intrusive_ptr<add_by_slot_command>(new add_by_slot_command(slot_, variant()));
					}
				}
			}
		}
	}
private:
	variant execute(const FormulaCallable& variables) const {
		if(me_slot_ != -1) {
			variant target = variables.queryValueBySlot(me_slot_);
			if(slot_ != -1) {
				return variant(new add_target_by_slot_command(target, slot_, args()[1]->evaluate(variables)));
			} else if(!key_.empty()) {
				return variant(new add_command(target, key_, variant(), args()[1]->evaluate(variables)));
			}
		} else if(slot_ != -1) {
			if(cmd_->refcount() == 1) {
				cmd_->setValue(args()[1]->evaluate(variables));
				cmd_->setExpression(this);
				return variant(cmd_.get());
			}

			cmd_ = boost::intrusive_ptr<add_by_slot_command>(new add_by_slot_command(slot_, args()[1]->evaluate(variables)));
			cmd_->setExpression(this);
			return variant(cmd_.get());
		}

		if(!key_.empty()) {
			static const std::string MeKey = "me";
			variant target = variables.queryValue(MeKey);
			add_command* cmd = new add_command(target, key_, variant(), args()[1]->evaluate(variables));
			cmd->setExpression(this);
			return variant(cmd);
		}

		if(args().size() == 2) {
			std::string member;
			variant variant_member;
			variant target = args()[0]->evaluateWithMember(variables, member, &variant_member);
			add_command* cmd = new add_command(
			      target, member, variant_member, args()[1]->evaluate(variables));
			cmd->setExpression(this);
			return variant(cmd);
		}

		variant target;
		if(args().size() == 3) {
			target = args()[0]->evaluate(variables);
		}
		const int begin_index = args().size() == 2 ? 0 : 1;
		add_command* cmd = new add_command(
		    target,
		    args()[begin_index]->evaluate(variables).as_string(), variant(),
			args()[begin_index + 1]->evaluate(variables));
		cmd->setExpression(this);
		return variant(cmd);
	}

	variant_type_ptr getVariantType() const {
		return variant_type::get_commands();
	}

	void staticErrorAnalysis() const {
		variant_type_ptr target_type = args()[0]->queryMutableType();
		if(!target_type) {
			ASSERT_LOG(false, "Writing to non-writeable value: " << args()[0]->queryVariantType()->to_string() << " in " << str() << " " << debugPinpointLocation() << "\n");
			return;
		}

		if(!variant_types_compatible(target_type, args()[1]->queryVariantType())) {
			ASSERT_LOG(false, "Writing to value with invalid type " << args()[1]->queryVariantType()->to_string() << " -> " << args()[0]->queryVariantType()->to_string() << " in " << str() << " " << debugPinpointLocation() << "\n");
		}
	}

	std::string key_;
	int me_slot_, slot_;
	mutable boost::intrusive_ptr<add_by_slot_command> cmd_;
};


class debug_command : public game_logic::CommandCallable
{
public:
	explicit debug_command(const std::string& str) : str_(str)
	{}
	virtual void execute(FormulaCallable& ob) const {
#ifndef NO_EDITOR
		debug_console::addMessage(str_);
#endif
		LOG_INFO("CONSOLE: " << str_);
	}
private:
	std::string str_;
};

FUNCTION_DEF(debug, 1, -1, "debug(...): outputs arguments to the console")
	if(!preferences::debug()) {
		return variant();
	}

	std::string str;
	for(int n = 0; n != args().size(); ++n) {
		if(n > 0) {
			str += " ";
		}

		str += args()[n]->evaluate(variables).to_debug_string();
	}

	//fprintf(stderr, "DEBUG FUNCTION: %s\n", str.c_str());

	return variant(new debug_command(str));

FUNCTION_TYPE_DEF
	return variant_type::get_commands();
END_FUNCTION_DEF(debug)

namespace 
{
	void debug_side_effect(variant v)
	{
		std::string s = v.to_debug_string();
	#ifndef NO_EDITOR
		debug_console::addMessage(s);
	#endif
		LOG_INFO("CONSOLE: " << s);
	}
}

FUNCTION_DEF(dump, 1, 2, "dump(msg[, expr]): evaluates and returns expr. Will print 'msg' to stderr if it's printable, or execute it if it's an executable command.")
	debug_side_effect(args().front()->evaluate(variables));
	variant res = args().back()->evaluate(variables);

	return res;
FUNCTION_TYPE_DEF
	return args().back()->queryVariantType();
END_FUNCTION_DEF(dump)

bool consecutive_periods(char a, char b) {
	return a == '.' && b == '.';
}

std::map<std::string, variant>& get_doc_cache() {
	static std::map<std::string, variant> cache;
	return cache;
}

PREF_BOOL(write_backed_maps, false, "Write to backed maps such as used in Citadel's evolutionary system");

class backed_map : public game_logic::FormulaCallable {
public:
	static void flush_all() {
		for(backed_map* m : all_backed_maps) {
			m->write_file();
		}
	}

	backed_map(const std::string& docname, variant generator, variant m)
	  : docname_(docname), generator_(generator)
	{
		all_backed_maps.insert(this);

		if(m.is_map()) {
			for(const variant::map_pair& p : m.as_map()) {
				map_[p.first.as_string()].value = p.second;
			}
		}

		if(sys::file_exists(docname_)) {
			try {
				variant v = json::parse(sys::read_file(docname_));

				if(sys::file_exists(docname_ + ".stats")) {
					try {
						variant stats = json::parse(sys::read_file(docname_ + ".stats"));
						for(const variant::map_pair& p : stats.as_map()) {
							map_[p.first.as_string()] = NodeInfo(p.second);
						}
					} catch(json::ParseError&) {
					}
				}

				for(const variant::map_pair& p : v.as_map()) {
					if(p.first.as_string() != "_stats") {
						map_[p.first.as_string()].value = p.second;
					}
				}
			} catch(json::ParseError& e) {
				ASSERT_LOG(false, "Error parsing json for backed map in " << docname_ << ": " << e.errorMessage());
			}
		}

		write_file();
	}

	~backed_map() {
		write_file();
		all_backed_maps.erase(this);
	}
private:
	variant getValue(const std::string& key) const {
		std::map<std::string, NodeInfo>::const_iterator i = map_.find(key);
		if(i != map_.end()) {
			i->second.last_session_reads++;
			i->second.lifetime_reads++;
			return i->second.value;
		}

		std::vector<variant> args;
		variant new_value = generator_(args);
		const_cast<backed_map*>(this)->mutateValue(key, new_value);
		return new_value;
	}

	void setValue(const std::string& key, const variant& value) {
		map_[key].value = value;

		write_file();
	}

	void write_file()
	{
		if(!g_write_backed_maps) {
			return;
		}

		std::map<variant, variant> v;
		std::map<variant, variant> stats;
		for(std::map<std::string,NodeInfo>::const_iterator i = map_.begin();
		    i != map_.end(); ++i) {
			v[variant(i->first)] = i->second.value;
			stats[variant(i->first)] = i->second.write();
		}

		sys::write_file(docname_, variant(&v).write_json());
		sys::write_file(docname_ + ".stats", variant(&stats).write_json());
	}

	struct NodeInfo {
		NodeInfo() : last_session_reads(0), lifetime_reads(0), value(0)
		{}

		NodeInfo(variant v) : last_session_reads(0), lifetime_reads(v["lifetime_reads"].as_int()), value(0)
		{}
		
		variant write() const {
			std::map<variant,variant> m;
			m[variant("last_session_reads")] = variant(last_session_reads);
			m[variant("lifetime_reads")] = variant(lifetime_reads);
			return variant(&m);
		}

		mutable int last_session_reads;
		mutable int lifetime_reads;

		variant value;
	};

	std::string docname_;
	std::map<std::string, NodeInfo> map_;
	variant generator_;
	static std::set<backed_map*> all_backed_maps;
};

std::set<backed_map*> backed_map::all_backed_maps;
} //namespace {

void flush_all_backed_maps()
{
	backed_map::flush_all();
}

namespace {

FUNCTION_DEF(file_backed_map, 2, 3, "file_backed_map(string filename, function generate_new, map initial_values)")
	Formula::failIfStaticContext();
	std::string docname = args()[0]->evaluate(variables).as_string();

	if(docname.empty()) {
		return variant("DOCUMENT NAME GIVEN TO write_document() IS EMPTY");
	}
	if(sys::is_path_absolute(docname)) {
		return variant(formatter() << "DOCUMENT NAME IS ABSOLUTE PATH " << docname);
	}
	if(std::adjacent_find(docname.begin(), docname.end(), consecutive_periods) != docname.end()) {
		return variant(formatter() << "RELATIVE PATH OUTSIDE ALLOWED " << docname);
	}

	if(sys::file_exists(module::map_file(docname))) {
		docname = module::map_file(docname);
	} else {
		docname = preferences::user_data_path() + docname;
	}

	variant fn = args()[1]->evaluate(variables);

	variant m;
	if(args().size() > 2) {
		m = args()[2]->evaluate(variables);
	}

	return variant(new backed_map(docname, fn, m));
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_CALLABLE);
END_FUNCTION_DEF(file_backed_map)

FUNCTION_DEF(write_document, 2, 2, "write_document(string filename, doc): writes 'doc' to the given filename")
	Formula::failIfStaticContext();
	std::string docname = args()[0]->evaluate(variables).as_string();
	variant doc = args()[1]->evaluate(variables);


	std::string path_error;
	if(!sys::is_safe_write_path(docname, &path_error)) {
		ASSERT_LOG(false, "ERROR in write_document(" + docname + "): " + path_error);
	}

	if(docname.empty()) {
		ASSERT_LOG(false, "DOCUMENT NAME GIVEN TO write_document() IS EMPTY");
	}
	if(sys::is_path_absolute(docname)) {
		ASSERT_LOG(false, "DOCUMENT NAME IS ABSOLUTE PATH " << docname);
	}
	if(std::adjacent_find(docname.begin(), docname.end(), consecutive_periods) != docname.end()) {
		ASSERT_LOG(false, "RELATIVE PATH OUTSIDE ALLOWED " << docname);
	}

	return variant(new FnCommandCallableArg([=](FormulaCallable* callable) {
		get_doc_cache()[docname] = doc;

		std::string real_docname = preferences::user_data_path() + docname;
		sys::write_file(real_docname, game_logic::serialize_doc_with_objects(doc));
	}));
FUNCTION_ARGS_DEF
	ARG_TYPE("string");
	ARG_TYPE("any");
	RETURN_TYPE("commands")
END_FUNCTION_DEF(write_document)

FUNCTION_DEF(get_document, 1, 2, "get_document(string filename, [enum {'null_on_failure', 'user_preferences_dir'}] flags): return reference to the given JSON document. flags can contain 'null_on_failure' and 'user_preferences_dir'")

	if(args().size() != 1) {
		Formula::failIfStaticContext();
	}

	std::string docname = args()[0]->evaluate(variables).as_string();
	ASSERT_LOG(docname.empty() == false, "DOCUMENT NAME GIVEN TO get_document() IS EMPTY");

	bool allow_failure = false;
	bool prefs_directory = false;

	if(args().size() > 1) {
		const variant flags = args()[1]->evaluate(variables);
		for(int n = 0; n != flags.num_elements(); ++n) {
			const std::string& flag = flags[n].as_string();
			if(flag == "null_on_failure") {
				allow_failure = true;
			} else if(flag == "user_preferences_dir") {
				prefs_directory = true;
			} else {
				ASSERT_LOG(false, "illegal flag given to get_document: " << flag);
			}
		}
	}

	variant& v = get_doc_cache()[docname];
	if(v.is_null() == false) {
		return v;
	}

	ASSERT_LOG(std::adjacent_find(docname.begin(), docname.end(), consecutive_periods) == docname.end(), "DOCUMENT NAME CONTAINS ADJACENT PERIODS " << docname);

	if(prefs_directory) {
		//docname = sys::compute_relative_path(preferences::user_data_path(), docname);
		docname = preferences::user_data_path() + docname;
	} else {
		ASSERT_LOG(!sys::is_path_absolute(docname), "DOCUMENT NAME USES AN ABSOLUTE PATH WHICH IS NOT ALLOWED: " << docname);
		docname = module::map_file(docname);
	}

	try {
		return game_logic::deserialize_file_with_objects(docname);
	} catch(json::ParseError& e) {
		if(allow_failure) {
			return variant();
		}

		ASSERT_LOG(false, "COULD NOT LOAD DOCUMENT: " << e.errorMessage());
		return variant();
	}
FUNCTION_ARGS_DEF
	ARG_TYPE("string");
FUNCTION_TYPE_DEF
	std::vector<variant_type_ptr> types;
	types.push_back(variant_type::get_type(variant::VARIANT_TYPE_MAP));
	types.push_back(variant_type::get_type(variant::VARIANT_TYPE_NULL));
	return variant_type::get_union(types);
END_FUNCTION_DEF(get_document)

}

void remove_formula_function_cached_doc(const std::string& name)
{
	get_doc_cache().erase(name);
}

void FunctionExpression::check_arg_type(int narg, const std::string& type_str) const
{
	if(static_cast<unsigned>(narg) >= args().size()) {
		return;
	}

	variant type_v(type_str);
	variant_type_ptr type;
	try {
		type = parse_variant_type(type_v);
	} catch(...) {
		ASSERT_LOG(false, "BAD ARG TYPE SPECIFIED: " << type);
	}

	variant_type_ptr provided = args()[narg]->queryVariantType();
	assert(provided);

	if(!variant_types_compatible(type, provided)) {
		std::ostringstream reason;
		variant_types_compatible(type, provided, &reason);
		std::string msg = reason.str();
		if(msg.empty() == false) {
			msg = " (" + msg + ")";
		}
		ASSERT_LOG(variant_types_compatible(type, provided), "Function call argument " << (narg+1) << " does not match. Function expects " << type_str << " provided " << provided->to_string() << msg << " " << debugPinpointLocation())
	}
}

FormulaFunctionExpression::FormulaFunctionExpression(const std::string& name, const args_list& args, ConstFormulaPtr formula, ConstFormulaPtr precondition, const std::vector<std::string>& arg_names, const std::vector<variant_type_ptr>& variant_types)
	: FunctionExpression(name, args, arg_names.size(), arg_names.size()),
	formula_(formula), 
	precondition_(precondition), 
	arg_names_(arg_names), 
	variant_types_(variant_types), 
	star_arg_(-1), 
	has_closure_(false), 
	base_slot_(0)
{
	assert(!precondition_ || !precondition_->str().empty());
	for(size_t n = 0; n != arg_names_.size(); ++n) {
		if(arg_names_.empty() == false && arg_names_[n][arg_names_[n].size()-1] == '*') {
			arg_names_[n].resize(arg_names_[n].size()-1);
			star_arg_ = n;
			break;
		}
	}
}

namespace 
{
	std::stack<const FormulaFunctionExpression*> formula_fn_stack;
	struct formula_function_scope 
	{
		explicit formula_function_scope(const FormulaFunctionExpression* f) {
			formula_fn_stack.push(f);
		}

		~formula_function_scope() {
			formula_fn_stack.pop();
		}
	};

	bool is_calculating_recursion = false;

	struct recursion_calculation_scope 
	{
		recursion_calculation_scope() { is_calculating_recursion = true; }
		~recursion_calculation_scope() { is_calculating_recursion = false; }
	};
}

boost::intrusive_ptr<SlotFormulaCallable> FormulaFunctionExpression::calculate_args_callable(const FormulaCallable& variables) const
{
	if(!callable_ || callable_->refcount() != 1) {
		callable_ = boost::intrusive_ptr<SlotFormulaCallable>(new SlotFormulaCallable);
		callable_->reserve(arg_names_.size());
		callable_->setBaseSlot(base_slot_);
	}

	callable_->setNames(&arg_names_);

	//we reset callable_ to NULL during any calls so that recursive calls
	//will work properly.
	boost::intrusive_ptr<SlotFormulaCallable> tmp_callable(callable_);
	callable_.reset(NULL);

	for(unsigned n = 0; n != arg_names_.size(); ++n) {
		variant var = args()[n]->evaluate(variables);

		if(n < variant_types_.size() && variant_types_[n]) {
			ASSERT_LOG(variant_types_[n]->match(var), "FUNCTION ARGUMENT " << (n+1) << " EXPECTED TYPE " << variant_types_[n]->str() << " BUT FOUND " << var.write_json() << " TYPE " << get_variant_type_from_value(var)->to_string() << " AT " << debugPinpointLocation());
		}

		tmp_callable->add(var);
		if(n == star_arg_) {
			tmp_callable->setFallback(var.as_callable());
		}
	}

	return tmp_callable;
}

variant FormulaFunctionExpression::execute(const FormulaCallable& variables) const
{
	if(fed_result_) {
		variant result = *fed_result_;
		fed_result_.reset();
		return result;
	}

	boost::intrusive_ptr<SlotFormulaCallable> tmp_callable = calculate_args_callable(variables);

	if(precondition_) {
		if(!precondition_->execute(*tmp_callable).as_bool()) {
			LOG_ERROR_NOLF("FAILED function precondition (" << precondition_->str() << ") for function '" << formula_->str() << "' with arguments: ");
			for(size_t n = 0; n != arg_names_.size(); ++n) {
				LOG_ERROR("  arg " << (n+1) << ": " << args()[n]->evaluate(variables).to_debug_string());
			}
		}
	}

	if(!is_calculating_recursion && formula_->hasGuards() && !formula_fn_stack.empty() && formula_fn_stack.top() == this) {
		const recursion_calculation_scope recursion_scope;

		typedef boost::intrusive_ptr<FormulaCallable> call_ptr;
		std::vector<call_ptr> invocations;
		invocations.push_back(tmp_callable);
		while(formula_->guardMatches(*invocations.back()) == -1) {
			invocations.push_back(calculate_args_callable(*formula_->wrapCallableWithGlobalWhere(*invocations.back())));
		}

		invocations.pop_back();

		if(invocations.size() > 2) {
			while(invocations.empty() == false) {
				fed_result_.reset(new variant(formula_->expr()->evaluate(*formula_->wrapCallableWithGlobalWhere(*invocations.back()))));
				invocations.pop_back();
			}

			variant result = *fed_result_;
			fed_result_.reset();
			return result;
		}
	}

	formula_function_scope scope(this);
	variant res = formula_->execute(*tmp_callable);

	callable_ = tmp_callable;
	callable_->clear();

	return res;
}

	FormulaFunctionExpressionPtr FormulaFunction::generateFunctionExpression(const std::vector<ExpressionPtr>& args_input) const
	{
		std::vector<ExpressionPtr> args = args_input;
		if(args.size() + default_args_.size() >= args_.size()) {
			const int base = args_.size() - default_args_.size();
			while(args.size() < args_.size()) {
				const unsigned index = args.size() - base;
				ASSERT_LOG(index >= 0 && index < default_args_.size(), "INVALID INDEX INTO DEFAULT ARGS: " << index << " / " << default_args_.size());
				args.push_back(ExpressionPtr(new VariantExpression(default_args_[index])));
			}
		}

		return FormulaFunctionExpressionPtr(new FormulaFunctionExpression(name_, args, formula_, precondition_, args_, variant_types_));
	}

	void FunctionSymbolTable::addFormulaFunction(const std::string& name, ConstFormulaPtr formula, ConstFormulaPtr precondition, const std::vector<std::string>& args, const std::vector<variant>& default_args, const std::vector<variant_type_ptr>& variant_types)
	{
		custom_formulas_[name] = FormulaFunction(name, formula, precondition, args, default_args, variant_types);
	}

	ExpressionPtr FunctionSymbolTable::createFunction(const std::string& fn, const std::vector<ExpressionPtr>& args, ConstFormulaCallableDefinitionPtr callable_def) const
	{

		const std::map<std::string, FormulaFunction>::const_iterator i = custom_formulas_.find(fn);
		if(i != custom_formulas_.end()) {
			return i->second.generateFunctionExpression(args);
		}

		if(backup_) {
			return backup_->createFunction(fn, args, callable_def);
		}

		return ExpressionPtr();
	}

	std::vector<std::string> FunctionSymbolTable::getFunctionNames() const
	{
		std::vector<std::string> res;
		for(std::map<std::string, FormulaFunction>::const_iterator iter = custom_formulas_.begin(); iter != custom_formulas_.end(); iter++ ) {
			res.push_back((*iter).first);
		}
		return res;
	}

	const FormulaFunction* FunctionSymbolTable::getFormulaFunction(const std::string& fn) const
	{
		const std::map<std::string, FormulaFunction>::const_iterator i = custom_formulas_.find(fn);
		if(i == custom_formulas_.end()) {
			return NULL;
		} else {
			return &i->second;
		}
	}

	RecursiveFunctionSymbolTable::RecursiveFunctionSymbolTable(const std::string& fn, const std::vector<std::string>& args, const std::vector<variant>& default_args, FunctionSymbolTable* backup, ConstFormulaCallableDefinitionPtr closure_definition, const std::vector<variant_type_ptr>& variant_types)
	: name_(fn), stub_(fn, ConstFormulaPtr(), ConstFormulaPtr(), args, default_args, variant_types), backup_(backup), closure_definition_(closure_definition)
	{
	}

	ExpressionPtr RecursiveFunctionSymbolTable::createFunction(
					const std::string& fn,
					const std::vector<ExpressionPtr>& args,
					ConstFormulaCallableDefinitionPtr callable_def) const
	{
		if(fn == name_) {
			FormulaFunctionExpressionPtr expr = stub_.generateFunctionExpression(args);
			if(closure_definition_) {
				expr->set_has_closure(closure_definition_->getNumSlots());
			}
			expr_.push_back(expr);
			return expr;
		} else if(backup_) {
			return backup_->createFunction(fn, args, callable_def);
		}

		return ExpressionPtr();
	}

	void RecursiveFunctionSymbolTable::resolveRecursiveCalls(ConstFormulaPtr f)
	{
		for(FormulaFunctionExpressionPtr& fn : expr_) {
			fn->set_formula(f);
		}
	}

namespace {

	typedef std::map<std::string, FunctionCreator*> functions_map;

	functions_map& get_functions_map() {

		static functions_map functions_table;

		if(functions_table.empty()) {
	#define FUNCTION(name) functions_table[#name] = new SpecificFunctionCreator<name##_function>();
			FUNCTION(if);
			FUNCTION(filter);
			FUNCTION(mapping);
			FUNCTION(find);
			FUNCTION(find_or_die);
			FUNCTION(visit_objects);
			FUNCTION(map);
			FUNCTION(sum);
			FUNCTION(range);
			FUNCTION(head);
			FUNCTION(size);
			FUNCTION(split);
			FUNCTION(split_any_of);
			FUNCTION(slice);
			FUNCTION(str);
			FUNCTION(strstr);
			FUNCTION(null);
			FUNCTION(refcount);
			FUNCTION(deserialize);
			FUNCTION(is_string);
			FUNCTION(is_null);
			FUNCTION(is_int);
			FUNCTION(is_bool);
			FUNCTION(is_decimal);
			FUNCTION(is_number);
			FUNCTION(is_map);
			FUNCTION(mod);
			FUNCTION(is_function);
			FUNCTION(is_list);
			FUNCTION(is_callable);
			FUNCTION(list_str);
	#undef FUNCTION
		}

		return functions_table;
	}

}

ExpressionPtr createFunction(const std::string& fn,
                               const std::vector<ExpressionPtr>& args,
							   const FunctionSymbolTable* symbols,
							   ConstFormulaCallableDefinitionPtr callable_def)
{
	if(fn == "set") {
		return ExpressionPtr(new set_function(args, callable_def));
	} else if(fn == "add") {
		return ExpressionPtr(new add_function(args, callable_def));
	}

	if(symbols) {
		ExpressionPtr res(symbols->createFunction(fn, args, callable_def));
		if(res) {
			return res;
		}
	}

	const std::map<std::string, FunctionCreator*>& creators = get_function_creators(FunctionModule);
	std::map<std::string, FunctionCreator*>::const_iterator creator_itor = creators.find(fn);
	if(creator_itor != creators.end()) {
		return ExpressionPtr(creator_itor->second->create(args));
	}

	functions_map::const_iterator i = get_functions_map().find(fn);
	if(i == get_functions_map().end()) {
		return ExpressionPtr();
	}

	return ExpressionPtr(i->second->create(args));
}

std::vector<std::string> builtin_function_names()
{
	std::vector<std::string> res;
	const functions_map& m = get_functions_map();
	for(functions_map::const_iterator i = m.begin(); i != m.end(); ++i) {
		res.push_back(i->first);
	}

	return res;
}

FunctionExpression::FunctionExpression(
                    const std::string& name,
                    const args_list& args,
                    int min_args, int max_args)
    : name_(name), args_(args), min_args_(min_args), max_args_(max_args)
{
	setName(name.c_str());
}

void FunctionExpression::setDebugInfo(const variant& parent_formula,
	                            std::string::const_iterator begin_str,
	                            std::string::const_iterator end_str)
{
	FormulaExpression::setDebugInfo(parent_formula, begin_str, end_str);

	if(min_args_ >= 0 && args_.size() < static_cast<size_t>(min_args_) ||
	   max_args_ >= 0 && args_.size() > static_cast<size_t>(max_args_)) {
		ASSERT_LOG(false, "ERROR: incorrect number of arguments to function '" << name_ << "': expected between " << min_args_ << " and " << max_args_ << ", found " << args_.size() << "\n" << debugPinpointLocation());
	}
}

namespace {
bool point_in_triangle(point p, point t[3]) 
{
	point v0(t[2].x - t[0].x, t[2].y - t[0].y);
	point v1(t[1].x - t[0].x, t[1].y - t[0].y);
	point v2(p.x - t[0].x, p.y - t[0].y);

	int dot00 = t[0].x * t[0].x + t[0].y * t[0].y;
	int dot01 = t[0].x * t[1].x + t[0].y * t[1].y;
	int dot02 = t[0].x * t[2].x + t[0].y * t[2].y;
	int dot11 = t[1].x * t[1].x + t[1].y * t[1].y;
	int dot12 = t[1].x * t[2].x + t[1].y * t[2].y;
	float invDenom = 1 / float(dot00 * dot11 - dot01 * dot01);
	float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
	float v = (dot00 * dot12 - dot01 * dot02) * invDenom;
	return u >= 0.0f && v >= 0.0f && (u+v) < 1.0f;
}
}

FUNCTION_DEF(hex_get_tile_at, 3, 3, "hex_get_tile_at(hexmap, x, y) -> hex_tile object: Finds the hex tile at the given level co-ordinates")
	// Because we assume hexes are placed at a regular series of intervals
	variant v = args()[0]->evaluate(variables);
	hex::HexMapPtr hexmap = hex::HexMapPtr(v.try_convert<hex::HexMap>());
	ASSERT_LOG(hexmap, "hexmap not of the correct type.");
	const int mx = args()[1]->evaluate(variables).as_int();
	const int my = args()[2]->evaluate(variables).as_int();

	return variant(hexmap->getTileFromPixelPos(mx, my).get());
END_FUNCTION_DEF(hex_get_tile_at)

FUNCTION_DEF(pixel_to_tile_coords, 1, 2, "pixel_to_tile_coords(args) -> [x,y]: Gets the tile at the pixel position given in the arguments. The position"
	"can either be a single list of two values suck as [x,y] or two seperate x,y co-ordinates.")
	int x, y;
	if(args().size() == 1) {
		variant vl = args()[0]->evaluate(variables);
		ASSERT_LOG(vl.is_list() && vl.num_elements() == 2, "Single argument must be a list of two elements");
		x = vl[0].as_int();
		y = vl[1].as_int();
	} else {
		x = args()[0]->evaluate(variables).as_int();
		y = args()[1]->evaluate(variables).as_int();
	}
	point xy = hex::HexMap::getTilePosFromPixelPos(x,y);
	std::vector<variant> v;
	v.push_back(variant(xy.x));
	v.push_back(variant(xy.y));
	return variant(&v);
END_FUNCTION_DEF(pixel_to_tile_coords)

FUNCTION_DEF(tile_to_pixel_coords, 2, 3, "tile_to_pixel_coords(x, y, (opt)string) -> [x,y]: Gets the center pixel co-ordinates of a given tile co-ordinate."
	"string can be effect the co-ordinates returned. \"bounding\" -> [x,y,w,h] Bounding rect of the tile. \"center\" -> [x,y] center co-ordinates of the tile(default)"
	"\"hex\" -> [[x0,y0],[x1,y1],[x2,y2],[x3,y3],[x4,y4],[x5,y5]] Co-ordinates of points around outside of the tile.")
	const int x = args()[0]->evaluate(variables).as_int();
	const int y = args()[1]->evaluate(variables).as_int();
	point p(hex::HexMap::getPixelPosFromTilePos(x, y));
	std::vector<variant> v;
	const int HexTileSize = 72;
	if(args().size() > 2) {
		const std::string opt(args()[2]->evaluate(variables).as_string());
		if(opt == "bounding" || opt == "rect") {
			v.push_back(variant(p.x));
			v.push_back(variant(p.y));
			v.push_back(variant(HexTileSize));
			v.push_back(variant(HexTileSize));
		} else if(opt == "hex") {
			const float angle = 2.0f * 3.14159265358979f / 6.0f;
			for(int i = 0; i < 6; i++) {
				v.push_back(variant(decimal(p.x + HexTileSize/2 + HexTileSize/2.0f * sin(i * angle))));
				v.push_back(variant(decimal(p.y + HexTileSize/2 + HexTileSize/2.0f * cos(i * angle))));
			}
		} else {
			v.push_back(variant(p.x + HexTileSize/2));
			v.push_back(variant(p.y + HexTileSize/2));
		}
		// unknown just drop down and do default
	} else {
		v.push_back(variant(p.x + HexTileSize/2));
		v.push_back(variant(p.y + HexTileSize/2));
	}
	return variant(&v);
END_FUNCTION_DEF(tile_to_pixel_coords)

FUNCTION_DEF(hex_pixel_coords, 2, 2, "hex_pixel_coords(x,y) -> [x,y]: Converts a pair of pixel co-ordinates to the corresponding tile co-ordinate.")
	const int x = args()[0]->evaluate(variables).as_int();
	const int y = args()[1]->evaluate(variables).as_int();
	point p(hex::HexMap::getTilePosFromPixelPos(x, y));
	std::vector<variant> v;
	v.push_back(variant(p.x));
	v.push_back(variant(p.y));
	return variant(&v);
END_FUNCTION_DEF(hex_pixel_coords)

FUNCTION_DEF(hex_location, 3, 3, "hex_location(x,y,string dir) -> [x,y]: calculates the co-ordinates of the tile in the given direction.")
	const int x = args()[0]->evaluate(variables).as_int();
	const int y = args()[1]->evaluate(variables).as_int();
	variant d = args()[2]->evaluate(variables);
	point p(x,y);
	if(d.is_list()) {
		for(unsigned i = 0; i < d.num_elements(); i++) {
			p = hex::HexMap::locInDir(p.x, p.y, d[i].as_string());
		}
	} else if(d.is_string()) {
		const std::string dir(d.as_string());
		p = hex::HexMap::locInDir(x, y, dir);
	}
	std::vector<variant> v;
	v.push_back(variant(p.x));
	v.push_back(variant(p.y));
	return variant(&v);
END_FUNCTION_DEF(hex_location)

FUNCTION_DEF(hex_get_tile, 1, 1, "hex_get_tile(string) -> hex_tile object: Returns a hex tile object with the given name.")
	const std::string& tstr(args()[0]->evaluate(variables).as_string());
	return variant(hex::HexObject::getHexTile(tstr).get());
END_FUNCTION_DEF(hex_get_tile)

FUNCTION_DEF(hex_get_random_tile, 1, 2, "hex_get_random_tile(regex, (opt)count) -> hex_tile object(s): Generates either a single random tile or an array of count random tiles, picked from the given regular expression")
	const boost::regex re(args()[0]->evaluate(variables).as_string());
	std::vector<hex::TileTypePtr>& tile_list = hex::HexObject::getEditorTiles();
	std::vector<hex::TileTypePtr> matches;
	for(size_t i = 0; i < tile_list.size(); ++i) {
		if(boost::regex_match(tile_list[i]->getgetEditorInfo().type, re)) {
			matches.push_back(tile_list[i]);
		}
	}
	if(matches.empty()) {
		return variant();
	}
	if(args().size() > 1) {
		const int count = args()[1]->evaluate(variables).as_int();
		std::vector<variant> v;
		for(int i = 0; i < count; ++i ) {
			v.push_back(variant(matches[rand() % matches.size()].get()));
		}
		return variant(&v);
	} else {
		return variant(matches[rand() % matches.size()].get());
	}
END_FUNCTION_DEF(hex_get_random_tile)

FUNCTION_DEF(sha1, 1, 1, "sha1(string) -> string: Returns the sha1 hash of the given string")
	variant v = args()[0]->evaluate(variables);
	const std::string& s = v.as_string();
	boost::uuids::detail::sha1 hash;
	hash.process_bytes(s.c_str(), s.length());
	unsigned int digest[5];
	hash.get_digest(digest);
	std::stringstream str;
	for(int n = 0; n < 5; ++n) {
		str << std::hex << std::setw(8) << std::setfill('0') << digest[n];
	}
	return variant(str.str());
END_FUNCTION_DEF(sha1)

FUNCTION_DEF(get_module_args, 0, 0, "get_module_args() -> callable: Returns the current module callable environment")
	Formula::failIfStaticContext();
	return variant(module::get_module_args().get());
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_CALLABLE);
END_FUNCTION_DEF(get_module_args)

FUNCTION_DEF(seed_rng, 0, 0, "seed_rng() -> none: Seeds the peudo-RNG used.")
	Formula::failIfStaticContext();
	::srand(static_cast<unsigned>(::time(NULL)));
	return variant();
END_FUNCTION_DEF(seed_rng)

FUNCTION_DEF(lower, 1, 1, "lower(s) -> string: lowercase version of string")
	std::string s = args()[0]->evaluate(variables).as_string();
	boost::algorithm::to_lower(s);
	return variant(s);
END_FUNCTION_DEF(lower)

FUNCTION_DEF(rects_intersect, 2, 2, "rects_intersect([int], [int]) ->bool")
	rect a(args()[0]->evaluate(variables));
	rect b(args()[1]->evaluate(variables));

	return variant::from_bool(rects_intersect(a, b));
FUNCTION_TYPE_DEF
	return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
END_FUNCTION_DEF(rects_intersect)

namespace {
void run_expression_for_edit_and_continue(ExpressionPtr expr, const game_logic::FormulaCallable* variables, bool* success)
{
	*success = false;
	expr->evaluate(*variables);
	*success = true;
}
}

FUNCTION_DEF(edit_and_continue, 2, 2, "edit_and_continue(expr, filename)")
	if(!preferences::edit_and_continue()) {
		return args()[0]->evaluate(variables);
	}

	const std::string filename = args()[1]->evaluate(variables).as_string();

	try {
		assert_recover_scope scope;
		return args()[0]->evaluate(variables);
	} catch (validation_failure_exception& e) {
		bool success = false;
		std::function<void()> fn(std::bind(run_expression_for_edit_and_continue, args()[0], &variables, &success));

		edit_and_continue_fn(filename, e.msg, fn);
		if(success == false) {
			_exit(0);
		}

		return args()[0]->evaluate(variables);
	}
END_FUNCTION_DEF(edit_and_continue)

class console_output_to_screen_command : public game_logic::CommandCallable
{
	bool value_;
public:
	explicit console_output_to_screen_command(bool value) : value_(value)
	{}

	virtual void execute(game_logic::FormulaCallable& ob) const 
	{
		debug_console::enable_screen_output(value_);
	}
};

FUNCTION_DEF(console_output_to_screen, 1, 1, "console_output_to_screen(bool) -> none: Turns the console output to the screen on and off")
	Formula::failIfStaticContext();
	return variant(new console_output_to_screen_command(args()[0]->evaluate(variables).as_bool()));
END_FUNCTION_DEF(console_output_to_screen)

FUNCTION_DEF(user_preferences_path, 0, 0, "user_preferences_path() -> string: Returns the users preferences path")
	return variant(preferences::user_data_path());
END_FUNCTION_DEF(user_preferences_path)

class set_user_details_command : public game_logic::CommandCallable
{
	std::string username_;
	std::string password_;
public:
	explicit set_user_details_command(const std::string& username, const std::string& password) 
		: username_(username), password_(password)
	{}
	virtual void execute(game_logic::FormulaCallable& ob) const 
	{
		preferences::set_username(username_);
		if(password_.empty() == false) {
			preferences::set_password(password_);
		}
	}
};

FUNCTION_DEF(set_user_details, 1, 2, "set_user_details(string username, (opt) string password) -> none: Sets the username and password in the preferences.")
	Formula::failIfStaticContext();
	return variant(new set_user_details_command(args()[0]->evaluate(variables).as_string(),
		args().size() > 1 ? args()[1]->evaluate(variables).as_string() : ""));
END_FUNCTION_DEF(set_user_details)

FUNCTION_DEF(clamp, 3, 3, "clamp(numeric value, numeric min_val, numeric max_val) -> numeric: Clamps the given value inside the given bounds.")
	variant v  = args()[0]->evaluate(variables);
	variant mn = args()[1]->evaluate(variables);
	variant mx = args()[2]->evaluate(variables);
	if(v.is_decimal() || mn.is_decimal() || mx.is_decimal()) {
		return variant(std::min(mx.as_decimal(), std::max(mn.as_decimal(), v.as_decimal())));
	}
	return variant(std::min(mx.as_int(), std::max(mn.as_int(), v.as_int())));
FUNCTION_ARGS_DEF
	ARG_TYPE("decimal|int")
	ARG_TYPE("decimal|int")
	ARG_TYPE("decimal|int")
	DEFINE_RETURN_TYPE
	std::vector<variant_type_ptr> result_types;
	for(int n = 0; n != args().size(); ++n) {
		result_types.push_back(args()[n]->queryVariantType());
	}
	return variant_type::get_union(result_types);
END_FUNCTION_DEF(clamp)

class set_cookie_command : public game_logic::CommandCallable
{
	variant cookie_;
public:
	explicit set_cookie_command(const variant& cookie) 
		: cookie_(cookie)
	{}
	virtual void execute(game_logic::FormulaCallable& ob) const 
	{
		preferences::set_cookie(cookie_);
	}
};

FUNCTION_DEF(set_cookie, 1, 1, "set_cookie(data) -> none: Sets the preferences user_data")
	Formula::failIfStaticContext();
	return variant(new set_cookie_command(args()[0]->evaluate(variables)));
END_FUNCTION_DEF(set_cookie)

FUNCTION_DEF(get_cookie, 0, 0, "get_cookie() -> none: Returns the preferences user_data")
	Formula::failIfStaticContext();
	return preferences::get_cookie();
END_FUNCTION_DEF(get_cookie)

FUNCTION_DEF(types_compatible, 2, 2, "types_compatible(string a, string b) ->bool: returns true if type 'b' is a subset of type 'a'")
	const variant a = args()[0]->evaluate(variables);
	const variant b = args()[1]->evaluate(variables);
	return variant::from_bool(variant_types_compatible(parse_variant_type(a), parse_variant_type(b)));
END_FUNCTION_DEF(types_compatible)

FUNCTION_DEF(typeof, 1, 1, "typeof(expression) -> string: yields the statically known type of the given expression")
	variant v = args()[0]->evaluate(variables);
	return variant(get_variant_type_from_value(v)->to_string());
END_FUNCTION_DEF(typeof)

FUNCTION_DEF(static_typeof, 1, 1, "static_typeof(expression) -> string: yields the statically known type of the given expression")
	variant_type_ptr type = args()[0]->queryVariantType();
	ASSERT_LOG(type.get() != NULL, "NULL VALUE RETURNED FROM TYPE QUERY");
	return variant(type->base_type_no_enum()->to_string());
END_FUNCTION_DEF(static_typeof)

class gc_command : public game_logic::CommandCallable
{
public:
	virtual void execute(game_logic::FormulaCallable& ob) const 
	{
		CustomObject::run_garbage_collection();
	}
};

FUNCTION_DEF(trigger_garbage_collection, 0, 0, "trigger_garbage_collection(): trigger an FFL garbage collection")
	return variant(new gc_command);
END_FUNCTION_DEF(trigger_garbage_collection)

class debug_dump_textures_command : public game_logic::CommandCallable
{
	std::string fname_, info_;
public:
	debug_dump_textures_command(const std::string& fname, const std::string& info) : fname_(fname), info_(info)
	{}
	virtual void execute(game_logic::FormulaCallable& ob) const 
	{
		const std::string* info = NULL;
		if(info_.empty() == false) {
			info = &info_;
		}
		ASSERT_LOG(false, "XXX write KRE::Texture::DebugDumpTextures(file, info)");
		//graphics::texture::debug_dump_textures(fname_.c_str(), info);
	}
};

FUNCTION_DEF(debug_dump_textures, 1, 2, "debug_dump_textures(string dir, string name=null): dump textures to the given directory")
	std::string path = args()[0]->evaluate(variables).as_string();
	std::string name;
	if(args().size() > 1) {
		name = args()[1]->evaluate(variables).as_string();
	}

	return variant(new debug_dump_textures_command(path, name));
END_FUNCTION_DEF(debug_dump_textures)

class mod_object_callable : public FormulaCallable {
public:
	explicit mod_object_callable(boost::intrusive_ptr<formula_object> obj) : obj_(obj), v_(obj.get())
	{}

private:
	variant getValue(const std::string& key) const {
		if(key == "object") {
			return v_;
		} else {
			ASSERT_LOG(false, "Unknown key: " << key);
		}

		return variant();
	}

	variant getValueBySlot(int slot) const {
		if(slot == 0) {
			return v_;
		}

		ASSERT_LOG(false, "Unknown key: " << slot);
		return variant();
	}

	boost::intrusive_ptr<formula_object> obj_;
	variant v_;
};

FUNCTION_DEF(inspect_object, 1, 1, "inspect_object(object obj) -> map: outputs an object's properties")
	variant obj = args()[0]->evaluate(variables);
	variant_type_ptr type = get_variant_type_from_value(obj);

	const game_logic::FormulaCallableDefinition* def = type->getDefinition();
	if(!def) {
		return variant();
	}

	std::map<variant, variant> m;

	const game_logic::FormulaCallable* callable = obj.as_callable();

	for(int slot = 0; slot < def->getNumSlots(); ++slot) {

		variant value;
		try {
			const assert_recover_scope scope;
			if(def->supportsSlotLookups()) {
				value = callable->queryValueBySlot(slot);
			} else {
				value = callable->queryValue(def->getEntry(slot)->id);
			}
		} catch(...) {
			continue;
		}

		m[variant(def->getEntry(slot)->id)] = value;
	}

	return variant(&m);

FUNCTION_TYPE_DEF
	return variant_type::get_map(variant_type::get_type(variant::VARIANT_TYPE_STRING), variant_type::get_any());
END_FUNCTION_DEF(inspect_object)

FUNCTION_DEF(get_modified_object, 2, 2, "get_modified_object(obj, commands) -> obj: yields a copy of the given object modified by the given commands")
	boost::intrusive_ptr<formula_object> obj(args()[0]->evaluate(variables).convert_to<formula_object>());

	obj = formula_object::deep_clone(variant(obj.get())).convert_to<formula_object>();

	variant commands_fn = args()[1]->evaluate(variables);

	std::vector<variant> args;
	args.push_back(variant(obj.get()));
	variant commands = commands_fn(args);

	obj->executeCommand(commands);

	return variant(obj.get());

FUNCTION_TYPE_DEF
	return args()[0]->queryVariantType();
END_FUNCTION_DEF(get_modified_object)

FUNCTION_DEF(DrawPrimitive, 1, 1, "DrawPrimitive(map): create and return a DrawPrimitive")
	variant v = args()[0]->evaluate(variables);
	return variant(graphics::DrawPrimitive::create(v).get());
FUNCTION_ARGS_DEF
ARG_TYPE("map")
RETURN_TYPE("builtin DrawPrimitive")
END_FUNCTION_DEF(DrawPrimitive)

FUNCTION_DEF(auto_update_status, 0, 0, "auto_update_info(): get info on auto update status")
	return g_auto_update_info;
FUNCTION_ARGS_DEF
RETURN_TYPE("map")
END_FUNCTION_DEF(auto_update_status)
}

FUNCTION_DEF(rotate_rect, 4, 4, "rotate_rect(int|decimal center_x, int|decimal center_y, decimal rotation, int|decimal[8] rect) -> int|decimal[8]: rotates rect and returns the result")
	variant center_x = args()[0]->evaluate(variables);
	variant center_y = args()[1]->evaluate(variables);
	float rotate = args()[2]->evaluate(variables).as_float();
	variant v = args()[3]->evaluate(variables);

	ASSERT_LE(v.num_elements(), 8);

	std::vector<variant> res;
	if(center_x.is_decimal() || center_y.is_decimal()) {
		float r[8];
		for(unsigned n = 0; n != v.num_elements(); ++n) {
			r[n] = v[n].as_float();
		}

		for(unsigned n = v.num_elements(); n < 8; ++n) {
			r[n] = 0;
		}

		rotate_rect(center_x.as_float(), center_y.as_float(), rotate, r);

		std::vector<variant> res;
		res.reserve(8);
		for(int n = 0; n != v.num_elements(); ++n) {
			res.push_back(variant(r[n]));
		}
	} else {
		short r[8];
		for(int n = 0; n != v.num_elements(); ++n) {
			r[n] = v[n].as_int();
		}

		for(int n = v.num_elements(); n < 8; ++n) {
			r[n] = 0;
		}

		rotate_rect(center_x.as_int(), center_y.as_int(), rotate, r);

		res.reserve(8);
		for(int n = 0; n != v.num_elements(); ++n) {
			res.push_back(variant(r[n]));
		}
	}
	return variant(&res);

FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal")
	ARG_TYPE("int|decimal")
	ARG_TYPE("decimal")
	ARG_TYPE("[int]")
//RETURN_TYPE("[int]")
FUNCTION_TYPE_DEF
	if(args()[1]->queryVariantType() == variant_type::get_type(variant::TYPE::VARIANT_TYPE_DECIMAL)) {
		return variant_type::get_list(args()[1]->queryVariantType());
	}
	return variant_type::get_list(args()[0]->queryVariantType());
END_FUNCTION_DEF(rotate_rect)


UNIT_TEST(modulo_operation) {
	CHECK(game_logic::Formula(variant("mod(-5, 20)")).execute() == game_logic::Formula(variant("15")).execute(), "test failed");
	CHECK(game_logic::Formula(variant("mod(-25, 20)")).execute() == game_logic::Formula(variant("15")).execute(), "test failed");
	CHECK(game_logic::Formula(variant("mod(15, 20)")).execute() == game_logic::Formula(variant("15")).execute(), "test failed");
	CHECK(game_logic::Formula(variant("mod(35, 20)")).execute() == game_logic::Formula(variant("15")).execute(), "test failed");
}

UNIT_TEST(flatten_function) {
	CHECK(game_logic::Formula(variant("flatten([1,[2,3]])")).execute() == game_logic::Formula(variant("[1,2,3]")).execute(), "test failed");
	CHECK(game_logic::Formula(variant("flatten([1,2,3,[[4,5],6]])")).execute() == game_logic::Formula(variant("[1,2,3,4,5,6]")).execute(), "test failed");
	CHECK(game_logic::Formula(variant("flatten([[1,2,3,4],5,6])")).execute() == game_logic::Formula(variant("[1,2,3,4,5,6]")).execute(), "test failed");
	CHECK(game_logic::Formula(variant("flatten([[[0,2,4],6,8],10,[12,14]])")).execute() == game_logic::Formula(variant("[0,2,4,6,8,10,12,14]")).execute(), "test failed");
}

UNIT_TEST(sqrt_function) {
	CHECK_EQ(game_logic::Formula(variant("sqrt(2147483)")).execute().as_int(), 1465);	

	for(uint64_t n = 0; n < 100000; n += 1000) {
		CHECK_EQ(game_logic::Formula(variant(formatter() << "sqrt(" << n << ".0^2)")).execute().as_decimal(), decimal::from_int(static_cast<int>(n)));
	}
}

UNIT_TEST(map_function) {
	CHECK_EQ(game_logic::Formula(variant("map([2,3,4], value+index)")).execute(), game_logic::Formula(variant("[2,4,6]")).execute());
}

UNIT_TEST(where_scope_function) {
	CHECK(game_logic::Formula(variant("{'val': num} where num = 5")).execute() == game_logic::Formula(variant("{'val': 5}")).execute(), "map where test failed");
	CHECK(game_logic::Formula(variant("'five: ${five}' where five = 5")).execute() == game_logic::Formula(variant("'five: 5'")).execute(), "string where test failed");
}

BENCHMARK(map_function) {
	using namespace game_logic;

	static MapFormulaCallable* callable = NULL;
	static variant callable_var;
	static variant main_callable_var;
	static std::vector<variant> v;
	
	if(callable == NULL) {
		callable = new MapFormulaCallable;
		callable_var = variant(callable);
		callable->add("x", variant(0));
		for(int n = 0; n != 1000; ++n) {
			v.push_back(callable_var);
		}

		callable = new MapFormulaCallable;
		main_callable_var = variant(callable);
		callable->add("items", variant(&v));
	}

	static Formula f(variant("map(items, 'obj', 0)"));
	BENCHMARK_LOOP {
		f.execute(*callable);
	}
}

namespace game_logic 
{
	ConstFormulaCallableDefinitionPtr get_map_callable_definition(ConstFormulaCallableDefinitionPtr base_def, variant_type_ptr key_type, variant_type_ptr value_type, const std::string& value_name)
	{
		return ConstFormulaCallableDefinitionPtr(new MapCallableDefinition(base_def, key_type, value_type, value_name));
	}

	ConstFormulaCallableDefinitionPtr get_variant_comparator_definition(ConstFormulaCallableDefinitionPtr base_def, variant_type_ptr type)
	{
		return ConstFormulaCallableDefinitionPtr(new variant_comparator_definition(base_def, type));
	}

	class formula_FunctionSymbolTable : public FunctionSymbolTable
	{
	public:
		virtual ExpressionPtr createFunction(
								   const std::string& fn,
								   const std::vector<ExpressionPtr>& args,
								   ConstFormulaCallableDefinitionPtr callable_def) const;
	};

	ExpressionPtr formula_FunctionSymbolTable::createFunction(
							   const std::string& fn,
							   const std::vector<ExpressionPtr>& args,
							   ConstFormulaCallableDefinitionPtr callable_def) const
	{
		const std::map<std::string, FunctionCreator*>& creators = get_function_creators(FunctionModule);
		std::map<std::string, FunctionCreator*>::const_iterator i = creators.find(fn);
		if(i != creators.end()) {
			return ExpressionPtr(i->second->create(args));
		}

		return FunctionSymbolTable::createFunction(fn, args, callable_def);
	}
}

FunctionSymbolTable& get_formula_functions_symbol_table()
{
	static formula_FunctionSymbolTable table;
	return table;
}
