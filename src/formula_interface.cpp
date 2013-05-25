#include "asserts.hpp"
#include "foreach.hpp"
#include "formula_interface.hpp"

#define RAISE_MISMATCH(cond, msg) \
	if(!(cond)) { std::ostringstream s; s << msg; throw formula_interface::interface_mismatch_error(s.str()); }

namespace game_logic
{

formula_interface_instance_factory::~formula_interface_instance_factory()
{}

namespace {

class dynamic_bound_factory;
class static_bound_factory;

class dynamic_interface_instance : public formula_callable
{
public:
	dynamic_interface_instance(const variant& obj, boost::intrusive_ptr<const dynamic_bound_factory> parent);
	int id() const;
private:
	variant get_value(const std::string& key) const;
	variant get_value_by_slot(int slot) const;
	void set_value(const std::string& key, const variant& value);
	void set_value_by_slot(int slot, const variant& value);

	boost::intrusive_ptr<const dynamic_bound_factory> factory_;
	variant obj_;
};

class static_interface_instance : public formula_callable
{
public:
	static_interface_instance(const variant& obj, boost::intrusive_ptr<const static_bound_factory> parent);
	int id() const;
private:
	variant get_value(const std::string& key) const;
	variant get_value_by_slot(int slot) const;
	void set_value(const std::string& key, const variant& value);
	void set_value_by_slot(int slot, const variant& value);

	boost::intrusive_ptr<const static_bound_factory> factory_;
	boost::intrusive_ptr<formula_callable> obj_;
};

struct Entry {
	std::string id;
	variant variant_id;
	variant_type_ptr type;
};

class dynamic_bound_factory : public formula_interface_instance_factory
{
public:
	dynamic_bound_factory(const std::vector<Entry>& slots, int id)
	  : slots_(slots), id_(id)
	{}
	virtual bool all_static_lookups() const { return false; }
	virtual variant create(const variant& v) const {
		return variant(new dynamic_interface_instance(v, boost::intrusive_ptr<const dynamic_bound_factory>(this)));
	}

	const std::string& translate_slot(int slot) const {
		ASSERT_LOG(slot >= 0 && slot < slots_.size(), "Illegal slot given to dynamic bound factory: " << slot << " / " << slots_.size());
		return slots_[slot].id;
	}

	int get_id() const { return id_; }

private:
	std::vector<Entry> slots_;
	int id_;
};

class static_bound_factory : public formula_interface_instance_factory
{
public:
	static_bound_factory(const std::vector<Entry>& slots, variant_type_ptr type, int id)
	  : slots_(slots), id_(id)
	{
		const formula_callable_definition* def = type->get_definition();
		RAISE_MISMATCH(def != NULL, "Trying to make an interface out of an invalid type");
		foreach(const Entry& e, slots) {
			const formula_callable_definition::entry* entry = def->get_entry_by_id(e.id);
			RAISE_MISMATCH(entry != NULL, "Type does not match interface because it does not contain " << e.id);
			RAISE_MISMATCH(entry->variant_type, "Type does not match interface because " << e.id << " does not have type information");
			RAISE_MISMATCH(variant_types_compatible(e.type, entry->variant_type), "Type does not match interface because " << e.id << " is a " << entry->variant_type->to_string() << " when a " << e.type->to_string() << " is expected");

			const int nslot = def->get_slot(e.id);
			mapping_.push_back(nslot);
		}
	}
	virtual bool all_static_lookups() const { return true; }
	virtual variant create(const variant& v) const {
		return variant(new static_interface_instance(v, boost::intrusive_ptr<const static_bound_factory>(this)));
	}

	int translate_slot(int slot) const {
		ASSERT_LOG(slot >= 0 && slot < slots_.size(), "Illegal slot given to static bound factory: " << slot << " / " << slots_.size());
		return mapping_[slot];
	}

	int get_id() const { return id_; }
private:
	std::vector<Entry> slots_;
	std::vector<int> mapping_;
	int id_;
};

dynamic_interface_instance::dynamic_interface_instance(const variant& obj, boost::intrusive_ptr<const dynamic_bound_factory> parent)
  : obj_(obj), factory_(parent)
{
}

variant dynamic_interface_instance::get_value(const std::string& key) const
{
	ASSERT_LOG(false, "Illegal dynamic query in interface: " << key);
}

variant dynamic_interface_instance::get_value_by_slot(int slot) const
{
	return obj_[factory_->translate_slot(slot)];
}

void dynamic_interface_instance::set_value(const std::string& key, const variant& value)
{
	ASSERT_LOG(false, "Illegal dynamic query in interface: " << key);
}

void dynamic_interface_instance::set_value_by_slot(int slot, const variant& value)
{
	obj_.add_attr_mutation(variant(factory_->translate_slot(slot)), value);
}

int dynamic_interface_instance::id() const
{
	return factory_->get_id();
}

static_interface_instance::static_interface_instance(const variant& obj, boost::intrusive_ptr<const static_bound_factory> parent)
  : obj_(obj.mutable_callable()), factory_(parent)
{
}

variant static_interface_instance::get_value(const std::string& key) const
{
	ASSERT_LOG(false, "Illegal dynamic query in interface: " << key);
}

variant static_interface_instance::get_value_by_slot(int slot) const
{
	return obj_->query_value_by_slot(factory_->translate_slot(slot));
}

void static_interface_instance::set_value(const std::string& key, const variant& value)
{
	ASSERT_LOG(false, "Illegal dynamic query in interface: " << key);
}

void static_interface_instance::set_value_by_slot(int slot, const variant& value)
{
	obj_->mutate_value_by_slot(factory_->translate_slot(slot), value);
}

int static_interface_instance::id() const
{
	return factory_->get_id();
}

}

struct formula_interface_impl
{
	formula_interface_impl() {
		static int id_num = 1;
		id_ = id_num++;
	}
	int id_;
	std::vector<Entry> entries_;
	const_formula_callable_definition_ptr def_;
	boost::intrusive_ptr<dynamic_bound_factory> dynamic_factory_;
};

formula_interface::formula_interface(const std::map<std::string, variant_type_ptr>& types_map) : impl_(new formula_interface_impl)
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

	impl_->def_ = create_formula_callable_definition(&names[0], &names[0] + names.size(), const_formula_callable_definition_ptr(), &types[0]);

}

variant formula_interface::get_value(const std::string& key) const
{
	return variant();
}

formula_interface_instance_factory* formula_interface::create_factory(variant_type_ptr type) const
{
	if(type->is_map_of().first) {
		return get_dynamic_factory();
	}

	RAISE_MISMATCH(type->get_definition(), "Attempt to create interface from non-map type with no definition: " << type->to_string());
	return new static_bound_factory(impl_->entries_, type, impl_->id_);
}

formula_interface_instance_factory* formula_interface::get_dynamic_factory() const
{
	if(!impl_->dynamic_factory_) {
		impl_->dynamic_factory_.reset(new dynamic_bound_factory(impl_->entries_, impl_->id_));
	}

	return impl_->dynamic_factory_.get();
}

const_formula_callable_definition_ptr formula_interface::get_definition() const
{
	return impl_->def_;
}

bool formula_interface::match(const variant& v) const
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

std::string formula_interface::to_string() const
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
