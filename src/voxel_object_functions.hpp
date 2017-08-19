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

/* XXX -- needs re-write

#include <string>

#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_function.hpp"
#include "reference_counted_object.hpp"
#include "user_voxel_object.hpp"
#include "variant.hpp"

namespace voxel 
{
	class voxel_object;
	class world;
}

using game_logic::FunctionSymbolTable;
FunctionSymbolTable& get_voxel_object_functions_symbol_table();
void init_voxel_object_functions(variant node);

class voxel_object_command_callable : public game_logic::FormulaCallable 
{
public:
	voxel_object_command_callable() : expr_(nullptr) {}
	void runCommand(voxel::world& world, voxel::user_voxel_object& obj) const;

	void setExpression(const game_logic::FormulaExpression* expr);

	bool isCommand() const { return true; }

private:
	virtual void execute(voxel::world& world, voxel::user_voxel_object& ob) const = 0;
	variant getValue(const std::string& key) const { return variant(); }
	void getInputs(std::vector<game_logic::FormulaInput>* inputs) const {}

	//these two members are used as a more compiler-friendly version of a
	//intrusive_ptr<FormulaExpression>
	const game_logic::FormulaExpression* expr_;
	ffl::IntrusivePtr<const reference_counted_object> expr_holder_;
};

*/