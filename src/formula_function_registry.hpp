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
#ifndef FORMULA_FUNCTION_REGISTRY_HPP_INCLUDED
#define FORMULA_FUNCTION_REGISTRY_HPP_INCLUDED

#include "formula_function.hpp"

#include <string>

class function_creator {
public:
	virtual ~function_creator() {}
	virtual game_logic::function_expression* create(const game_logic::function_expression::args_list& args) const = 0;
};

template<typename T>
class specific_function_creator : public function_creator {
public:
	virtual ~specific_function_creator() {}
	virtual game_logic::function_expression* create(const game_logic::function_expression::args_list& args) const {
		return new T(args);
	}
};

const std::map<std::string, function_creator*>& get_function_creators(const std::string& module);

int register_function_creator(const std::string& module, const std::string& id, function_creator* creator);

const std::vector<std::string>& function_helpstrings(const std::string& module);

int register_function_helpstring(const std::string& module, const std::string& str);

#define FUNCTION_DEF(name, min_args, max_args, helpstring) \
const int name##_dummy_help_var = register_function_helpstring(FunctionModule, helpstring); \
class name##_function : public function_expression { \
public: \
	explicit name##_function(const args_list& args) \
	  : function_expression(#name, args, min_args, max_args) {} \
private: \
	variant execute(const formula_callable& variables) const {

#define END_FUNCTION_DEF(name) } }; const int name##_dummy_var = register_function_creator(FunctionModule, #name, new specific_function_creator<name##_function>());

#define FUNCTION_ARGS_DEF } void static_error_analysis() const { int narg_number = 0;
#define ARG_TYPE(str) check_arg_type(narg_number++, str);
#define FUNCTION_TYPE_DEF } variant_type_ptr get_variant_type() const {
#define RETURN_TYPE(str) } variant_type_ptr get_variant_type() const { return parse_variant_type(variant(str));
#define DEFINE_RETURN_TYPE } variant_type_ptr get_variant_type() const {

#define FUNCTION_OPTIMIZE } expression_ptr optimize() const {

#define EVAL_ARG(n) (args()[n]->evaluate(variables))
#define NUM_ARGS (args().size())


#endif
