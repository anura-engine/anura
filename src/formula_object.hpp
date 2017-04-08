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
#include <vector>

#include <boost/uuid/uuid.hpp>

#include "formula.hpp"
#include "formula_callable_definition.hpp"
#include "variant.hpp"
#include "wml_formula_callable.hpp"

#ifdef USE_LUA
#include "lua_iface.hpp"
#endif

namespace game_logic
{
	class FormulaClass;

	FormulaCallableDefinitionPtr get_class_definition(const std::string& name);

	bool is_class_derived_from(const std::string& derived, const std::string& base);

	class FormulaObject : public game_logic::WmlSerializableFormulaCallable
	{
		static void visitVariantsInternal(const variant& v, const std::function<void (variant)>& fn, std::vector<FormulaObject*>* seen);
		static void visitVariantsInternal(const variant& v, const std::function<void (FormulaObject*)>& fn, std::vector<FormulaObject*>* seen);
	public:
		static void visitVariants(const variant& v, const std::function<void (variant)>& fn);
		static void visitVariantObjects(const variant& v, const std::function<void (FormulaObject*)>& fn);
		static void mapObjectIntoDifferentTree(variant& v, const std::map<FormulaObject*, FormulaObject*>& mapping, std::set<FormulaObject*>& seen);

		void update(FormulaObject& updated);

		static variant deepClone(variant v);
		static variant deepClone(variant v, std::map<FormulaObject*,FormulaObject*>& mapping);

		static void deepDestroy(variant v);
		static void deepDestroy(variant v, std::set<FormulaObject*>& seen);
		
		static variant generateDiff(variant a, variant b);
		void applyDiff(variant delta);


		static void reloadClasses();
		static void loadAllClasses();
		static void tryLoadClass(const std::string& name);

		static ffl::IntrusivePtr<FormulaObject> create(const std::string& type, variant args=variant());

		bool isA(const std::string& class_name) const;
		const std::string& getClassName() const;

		//construct with data representing private/internal represenation.
		explicit FormulaObject(variant data);
		virtual ~FormulaObject();

		ffl::IntrusivePtr<FormulaObject> clone() const;

		void validate() const;

		std::string write_id() const;

		void surrenderReferences(GarbageCollector* collector) override;
		std::string debugObjectName() const override;

		variant_type_ptr getPropertySetType(const std::string& key) const;

		bool getConstantValue(const std::string& id, variant* result) const override;

#if defined(USE_LUA)
		const ffl::IntrusivePtr<lua::LuaContext> & get_lua_context() const {
			return lua_ptr_;
		}
#endif
	private:
		//construct with type and constructor parameters.
		//Don't call directly, use create() instead.
		explicit FormulaObject(const std::string& type, variant args=variant());
		void callConstructors(variant args);

		variant serializeToWml() const override;

		variant getValue(const std::string& key) const override;
		variant getValueBySlot(int slot) const override;
		void setValue(const std::string& key, const variant& value) override;
		void setValueBySlot(int slot, const variant& value) override;

		void getInputs(std::vector<FormulaInput>* inputs) const override;

		bool new_in_update_;
		bool orphaned_;

		ffl::IntrusivePtr<game_logic::FormulaCallable> builtin_base_;

		//overrides of the class's read-only properties.
		std::vector<FormulaPtr> property_overrides_;

		std::vector<variant> variables_;

		ffl::IntrusivePtr<const FormulaClass> class_;

		// for lua integration
#if defined(USE_LUA)
		void init_lua();
		ffl::IntrusivePtr<lua::LuaContext> lua_ptr_;
#endif

		variant tmp_value_;

		//if this is non-zero, then private_data_ will be exposed via getValue.
		mutable int private_data_;
	};

	typedef ffl::IntrusivePtr<FormulaObject> FormulaObjectPtr;

	bool formula_class_valid(const std::string& type);

	FormulaCallableDefinitionPtr get_library_definition();
	FormulaCallablePtr get_library_object();

	bool can_load_library_instance(const std::string& id);
	FormulaCallablePtr get_library_instance(const std::string& id);

#if defined(USE_LUA)
	class formula_class_unit_test_helper {
	public:
		formula_class_unit_test_helper();
		~formula_class_unit_test_helper();
		void add_class_defn(const std::string & name, const variant & node);
		//friend void TEST_lua_in_ffl_objects();
	};
#endif
}
