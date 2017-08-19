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

#include "asserts.hpp"
#include "formula_interface.hpp"

#define RAISE_MISMATCH(cond, msg) \
	if(!(cond)) { std::ostringstream s; s << msg; throw FormulaInterface::interface_mismatch_error(s.str()); }

namespace game_logic
{
	FormulaInterfaceInstanceFactory::~FormulaInterfaceInstanceFactory()
	{}

	namespace 
	{
		class dynamic_bound_factory;
		class static_bound_factory;

		class dynamic_interface_instance : public FormulaCallable
		{
		public:
			dynamic_interface_instance(const variant& obj, ffl::IntrusivePtr<const dynamic_bound_factory> parent);
			int id() const;
		private:
			variant getValue(const std::string& key) const override;
			variant getValueBySlot(int slot) const override;
			void setValue(const std::string& key, const variant& value) override;
			void setValueBySlot(int slot, const variant& value) override;

			ffl::IntrusivePtr<const dynamic_bound_factory> factory_;
			variant obj_;
		};

		class static_interface_instance : public FormulaCallable
		{
		public:
			static_interface_instance(const variant& obj, ffl::IntrusivePtr<const static_bound_factory> parent);
			int id() const;
		private:
			variant getValue(const std::string& key) const override;
			variant getValueBySlot(int slot) const override;
			void setValue(const std::string& key, const variant& value) override;
			void setValueBySlot(int slot, const variant& value) override;

			ffl::IntrusivePtr<const static_bound_factory> factory_;
			ffl::IntrusivePtr<FormulaCallable> obj_;
		};

		struct Entry {
			std::string id;
			variant variant_id;
			variant_type_ptr type;
		};

		class dynamic_bound_factory : public FormulaInterfaceInstanceFactory
		{
		public:
			dynamic_bound_factory(const std::vector<Entry>& slots, int id)
			  : slots_(slots), id_(id)
			{}
			virtual bool all_static_lookups() const override { return false; }
			virtual variant create(const variant& v) const override {
				return variant(new dynamic_interface_instance(v, ffl::IntrusivePtr<const dynamic_bound_factory>(this)));
			}

			const std::string& translate_slot(int slot) const {
				ASSERT_LOG(slot >= 0 && static_cast<unsigned>(slot) < slots_.size(), "Illegal slot given to dynamic bound factory: " << slot << " / " << slots_.size());
				return slots_[slot].id;
			}

			int getId() const override { return id_; }

		private:
			std::vector<Entry> slots_;
			int id_;
		};

		class static_bound_factory : public FormulaInterfaceInstanceFactory
		{
		public:
			static_bound_factory(const std::vector<Entry>& slots, variant_type_ptr type, int id)
			  : slots_(slots), id_(id)
			{
				const FormulaCallableDefinition* def = type->getDefinition();
				RAISE_MISMATCH(def != nullptr, "Trying to make an interface out of an invalid type");
				for(const Entry& e : slots) {
					const FormulaCallableDefinition::Entry* entry = def->getEntryById(e.id);
					RAISE_MISMATCH(entry != nullptr, "Type " << type->to_string() << " does not match interface because it does not contain " << e.id);
					RAISE_MISMATCH(entry->variant_type, "Type " << type->to_string() << " does not match interface because " << e.id << " does not have type information");
					RAISE_MISMATCH(variant_types_compatible(e.type, entry->variant_type), "Type " << type->to_string() << " does not match interface because " << e.id << " is a " << entry->variant_type->to_string() << " when a " << e.type->to_string() << " is expected");

					const int nslot = def->getSlot(e.id);
					mapping_.push_back(nslot);
				}
			}
			virtual bool all_static_lookups() const override { return true; }
			virtual variant create(const variant& v) const override {
				return variant(new static_interface_instance(v, ffl::IntrusivePtr<const static_bound_factory>(this)));
			}

			int translate_slot(int slot) const {
				ASSERT_LOG(slot >= 0 && static_cast<unsigned>(slot) < slots_.size(), "Illegal slot given to static bound factory: " << slot << " / " << slots_.size());
				return mapping_[slot];
			}

			int getId() const override { return id_; }
		private:
			std::vector<Entry> slots_;
			std::vector<int> mapping_;
			int id_;
		};

		dynamic_interface_instance::dynamic_interface_instance(const variant& obj, ffl::IntrusivePtr<const dynamic_bound_factory> parent)
		  : obj_(obj), factory_(parent)
		{
		}

		variant dynamic_interface_instance::getValue(const std::string& key) const
		{
			if(obj_.is_callable()) {
				return obj_.as_callable()->queryValue(key);
			} else {
				return obj_[key];
			}
		}

		variant dynamic_interface_instance::getValueBySlot(int slot) const
		{
			const std::string& key = factory_->translate_slot(slot);
			if(obj_.is_callable()) {
				return obj_.as_callable()->queryValue(key);
			} else {
				return obj_[key];
			}
		}

		void dynamic_interface_instance::setValue(const std::string& key, const variant& value)
		{
			obj_.add_attr_mutation(variant(key), value);
		}

		void dynamic_interface_instance::setValueBySlot(int slot, const variant& value)
		{
			obj_.add_attr_mutation(variant(factory_->translate_slot(slot)), value);
		}

		int dynamic_interface_instance::id() const
		{
			return factory_->getId();
		}

		static_interface_instance::static_interface_instance(const variant& obj, ffl::IntrusivePtr<const static_bound_factory> parent)
		  : obj_(obj.mutable_callable()), factory_(parent)
		{
		}

		variant static_interface_instance::getValue(const std::string& key) const
		{
			return obj_->queryValue(key);
		}

		variant static_interface_instance::getValueBySlot(int slot) const
		{
			return obj_->queryValueBySlot(factory_->translate_slot(slot));
		}

		void static_interface_instance::setValue(const std::string& key, const variant& value)
		{
			obj_->mutateValue(key, value);
		}

		void static_interface_instance::setValueBySlot(int slot, const variant& value)
		{
			obj_->mutateValueBySlot(factory_->translate_slot(slot), value);
		}

		int static_interface_instance::id() const
		{
			return factory_->getId();
		}
	}

	struct FormulaInterfaceImpl
	{
		FormulaInterfaceImpl() {
			static int id_num = 1;
			id_ = id_num++;
		}
		int id_;
		std::vector<Entry> entries_;
		ConstFormulaCallableDefinitionPtr def_;
		ffl::IntrusivePtr<dynamic_bound_factory> dynamic_factory_;
	};

	FormulaInterface::FormulaInterface(const std::map<std::string, variant_type_ptr>& types_map) 
		: types_(types_map), 
		impl_(new FormulaInterfaceImpl())
	{
		ASSERT_LOG(!types_map.empty(), "Empty interface");

		std::vector<std::string> names;
		std::vector<variant_type_ptr> types;
		for(std::map<std::string, variant_type_ptr>::const_iterator i = types_map.begin(); i != types_map.end(); ++i) {
			Entry e = { i->first, variant(i->first), i->second };
			impl_->entries_.push_back(e);
			names.push_back(i->first);
			types.push_back(i->second);
		}

		impl_->def_ = execute_command_callable_definition(&names[0], &names[0] + names.size(), ConstFormulaCallableDefinitionPtr(), &types[0]);

	}

	variant FormulaInterface::getValue(const std::string& key) const
	{
		return variant();
	}

	FormulaInterfaceInstanceFactory* FormulaInterface::createFactory(variant_type_ptr type) const
	{
		if(type->is_interface() == this) {
			return nullptr;
		}

		if(type->is_map_of().first) {
			return getDynamicFactory();
		}

		RAISE_MISMATCH(type->getDefinition(), "Attempt to create interface from non-map type with no definition: " << type->to_string());
		return new static_bound_factory(impl_->entries_, type, impl_->id_);
	}

	FormulaInterfaceInstanceFactory* FormulaInterface::getDynamicFactory() const
	{
		if(!impl_->dynamic_factory_) {
			impl_->dynamic_factory_.reset(new dynamic_bound_factory(impl_->entries_, impl_->id_));
		}

		return impl_->dynamic_factory_.get();
	}

	ConstFormulaCallableDefinitionPtr FormulaInterface::getDefinition() const
	{
		return impl_->def_;
	}

	bool FormulaInterface::match(const variant& v) const
	{
		if(v.is_callable()) {
			const static_interface_instance* static_instance = v.try_convert<static_interface_instance>();
			if(static_instance) {
				return static_instance->id() == impl_->id_;
			} else {
				const dynamic_interface_instance* dynamic_instance = v.try_convert<dynamic_interface_instance>();
				if(dynamic_instance) {
					return dynamic_instance->id() == impl_->id_;
				}
			}
		}

		return false;
	}

	std::string FormulaInterface::to_string() const
	{
		std::ostringstream s;
		s << "interface { ";
		for(int n = 0; n != impl_->entries_.size(); ++n) {
			s << impl_->entries_[n].id << ": " << impl_->entries_[n].type->to_string();
			if(n+1 != impl_->entries_.size()) {
				s << ", ";
			}
		}

		s << " }";
		return s.str();
	}
}
