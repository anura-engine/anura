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
#include <vector>

#include "formula_callable_definition.hpp"

enum CUSTOM_OBJECT_PROPERTY {
#define CUSTOM_OBJECT_CALLABLE_INC(def, value, vtype)	def,
#include "custom_object.inc"
#undef CUSTOM_OBJECT_CALLABLE_INC

	NUM_CUSTOM_OBJECT_PROPERTIES
};

enum { NUM_CUSTOM_OBJECT_NON_PLAYER_PROPERTIES = CUSTOM_OBJECT_PLAYER_DIFFICULTY };

class CustomObjectCallable : public game_logic::FormulaCallableDefinition

{
public:
	static const CustomObjectCallable& instance();

	static int getKeySlot(const std::string& key);

	explicit CustomObjectCallable(bool is_singleton=false);

	void setObjectType(variant_type_ptr type);

	int getSlot(const std::string& key) const override;
	Entry* getEntry(int slot) override;
	const Entry* getEntry(int slot) const override;
	int getNumSlots() const override { return static_cast<int>(entries_.size()); }

	const std::vector<int>& slots_requiring_initialization() const { return slots_requiring_initialization_; }

	void addProperty(const std::string& id, variant_type_ptr type, variant_type_ptr write_type, bool requires_initialization, bool is_private);

	void finalizeProperties();

	void pushPrivateAccess();
	void popPrivateAccess();

	bool getSymbolIndexForSlot(int slot, int* index) const override {
		return false;
	}

	int getBaseSymbolIndex() const override {
		return 0;
	}

private:
	int getSubsetSlotBase(const FormulaCallableDefinition* subset) const override { return -1; }
	
	std::vector<Entry> entries_;

	std::map<std::string, int> properties_;

	std::vector<int> slots_requiring_initialization_;
};

struct CustomObjectCallableExposePrivateScope
{
	CustomObjectCallableExposePrivateScope(CustomObjectCallable& c);
	~CustomObjectCallableExposePrivateScope();

	CustomObjectCallable& c_;
};

class CustomObjectCallableModifyScope
{
public:
	CustomObjectCallableModifyScope(const CustomObjectCallable& c, int slot, variant_type_ptr type);
	~CustomObjectCallableModifyScope();

private:
	CustomObjectCallable& c_;
	game_logic::FormulaCallableDefinition::Entry entry_;
	int slot_;
};

typedef ffl::IntrusivePtr<CustomObjectCallable> CustomObjectCallablePtr;
typedef ffl::IntrusivePtr<const CustomObjectCallable> ConstCustomObjectCallablePtr;
