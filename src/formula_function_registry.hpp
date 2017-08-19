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

#include "formula_function.hpp"

#include <string>

class FunctionCreator {
public:
	std::string module_name;
	explicit FunctionCreator(const std::string& module_name_) : module_name(module_name_)
	{}
	virtual ~FunctionCreator() {}
	virtual game_logic::FunctionExpression* create(const game_logic::FunctionExpression::args_list& args) const = 0;
};

template<typename T>
class SpecificFunctionCreator : public FunctionCreator {
public:
	explicit SpecificFunctionCreator(const std::string& module_name) : FunctionCreator(module_name)
	{}
	virtual ~SpecificFunctionCreator() {}
	virtual game_logic::FunctionExpression* create(const game_logic::FunctionExpression::args_list& args) const override {
		T* result =  new T(args);
		result->setModule(module_name);
		return result;
	}
};

const std::map<std::string, FunctionCreator*>& get_function_creators(const std::string& module);

int register_function_creator(const std::string& module, const std::string& id, FunctionCreator* creator);

const std::vector<std::string>& function_helpstrings(const std::string& module);

int register_function_helpstring(const std::string& module, const std::string& str);

int get_builtin_ffl_function_index(const std::string& module, const std::string& id);
game_logic::FunctionExpression* get_builtin_ffl_function_from_index(int index);

#define FUNCTION_DEF_CTOR(name, min_args, max_args, helpstring) \
const int name##_dummy_help_var = register_function_helpstring(FunctionModule, helpstring); \
class name##_function : public FunctionExpression { \
public: \
	explicit name##_function(const args_list& myargs) \
	  : FunctionExpression(#name, myargs, min_args, max_args) {
			  
#define FUNCTION_DEF_MEMBERS \
			  } \
private:

#define FUNCTION_DEF_IMPL \
	variant execute(const FormulaCallable& variables) const override { \
		return executeWithArgs(variables, nullptr, -1); \
	} \
	variant executeWithArgs(const FormulaCallable& variables, const variant* passed_args, int num_passed_args) const override {

#define FUNCTION_DEF(name, min_args, max_args, helpstring) \
FUNCTION_DEF_CTOR(name, min_args, max_args, helpstring) \
FUNCTION_DEF_MEMBERS \
FUNCTION_DEF_IMPL

#define FUNCTION_DYNAMIC_ARGUMENTS } bool dynamicArguments() const override { return true;

#define END_FUNCTION_DEF(name) } }; const int name##_dummy_var = register_function_creator(FunctionModule, #name, new SpecificFunctionCreator<name##_function>(FunctionModule));

#define FUNCTION_ARGS_DEF } void staticErrorAnalysis() const override { int num_passed_args = -1; int narg_number = 0;
#define ARG_TYPE(str) check_arg_type(narg_number++, str);
#define FUNCTION_TYPE_DEF } variant_type_ptr getVariantType() const override { int num_passed_args = -1;
#define RETURN_TYPE(str) } variant_type_ptr getVariantType() const override { return parse_variant_type(variant(str));
#define DEFINE_RETURN_TYPE } variant_type_ptr getVariantType() const override { int num_passed_args = -1;

#define FUNCTION_OPTIMIZE } ExpressionPtr optimize() const override {

#define CAN_VM } bool canCreateVM() const override {

#define FUNCTION_VM } ExpressionPtr optimizeToVM() override { \
	FunctionExpression::optimizeToVM(); \
	for(auto& a : args()) { if(a->canCreateVM() == false) { return ExpressionPtr(); } } \
	formula_vm::VirtualMachine vm;


#define EVAL_ARG(n) (num_passed_args < 0 ? args()[n]->evaluate(variables) : passed_args[n])
#define NUM_ARGS (num_passed_args < 0 ? args().size() : num_passed_args)
