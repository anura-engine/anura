#ifndef FORMULA_INTERFACE_HPP_INCLUDED
#define FORMULA_INTERFACE_HPP_INCLUDED

#include <map>
#include <vector>

#include <boost/intrusive_ptr.hpp>
#include <boost/scoped_ptr.hpp>

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "variant.hpp"
#include "variant_type.hpp"

namespace game_logic
{

struct formula_interface_impl;

class formula_interface_instance_factory : public reference_counted_object
{
public:
	virtual ~formula_interface_instance_factory();

	virtual bool all_static_lookups() const = 0;
	virtual variant create(const variant& v) const = 0;
	virtual int getId() const = 0;
};

class formula_interface : public FormulaCallable
{
public:
	struct interface_mismatch_error {
		explicit interface_mismatch_error(const std::string& msg) : msg(msg) {}
		std::string msg;
	};

	explicit formula_interface(const std::map<std::string, variant_type_ptr>& types);
	const std::map<std::string, variant_type_ptr>& get_types() const { return types_; }

	formula_interface_instance_factory* create_factory(variant_type_ptr type) const; //throw interface_mismatch_error
	formula_interface_instance_factory* get_dynamic_factory() const;

	ConstFormulaCallableDefinitionPtr getDefinition() const;

	bool match(const variant& v) const;

	std::string to_string() const;

private:
	variant getValue(const std::string& key) const;

	std::map<std::string, variant_type_ptr> types_;
	std::unique_ptr<formula_interface_impl> impl_;
};

}

typedef boost::intrusive_ptr<game_logic::formula_interface> formula_interface_ptr;
typedef boost::intrusive_ptr<const game_logic::formula_interface> const_formula_interface_ptr;

#endif
