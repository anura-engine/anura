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

#include <set>
#include <string>

#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_function.hpp"
#include "reference_counted_object.hpp"
#include "variant.hpp"

class CustomObject;
class Entity;
class Level;

bool in_speech_dialog();

using game_logic::FunctionSymbolTable;
FunctionSymbolTable& get_custom_object_functions_symbol_table();

class EntityCommandCallable : public game_logic::FormulaCallable 
{
public:
	EntityCommandCallable() : expr_(nullptr) {}
	void runCommand(Level& lvl, Entity& obj) const;

	void setExpression(const game_logic::FormulaExpression* expr);

	bool isCommand() const override { return true; }

private:
	virtual void execute(Level& lvl, Entity& ob) const = 0;
	variant getValue(const std::string& key) const override { return variant(); }
	void getInputs(std::vector<game_logic::FormulaInput>* inputs) const override {}

	//these two members are used as a more compiler-friendly version of a
	//intrusive_ptr<FormulaExpression>
	const game_logic::FormulaExpression* expr_;
	ffl::IntrusivePtr<const reference_counted_object> expr_holder_;
};

class CustomObjectCommandCallable : public game_logic::FormulaCallable 
{
public:
	CustomObjectCommandCallable() : expr_(nullptr) {}
	void runCommand(Level& lvl, CustomObject& ob) const;

	void setExpression(const game_logic::FormulaExpression* expr);

	bool isCommand() const override { return true; }

private:
	virtual void execute(Level& lvl, CustomObject& ob) const = 0;
	variant getValue(const std::string& key) const override { return variant(); }
	void getInputs(std::vector<game_logic::FormulaInput>* inputs) const override {}
	
	//these two members are used as a more compiler-friendly version of a
	//intrusive_ptr<FormulaExpression>
	const game_logic::FormulaExpression* expr_;
	ffl::IntrusivePtr<const reference_counted_object> expr_holder_;
};

class SwallowObjectCommandCallable : public game_logic::FormulaCallable 
{
public:
	bool isCommand() const override { return true; }
private:
	variant getValue(const std::string& key) const override { return variant(); }
	void getInputs(std::vector<game_logic::FormulaInput>* inputs) const override {}
};

class SwallowMouseCommandCallable : public game_logic::FormulaCallable 
{
public:
	bool isCommand() const override { return true; }
private:
	variant getValue(const std::string& key) const override { return variant(); }
	void getInputs(std::vector<game_logic::FormulaInput>* inputs) const override {}
};

//create one of these to track all formulas parsed during its scope which contain object spawn points.
//will record all possible objects the formulas can spawn. Useful for discovering which possible
//objects might be spawned so we can preload them.
struct ObjectTypesSpawnedTracker
{
	ObjectTypesSpawnedTracker();
	~ObjectTypesSpawnedTracker();

	std::set<std::string> spawned;
};
