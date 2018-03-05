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
#include "custom_object.hpp"
#include "custom_object_type.hpp"
#include "formula_function.hpp"
#include "formula_interface.hpp"
#include "formula_object.hpp"
#include "formula_tokenizer.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"
#include "variant_type.hpp"
//#include "voxel_object.hpp"
//#include "voxel_object_type.hpp"

variant_type::variant_type() : FormulaCallable(GARBAGE_COLLECTOR_EXCLUDE)
{
}

variant_type::~variant_type()
{
}

void variant_type::set_expr(const game_logic::FormulaExpression* expr) const
{
	expr_ .reset(expr);
}

ffl::IntrusivePtr<const game_logic::FormulaExpression> variant_type::get_expr() const
{
	return expr_.get();
}

namespace {

std::set<std::string> g_generic_variant_names;

std::map<std::string, std::vector<variant> > get_builtin_variant_info()
{
	std::map<std::string, std::vector<variant> > result;
	result["Numeric"] = std::vector<variant>(1, variant("int|decimal"));
	result["Vec2"] = std::vector<variant>(1, variant("[numeric,numeric]"));
	result["Vec3"] = std::vector<variant>(1, variant("[numeric,numeric,numeric]"));
	return result;
}

std::map<std::string, std::vector<variant> > load_named_variant_info()
{
	std::map<std::string, std::vector<variant> > result = get_builtin_variant_info();

	std::map<std::string, std::string> definition_file;

	std::vector<std::pair<std::string, variant> > extends;

	const std::string path = module::map_file("data/types.cfg");
	if(sys::file_exists(path)) {
		variant node = json::parse_from_file(path);
		for(const variant::map_pair& p : node.as_map()) {
			std::string key = p.first.as_string();

			auto colon = std::find(key.begin(), key.end(), ':');
			if(colon != key.end()) {
				std::string directive = std::string(key.begin(), colon);
				key.erase(key.begin(), colon+1);

				if(directive == "extends") {
					extends.push_back(std::pair<std::string,variant>(key, p.second));
				} else {
					ASSERT_LOG(false, "Unknown type directive: " << directive << ":" << key);
				}
			} else {
				result[key].push_back(p.second);
				definition_file[key] = "data/types.cfg";
			}
		}
	}

	std::vector<std::string> files;
	module::get_files_in_dir("data/types", &files);
	for(const std::string& f : files) {
		if(f.size() <= 4 || !std::equal(f.end()-4, f.end(), ".cfg") || f[0] == '.') {
			continue;
		}

		const std::string path = module::map_file("data/types/" + f);
		variant node = json::parse_from_file(path);
		for(const variant::map_pair& p : node.as_map()) {
			ASSERT_LOG(result.count(p.first.as_string()) == 0, "Multiple definition of type " << p.first.as_string() << " defined in " << definition_file[p.first.as_string()] << " and " << path);
			std::string key = p.first.as_string();
			auto colon = std::find(key.begin(), key.end(), ':');
			if(colon != key.end()) {
				std::string directive = std::string(key.begin(), colon);
				key.erase(key.begin(), colon+1);

				if(directive == "extends") {
					extends.push_back(std::pair<std::string,variant>(key, p.second));
				} else {
					ASSERT_LOG(false, "Unknown type directive: " << directive << ":" << key);
				}
			} else {
				result[key].push_back(p.second);
				definition_file[key] = path;
			}
		}
	}

	for(auto p : extends) {
		auto itor = result.find(p.first);
		ASSERT_LOG(itor != result.end(), "Type extension when base type not found: " << p.first);

		itor->second.push_back(p.second);
	}

	return result;
}

std::vector<std::map<std::string, variant_type_ptr> >& named_type_cache()
{
	static std::vector<std::map<std::string, variant_type_ptr> > instance(1);
	return instance;
}

std::vector<std::map<std::string, std::vector<variant> > >& named_type_symbols()
{
	static std::vector<std::map<std::string, std::vector<variant> > > instance(1, load_named_variant_info());
	return instance;
}

variant_type_ptr get_unified_variant_type(const std::vector<variant>& defs)
{
	variant_type_ptr result = parse_variant_type(defs[0]);

	for(int i = 1; i < static_cast<int>(defs.size()); ++i) {
		variant_type_ptr extension = parse_variant_type(defs[i]);
		result = result->extend_type(extension);
		ASSERT_LOG(result, "Could not extend variant type: " << defs[0].write_json() << " with " << defs[i].write_json());
	}

	return result;
}

variant_type_ptr get_named_variant_type(const std::string& name)
{
	for(auto n = named_type_cache().size(); n > 0; --n) {
		std::map<std::string, std::vector<variant> > & info = named_type_symbols()[n-1];
		std::map<std::string, variant_type_ptr>& cache = named_type_cache()[n-1];

		std::map<std::string,variant_type_ptr>::const_iterator itor = cache.find(name);
		if(itor != cache.end()) {
			return itor->second;
		}

		auto info_itor = info.find(name);
		if(info_itor != info.end()) {
			//insert into the cache a null entry to symbolize we're parsing
			//this, to avoid infinite recursion.
			variant_type_ptr& ptr = cache[name];
			ptr = get_unified_variant_type(info_itor->second);
			return ptr;
		}
	}

	return variant_type_ptr();

}

}

types_cfg_scope::types_cfg_scope(variant v)
{
	ASSERT_LOG(v.is_null() || v.is_map(), "Unrecognized types definition: " << v.write_json() << " " << v.debug_location());
	std::map<std::string, std::vector<variant> > symbols;
	if(v.is_map()) {
		for(const variant::map_pair& p : v.as_map()) {
			symbols[p.first.as_string()].push_back(p.second);
		}
	}
	named_type_cache().resize(named_type_cache().size()+1);
	named_type_symbols().push_back(symbols);
}

types_cfg_scope::~types_cfg_scope()
{
	named_type_cache().pop_back();
	named_type_symbols().pop_back();
}

namespace {

class variant_type_simple : public variant_type
{
public:
	variant_type_simple(const variant& original_str, const formula_tokenizer::Token& tok)
	  : type_(variant::string_to_type(std::string(tok.begin, tok.end)))
	{
		ASSERT_LOG(type_ != variant::VARIANT_TYPE_INVALID, "INVALID TYPE: " << std::string(tok.begin, tok.end) << " AT:\n" << game_logic::pinpoint_location(original_str, tok.begin, tok.end));
	}

	explicit variant_type_simple(variant::TYPE type) : type_(type) {}

	bool match(const variant& v) const override {
		return v.type() == type_ || (type_ == variant::VARIANT_TYPE_DECIMAL && v.type() == variant::VARIANT_TYPE_INT);
	}

	bool is_type(variant::TYPE type) const override {
		return type == type_;
	}

	bool is_numeric() const override {
		return type_ == variant::VARIANT_TYPE_DECIMAL || type_ == variant::VARIANT_TYPE_INT;
	}

	bool is_equal(const variant_type& o) const override {
		const variant_type_simple* other = dynamic_cast<const variant_type_simple*>(&o);
		if(!other) {
			return false;
		}

		return type_ == other->type_;
	}

	std::string to_string_impl() const override {
		return variant::variant_type_to_string(type_);
	}

	variant_type_ptr is_list_of() const override {
		if(type_ == variant::VARIANT_TYPE_LIST) {
			return variant_type::get_any();
		} else {
			return variant_type_ptr();
		}
	}

	std::pair<variant_type_ptr, variant_type_ptr> is_map_of() const override {
		if(type_ == variant::VARIANT_TYPE_MAP) {
			return std::pair<variant_type_ptr, variant_type_ptr>(variant_type::get_any(), variant_type::get_any());
		} else {
			return std::pair<variant_type_ptr, variant_type_ptr>();
		}
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		const variant_type_simple* simple_type = dynamic_cast<const variant_type_simple*>(type.get());
		if(simple_type && simple_type->type_ == type_) {
			return true;
		}

		if(type_ == variant::VARIANT_TYPE_DECIMAL) {
			if(simple_type && simple_type->type_ == variant::VARIANT_TYPE_INT) { 
				return true;
			}
		} else if(type_ == variant::VARIANT_TYPE_LIST) {
			if(type->is_list_of()) {
				return true;
			}
		} else if(type_ == variant::VARIANT_TYPE_MAP) {
			if(type->is_map_of().first) {
				return true;
			}
		} else if(type_ == variant::VARIANT_TYPE_FUNCTION) {
			if(type->is_function(nullptr, nullptr, nullptr)) {
				return true;
			}
		} else if(type_ == variant::VARIANT_TYPE_CALLABLE) {
			if(type->is_builtin() || type->is_custom_object() || type->is_voxel_object() || type->is_class() || type->is_interface()) {
				return true;
			}
		}

		return false;
	}

private:
	variant::TYPE type_;
};

class variant_type_none : public variant_type
{
public:
	bool match(const variant& v) const override { return false; }
	bool is_equal(const variant_type& o) const override {
		const variant_type_none* other = dynamic_cast<const variant_type_none*>(&o);
		return other != nullptr;
	}

	std::string to_string_impl() const override {
		return "none";
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		return false;
	}

	bool maybe_convertible_to(variant_type_ptr type) const override {
		return false;
	}

	bool is_none() const override { return true; }
private:
};

class variant_type_any : public variant_type
{
public:
	bool match(const variant& v) const override { return true; }
	bool is_equal(const variant_type& o) const override {
		const variant_type_any* other = dynamic_cast<const variant_type_any*>(&o);
		return other != nullptr;
	}

	std::string to_string_impl() const override {
		return "any";
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		return true;
	}

	bool maybe_convertible_to(variant_type_ptr type) const override {
		return true;
	}

	bool is_any() const override { return true; }
private:
};

class variant_type_commands : public variant_type
{
public:
	bool match(const variant& v) const override {
		if(v.is_null()) {
			return true;
		}

		if(v.is_callable()) {
			return v.as_callable()->isCommand();
		}

		if(v.is_list()) {
			for(int n = 0; n != v.num_elements(); ++n) {
				if(!match(v[n])) {
					return false;
				}
			}

			return true;
		}

		return false;
	}
	bool is_equal(const variant_type& o) const override {
		const variant_type_commands* other = dynamic_cast<const variant_type_commands*>(&o);
		return other != nullptr;
	}

	std::string to_string_impl() const override {
		return "commands";
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		if(type->is_type(variant::VARIANT_TYPE_NULL)) {
			return true;
		}

		variant_type_ptr list_type = type->is_list_of();
		if(list_type) {
			return variant_types_compatible(get_commands(), list_type);
		}

		return is_equal(*type);
	}
private:
};

class variant_type_cairo_commands : public variant_type
{
public:
	bool match(const variant& v) const override {
		if(v.is_null()) {
			return true;
		}

		if(v.is_callable()) {
			return v.as_callable()->isCairoOp();
		}

		if(v.is_list()) {
			for(int n = 0; n != v.num_elements(); ++n) {
				if(!match(v[n])) {
					return false;
				}
			}

			return true;
		}

		return false;
	}
	bool is_equal(const variant_type& o) const override {
		const variant_type_cairo_commands* other = dynamic_cast<const variant_type_cairo_commands*>(&o);
		return other != nullptr;
	}

	std::string to_string_impl() const override {
		return "cairo_commands";
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		if(type->is_type(variant::VARIANT_TYPE_NULL)) {
			return true;
		}

		if(type->to_string() == "cairo_op") {
			return true;
		}

		variant_type_ptr list_type = type->is_list_of();
		if(list_type) {
			return variant_types_compatible(get_cairo_commands(), list_type);
		}

		return is_equal(*type);
	}
private:
};

class variant_type_class : public variant_type
{
public:
	explicit variant_type_class(const std::string& type) : type_(type)
	{
		ASSERT_LOG(game_logic::formula_class_valid(type), "INVALID FORMULA CLASS: " << type);
	}

	bool match(const variant& v) const override {
		const game_logic::FormulaObject* obj = v.try_convert<game_logic::FormulaObject>();
		if(!obj) {
			return false;
		}

		return obj->isA(type_);
	}

	bool is_class(std::string* class_name) const override {
		if(class_name) {
			*class_name = type_;
		}

		return true;
	}

	bool is_equal(const variant_type& o) const override {
		const variant_type_class* other = dynamic_cast<const variant_type_class*>(&o);
		if(!other) {
			return false;
		}

		return type_ == other->type_;
	}

	std::string to_string_impl() const override {
		return "class " + type_;
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		const variant_type_class* class_type = dynamic_cast<const variant_type_class*>(type.get());
		if(class_type) {
			return game_logic::is_class_derived_from(class_type->type_, type_);
		} else if(type->is_type(variant::VARIANT_TYPE_MAP)) {
			//maps can be converted implicity to class type.
			return true;
		}

		return false;
	}

	const game_logic::FormulaCallableDefinition* getDefinition() const override {
		return game_logic::get_class_definition(type_).get();
	}
private:
	std::string type_;
};

class variant_type_custom_object : public variant_type
{
public:
	explicit variant_type_custom_object(const std::string& type) : type_(type), type_index_(type_ == "" ? -1 : CustomObjectType::getObjectTypeIndex(type))
	{
	}

	bool match(const variant& v) const override {
		const CustomObject* obj = v.try_convert<CustomObject>();
		if(!obj) {
			return false;
		}

		return type_index_ == -1 || obj->isA(type_index_);
	}

	bool is_equal(const variant_type& o) const override {
		const variant_type_custom_object* other = dynamic_cast<const variant_type_custom_object*>(&o);
		if(!other) {
			return false;
		}

		return type_ == other->type_;
	}

	std::string to_string_impl() const override {
		if(type_ == "") {
			return "custom_obj";
		}

		return "obj " + type_;
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		const variant_type_custom_object* other = dynamic_cast<const variant_type_custom_object*>(type.get());
		if(other == nullptr) {
			return false;
		}

		return type_ == "" || CustomObjectType::isDerivedFrom(type_, other->type_);
	}

	const game_logic::FormulaCallableDefinition* getDefinition() const override {
		if(type_ == "") {
			return &CustomObjectCallable::instance();
		}

		const game_logic::FormulaCallableDefinition* def = CustomObjectType::getDefinition(type_).get();
		ASSERT_LOG(def, "Could not find custom object: " << type_);
		return def;
	}

	const std::string* is_custom_object() const override { return &type_; }
private:
	std::string type_;
	int type_index_;
};

/* XXX -- rework
class variant_type_voxel_object : public variant_type
{
public:
	explicit variant_type_voxel_object(const std::string& type) : type_(type)
	{
		assert(type.empty() == false);
	}

	bool match(const variant& v) const {
		const voxel_object* obj = v.try_convert<voxel_object>();
		if(!obj) {
			return false;
		}

		return type_ == "" || obj->isA(type_);
	}

	bool is_equal(const variant_type& o) const {
		const variant_type_voxel_object* other = dynamic_cast<const variant_type_voxel_object*>(&o);
		if(!other) {
			return false;
		}

		return type_ == other->type_;
	}

	std::string to_string_impl() const {
		if(type_ == "") {
			return "voxel_object";
		}

		return "vox " + type_;
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const {
		const variant_type_voxel_object* other = dynamic_cast<const variant_type_voxel_object*>(type.get());
		if(other == nullptr) {
			return false;
		}

		return type_ == "" || voxel_object_type::isDerivedFrom(type_, other->type_);
	}

	const game_logic::FormulaCallableDefinition* getDefinition() const {
		if(type_ == "") {
			return variant_type::get_builtin("voxel_object")->getDefinition();
		}

		const game_logic::FormulaCallableDefinition* def = voxel_object_type::getDefinition(type_).get();
		ASSERT_LOG(def, "Could not find custom object: " << type_);
		return def;
	}

	const std::string* is_voxel_object() const { return &type_; }
private:
	std::string type_;
};
*/

class variant_type_builtin : public variant_type
{
public:
	variant_type_builtin(const std::string& type, game_logic::ConstFormulaCallableDefinitionPtr def) : type_(type), def_(def)
	{
	}

	bool match(const variant& v) const override {
		const game_logic::FormulaCallable* obj = v.try_convert<game_logic::FormulaCallable>();
		if(!obj) {
			return false;
		}

		return game_logic::registered_definition_is_a(obj->queryId(), type_);
	}

	bool is_equal(const variant_type& o) const override {
		const variant_type_builtin* other = dynamic_cast<const variant_type_builtin*>(&o);
		if(!other) {
			return false;
		}

		return type_ == other->type_;
	}

	std::string to_string_impl() const override {
		return type_;
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		if(is_equal(*type)) {
			return true;
		}

		const std::string* builtin = type->is_builtin();
		if(builtin && game_logic::registered_definition_is_a(*builtin, type_)) {
			return true;
		}

		return false;
	}

	const game_logic::FormulaCallableDefinition* getDefinition() const override {
		if(!def_) {
			game_logic::ConstFormulaCallableDefinitionPtr def = game_logic::get_formula_callable_definition(type_);
			ASSERT_LOG(def, "Could not find builtin type definition: " << type_);
			def_ = def;
		}
		return def_.get();
	}

	const std::string* is_builtin() const override { return &type_; }
private:
	std::string type_;
	mutable game_logic::ConstFormulaCallableDefinitionPtr def_;
};

class variant_type_interface : public variant_type
{
public:
	explicit variant_type_interface(ConstFormulaInterfacePtr i)
	  : interface_(i)
	{}

	bool match(const variant& v) const override {
		return interface_->match(v);
	}

	bool is_equal(const variant_type& o) const override {
		const variant_type_interface* other = dynamic_cast<const variant_type_interface*>(&o);
		return other && other->interface_ == interface_;
	}

	std::string to_string_impl() const override {
		return interface_->to_string();
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		const game_logic::FormulaInterface* interface_a = is_interface();
		const game_logic::FormulaInterface* interface_b = type->is_interface();
		if(interface_a == interface_b) {
			return true;
		}

		if(interface_a && interface_b) {
			//Right now compatibility between two interfaces requires an
			//exact, complete match.
			const auto& a = interface_a->get_types();
			const auto& b = interface_b->get_types();
			auto i_a = a.begin();
			auto i_b = b.begin();
			while(i_a != a.end() && i_b != b.end()) {
				if(i_a->first != i_b->first) {
					return false;
				}

				if(!variant_types_compatible(i_a->second, i_b->second)) {
					return false;
				}

				++i_a;
				++i_b;
			}

			if(i_a == a.end() && i_b == b.end()) {
				return true;
			}
		}

		return false;
		//TODO: is this the right thing to do? Interfaces aren't considered
		//compatible types with anything since they can only be used in places
		//where conversions explicitly occur.
		/*
		if(type->is_map_of().first) {
			const std::map<variant, variant_type_ptr>* spec = type->is_specific_map();
			if(!spec) {
				return false;
			}
			const std::map<std::string, variant_type_ptr>& interface = interface_->get_types();
			if(spec) {
				for(std::map<std::string, variant_type_ptr>::const_iterator i = interface.begin(); i != interface.end(); ++i) {
					std::map<variant, variant_type_ptr>::const_iterator entry = spec->find(variant(i->first));
					if(entry == spec->end()) {
						if(!may_be_null(i->second)) {
							return false;
						}
					} else {
						if(!variant_types_compatible(i->second, entry->second)) {
							return false;
						}
					}
				}

				return true;
			}
			return false;
		}

		try {
			interface_->create_factory(type);
			return true;
		} catch(game_logic::Formula_interface::interface_mismatch_error&) {
			return false;
		}*/
	}

	const game_logic::FormulaCallableDefinition* getDefinition() const override {
		return interface_->getDefinition().get();
	}

	const game_logic::FormulaInterface* is_interface() const override {
		return interface_.get();
	}
private:
	ConstFormulaInterfacePtr interface_;
};

class variant_type_union : public variant_type
{
public:
	explicit variant_type_union(const std::vector<variant_type_ptr>& v) : types_(v)
	{}
	bool match(const variant& v) const override {
		for(const variant_type_ptr& p : types_) {
			if(p->match(v)) {
				return true;
			}
		}

		return false;
	}

	bool is_numeric() const override {
		if(types_.empty()) {
			return false;
		}

		for(const variant_type_ptr& p : types_) {
			if(p->is_numeric() == false) {
				return false;
			}
		}

		return true;
	}

	bool is_equal(const variant_type& o) const override {
		const variant_type_union* other = dynamic_cast<const variant_type_union*>(&o);
		if(!other) {
			return false;
		}

		if(types_.size() != other->types_.size()) {
			return false;
		}

		for(int n = 0; n != types_.size(); ++n) {
			if(types_[n]->is_equal(*other->types_[n]) == false) {
				return false;
			}
		}

		return true;
	}

	std::string to_string_impl() const override {
		std::string result;
		if(types_.empty()) {
			result = "(empty union)";
		}
		for(int n = 0; n != types_.size(); ++n) {
			if(n != 0) {
				result += "|";
			}

			result += types_[n]->to_string();
		}
		return result;
	}

	bool is_function(std::vector<variant_type_ptr>* args, variant_type_ptr* return_type, int* min_args, bool* return_type_specified) const override
	{
		std::vector<std::vector<variant_type_ptr>> arg_lists(types_.size());
		std::vector<variant_type_ptr> return_types(types_.size());
		std::vector<int> min_args_list(types_.size());

		if(return_type_specified) {
			*return_type_specified = true;
		}

		int max_min_args = -1;
		std::vector<std::vector<variant_type_ptr>>::size_type num_args = 0;
		for(std::vector<variant_type_ptr>::size_type n = 0; n != types_.size(); ++n) {
			bool return_type_spec = false;
			if(!types_[n]->is_function(&arg_lists[n], &return_types[n], &min_args_list[n], &return_type_spec)) {
				return false;
			}

			if(return_type_specified && !return_type_spec) {
				*return_type_specified = false;
			}

			if(max_min_args == -1 || min_args_list[n] > max_min_args) {
				max_min_args = min_args_list[n];
			}

			if(arg_lists[n].size() > static_cast<unsigned>(num_args)) {
				num_args = arg_lists[n].size();
			}
		}

		if(args) {
			args->clear();
			for(unsigned n = 0; n != num_args; ++n) {
				std::vector<variant_type_ptr> a;
				for(const std::vector<variant_type_ptr>& arg : arg_lists) {
					if(n < arg.size()) {
						a.push_back(arg[n]);
					}
				}

				args->push_back(get_union(a));
			}
		}

		if(return_type) {
			*return_type = get_union(return_types);
		}

		if(min_args) {
			*min_args = max_min_args;
		}

		return true;
	}

	const std::vector<variant_type_ptr>* is_union() const override { return &types_; }

	variant_type_ptr is_list_of() const override {
		std::vector<variant_type_ptr> types;
		for(const variant_type_ptr& type : types_) {
			types.push_back(type->is_list_of());
			if(!types.back()) {
				return variant_type_ptr();
			}
		}

		return get_union(types);
	}

	const std::map<variant, variant_type_ptr>* is_specific_map() const override {
		if(specific_map_.get() != nullptr) {
			return specific_map_.get();
		}

		for(const auto& t : types_) {
			if(!t->is_specific_map()) {
				return nullptr;
			}
		}

		specific_map_.reset(new std::map<variant, variant_type_ptr>);
		for(const auto& t : types_) {
			auto m = t->is_specific_map();
			for(const auto& p : *m) {
				auto itor = specific_map_->find(p.first);
				if(itor != specific_map_->end()) {
					std::vector<variant_type_ptr> u;
					u.push_back(itor->second);
					u.push_back(p.second);
					itor->second = get_union(u);
				} else {
					specific_map_->insert(p);
				}
			}
		}

		return specific_map_.get();
	}
	

	std::pair<variant_type_ptr,variant_type_ptr> is_map_of() const override {
		std::vector<variant_type_ptr> key_types, value_types;
		for(const variant_type_ptr& type : types_) {
			key_types.push_back(type->is_map_of().first);
			value_types.push_back(type->is_map_of().second);
			if(!key_types.back()) {
				return std::pair<variant_type_ptr,variant_type_ptr>();
			}
		}

		return std::pair<variant_type_ptr,variant_type_ptr>(get_union(key_types), get_union(value_types));
	}

	variant_type_ptr base_type_no_enum() const override {
		std::vector<variant_type_ptr> result;
		bool is_different = false;
		for(const variant_type_ptr& t : types_) {
			result.push_back(t->base_type_no_enum());
			if(result.back() != t) {
				is_different = true;
			}
		}

		if(is_different == false) {
			return variant_type_ptr(this);
		}

		return get_union(result);
	}
private:
	variant_type_ptr null_excluded() const override {
		std::vector<variant_type_ptr> new_types;
		for(variant_type_ptr t : types_) {
			if(t->is_type(variant::VARIANT_TYPE_NULL) == false) {
				new_types.push_back(t);
			}
		}

		if(new_types.size() != types_.size()) {
			return get_union(new_types);
		} else {
			return variant_type_ptr();
		}
	}

	variant_type_ptr subtract(variant_type_ptr type) const override {
		std::vector<variant_type_ptr> new_types;
		for(variant_type_ptr t : types_) {
			if(t->is_equal(*type) == false) {
				new_types.push_back(t);
			}
		}

		if(new_types.size() != types_.size()) {
			return get_union(new_types);
		} else {
			return variant_type_ptr();
		}
	}

	std::vector<variant_type_ptr> types_;

	mutable std::shared_ptr<std::map<variant, variant_type_ptr> > specific_map_;
};

class variant_type_list : public variant_type
{
public:
	explicit variant_type_list(const variant_type_ptr& value) : value_type_(value)
	{
		assert(value);
	}

	bool match(const variant& v) const override {
		if(!v.is_list()) {
			return false;
		}

		for(int n = 0; n != v.num_elements(); ++n) {
			if(!value_type_->match(v[n])) {
				return false;
			}
		}

		return true;
	}

	bool is_equal(const variant_type& o) const override {
		const variant_type_list* other = dynamic_cast<const variant_type_list*>(&o);
		if(!other) {
			return false;
		}

		return value_type_->is_equal(*other->value_type_);
	}

	std::string to_string_impl() const override {
		return "[" + value_type_->to_string() + "]";
	}

	variant_type_ptr is_list_of() const override {
		return value_type_;
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		variant_type_ptr value_type = type->is_list_of();
		if(value_type) {
			bool result = variant_types_compatible(value_type_, value_type);
			if(!result && why != NULL && type->is_specific_list()) {
				variant_type_ptr mismatching_element;
				for(variant_type_ptr eltype : *type->is_specific_list()) {
					if(variant_types_compatible(value_type_, eltype) == false) {
						if(mismatching_element.get() == NULL) {
							mismatching_element = eltype;
						} else {
							mismatching_element = variant_type_ptr();
							break;
						}
					}
				}

				if(mismatching_element.get() != NULL) {
					*why << "Element of list does not match: " << mismatching_element->to_string() << "\nExpected " << value_type_->to_string() << "\n";
					if(mismatching_element->get_expr() && mismatching_element->get_expr()->hasDebugInfo()) {
						*why << mismatching_element->get_expr()->debugPinpointLocation() << "\n";
					}

					variant_types_compatible(get_null_excluded(value_type_), get_null_excluded(mismatching_element), why);
					
				}
			}

			return result;
		}

		if(type->is_type(variant::VARIANT_TYPE_LIST)) {
			return variant_types_compatible(value_type_, variant_type::get_any());
		}

		return false;

	}
private:
	variant_type_ptr value_type_;
};

class variant_type_specific_list : public variant_type
{
public:
	explicit variant_type_specific_list(const std::vector<variant_type_ptr>& value) : value_(value)
	{
		list_ = get_union(value);
	}

	bool match(const variant& v) const override {
		if(!v.is_list()) {
			return false;
		}

		if(v.num_elements() != value_.size()) {
			return false;
		}

		for(int n = 0; n != v.num_elements(); ++n) {
			if(!value_[n]->match(v[n])) {
				return false;
			}
		}

		return true;
	}

	bool is_equal(const variant_type& o) const override {
		const variant_type_specific_list* other = dynamic_cast<const variant_type_specific_list*>(&o);
		if(!other) {
			return false;
		}

		if(value_.size() != other->value_.size()) {
			return false;
		}

		for(int n = 0; n != value_.size(); ++n) {
			if(value_[n]->is_equal(*other->value_[n]) == false) {
				return false;
			}
		}

		return true;
	}

	std::string to_string_impl() const override {
		std::ostringstream s;
		s << "[";
		for(variant_type_ptr t : value_) {
			s << t->to_string() << ",";
		}
		s << "]";
		return s.str();
	}

	variant_type_ptr is_list_of() const override {
		return list_;
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		if(is_equal(*type)) {
			return true;
		}

		const variant_type_specific_list* other = dynamic_cast<const variant_type_specific_list*>(type.get());
		if(!other || other->value_.size() != value_.size()) {
			return false;
		}

		for(int n = 0; n != value_.size(); ++n) {
			if(!variant_types_compatible(value_[n], other->value_[n])) {
				return false;
			}
		}

		return true;
	}

	const std::vector<variant_type_ptr>* is_specific_list() const override { return &value_; }
private:
	variant_type_ptr list_;
	std::vector<variant_type_ptr> value_;
};

class variant_type_map : public variant_type
{
public:
	variant_type_map(variant_type_ptr key, variant_type_ptr value)
	  : key_type_(key), value_type_(value)
	{}

	bool match(const variant& v) const override {
		if(!v.is_map()) {
			return false;
		}

		for(const variant::map_pair& p : v.as_map()) {
			if(!key_type_->match(p.first) || !value_type_->match(p.second)) {
				return false;
			}
		}

		return true;
	}

	bool is_equal(const variant_type& o) const override {
		const variant_type_map* other = dynamic_cast<const variant_type_map*>(&o);
		if(!other) {
			return false;
		}

		return value_type_->is_equal(*other->value_type_) &&
		       key_type_->is_equal(*other->key_type_);
	}
	std::string to_string_impl() const override {
		return "{" + key_type_->to_string() + " -> " + value_type_->to_string() + "}";
	}

	std::pair<variant_type_ptr, variant_type_ptr> is_map_of() const override {
		return std::pair<variant_type_ptr, variant_type_ptr>(key_type_, value_type_);
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		std::pair<variant_type_ptr,variant_type_ptr> p = type->is_map_of();
		if(p.first && p.second) {
			// The given type of map can be used as a map of this type if,
			// every key of that type of map can be converted to the key type of this map,
			// and every value of that type of map can be converted to the value type of this map
			return variant_types_compatible(key_type_, p.first) &&
			       variant_types_compatible(value_type_, p.second);
		}

		if(type->is_type(variant::VARIANT_TYPE_LIST)) {
			return variant_types_compatible(key_type_, variant_type::get_any()) &&
			       variant_types_compatible(value_type_, variant_type::get_any());
		}

		return false;

	}

	const game_logic::FormulaCallableDefinition* getDefinition() const override {
		if(!def_) {
			def_ = game_logic::create_map_formula_callable_definition(value_type_);
		}

		return def_.get();
	}
private:
	variant_type_ptr key_type_, value_type_;

	mutable game_logic::FormulaCallableDefinitionPtr def_;
};

class variant_type_specific_map : public variant_type
{
public:
	variant_type_specific_map(const std::map<variant, variant_type_ptr>& type_map, variant_type_ptr key_type, variant_type_ptr value_type)
	  : type_map_(type_map), key_type_(key_type), value_type_(value_type)
	{
		ASSERT_LOG(type_map.empty() == false, "Specific map which is empty");
		std::vector<std::string> keys;
		std::vector<variant_type_ptr> values;
		for(std::map<variant,variant_type_ptr>::const_iterator i = type_map.begin(); i != type_map.end(); ++i) {
			keys.push_back(i->first.as_string());
			values.push_back(i->second);

			if(get_null_excluded(i->second) == i->second) {
				must_have_keys_.insert(i->first);
			}
		}
		def_ = game_logic::execute_command_callable_definition(&keys[0], &keys[0] + keys.size(), game_logic::ConstFormulaCallableDefinitionPtr(), &values[0]);
		def_->setSupportsSlotLookups(false);
	}

	variant_type_ptr extend_type(variant_type_ptr extension) const override {
		auto m = extension->is_specific_map();
		if(!m) {
			return variant_type_ptr();
		}

		auto result = type_map_;

		for(auto p : *m) {
			result[p.first] = p.second;
		}

		return get_specific_map(result);
	}

	bool match(const variant& v) const override {
		if(!v.is_map()) {
			return false;
		}

		for(const variant::map_pair& p : v.as_map()) {
			std::map<variant, variant_type_ptr>::const_iterator itor = type_map_.find(p.first);
			if(itor == type_map_.end()) {
				return false;
			}

			if(!itor->second->match(p.second)) {
				return false;
			}
		}

		for(const variant& k : must_have_keys_) {
			if(v.as_map().count(k) == 0) {
				return false;
			}
		}

		return true;
	}

	std::string mismatch_reason(const variant& v) const override
	{
		if(!v.is_map()) {
			return "Type is not a map";
		}

		std::ostringstream s;

		for(const variant::map_pair& p : v.as_map()) {
			std::map<variant, variant_type_ptr>::const_iterator itor = type_map_.find(p.first);
			if(itor == type_map_.end()) {
				s << "Key " << p.first << " not in type";
				return s.str();
			}

			if(!itor->second->match(p.second)) {
				s << "Value for " << p.first << " does not match";
				return s.str();
			}
		}

		for(const variant& k : must_have_keys_) {
			if(v.as_map().count(k) == 0) {
				s << "Key " << k.write_json() << " required but not found";
				return s.str();
			}
		}

		return "";
	}

	bool is_equal(const variant_type& o) const override {
		const variant_type_specific_map* other = dynamic_cast<const variant_type_specific_map*>(&o);
		if(!other) {
			return false;
		}

		if(type_map_.size() != other->type_map_.size()) {
			return false;
		}

		std::map<variant, variant_type_ptr>::const_iterator i1 = type_map_.begin();
		std::map<variant, variant_type_ptr>::const_iterator i2 = other->type_map_.begin();
		while(i1 != type_map_.end()) {
			if(i1->first != i2->first) {
				return false;
			}

			if(!i1->second->is_equal(*i2->second)) {
				return false;
			}

			++i1;
			++i2;
		}

		return true;
	}
	std::string to_string_impl() const override {
		std::ostringstream s;
		s << "{";
		std::map<variant, variant_type_ptr>::const_iterator i = type_map_.begin();
		while(i != type_map_.end()) {
			if(i->first.is_string()) {
				s << i->first.as_string();
			} else {
				s << i->first.write_json();
			}

			s << ": " << i->second->to_string();
			++i;

			if(i != type_map_.end()) {
				s << ", ";
			}
		}

		s << "}";
		return s.str();
	}

	std::pair<variant_type_ptr, variant_type_ptr> is_map_of() const override {
		return std::pair<variant_type_ptr, variant_type_ptr>(key_type_, value_type_);
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		if(type->is_equal(*this)) {
			return true;
		}

		const variant_type_specific_map* other = dynamic_cast<const variant_type_specific_map*>(type.get());
		if(!other) {
			return false;
		}

		for(std::map<variant, variant_type_ptr>::const_iterator i = type_map_.begin(); i != type_map_.end(); ++i) {
			std::map<variant, variant_type_ptr>::const_iterator other_itor =
			      other->type_map_.find(i->first);
			if(other_itor == other->type_map_.end()) {
				if(must_have_keys_.count(i->first)) {
					if(why) {
						*why << "Required key not present: " << i->first.write_json();
					}
					return false;
				}
			} else {
				if(!variant_types_compatible(i->second, other_itor->second)) {
					if(why) {
						*why << "Key " << i->first.write_json() << " expected " << i->second->to_string() << " but given " << other_itor->second->to_string();
					}
					return false;
				}
			}
		}

		for(std::map<variant, variant_type_ptr>::const_iterator i = other->type_map_.begin(); i != other->type_map_.end(); ++i) {
			if(type_map_.count(i->first) == 0) {
				if(why) {
					*why << "Found unexpected key " << i->first.write_json();
				}
				return false;
			}
		}

		return true;
	}

	const std::map<variant, variant_type_ptr>* is_specific_map() const override { return &type_map_; }

	const game_logic::FormulaCallableDefinition* getDefinition() const override
	{
		return def_.get();
	}
private:
	std::map<variant, variant_type_ptr> type_map_;
	std::set<variant> must_have_keys_;
	variant_type_ptr key_type_, value_type_;
	game_logic::FormulaCallableDefinitionPtr def_;
};

class variant_type_function : public variant_type
{
public:
	variant_type_function(const std::vector<variant_type_ptr>& args,
	                      variant_type_ptr return_type, int min_args)
	  : args_(args), return_(return_type), min_args_(min_args), return_type_specified_(true)
	{
		if(!return_) {
			return_ = get_any();
			return_type_specified_ = false;
		}
	}

	bool is_function(std::vector<variant_type_ptr>* args, variant_type_ptr* return_type, int* min_args, bool* return_type_specified) const override
	{
		if(args) {
			*args = args_;
		}

		if(return_type) {
			*return_type = return_;
		}

		if(min_args) {
			*min_args = min_args_;
		}

		if(return_type_specified) {
			*return_type_specified = return_type_specified_;
		}

		return true;
	}

	bool is_equal(const variant_type& o) const override {
		const variant_type_function* other = dynamic_cast<const variant_type_function*>(&o);
		if(!other) {
			return false;
		}

		if(!return_->is_equal(*other->return_) || args_.size() != other->args_.size()) {
			return false;
		}

		for(int n = 0; n != args_.size(); ++n) {
			if(!args_[n]->is_equal(*other->args_[n])) {
				return false;
			}
		}

		return true;
	}

	std::string to_string_impl() const override {
		std::string result = "function(";
		for(int n = 0; n != args_.size(); ++n) {
			if(n != 0) {
				result += ",";
			}

			result += args_[n]->to_string();
		}

		result += ") -> " + return_->to_string();
		return result;
	}

	bool match(const variant& v) const override {
		if(v.is_function() == false) {
			return false;
		}

		if(variant_types_compatible(return_, v.function_return_type()) == false) {
			return false;
		}

		if(v.max_function_arguments() != args_.size() || v.min_function_arguments() != min_args_) {
			return false;
		}

		const std::vector<variant_type_ptr>& arg_types = v.function_arg_types();
		for(int n = 0; n != arg_types.size(); ++n) {
			if(variant_types_compatible(arg_types[n], args_[n]) == false) {
				return false;
			}
		}

		return true;
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		std::vector<variant_type_ptr> args;
		variant_type_ptr return_type;
		int min_args = 0;
		if(type->is_function(&args, &return_type, &min_args)) {
			if(min_args != min_args_) {
				return false;
			}

			if(!variant_types_compatible(return_, return_type)) {
				return false;
			}

			if(args.size() != args_.size()) {
				return false;
			}

			// Order is args[n], args_[n] here, and return_, return_type above,
			// Because type argument can serve as an instance of this type if,
			// the inputs to this can be used as arguments of type,
			// and the result of type can be used as the return value of this.
			for(int n = 0; n != args_.size(); ++n) {
				if(!variant_types_compatible(args[n], args_[n])) {
					return false;
				}
			}

			return true;
		}

		return false;
	}
	
private:

	std::vector<variant_type_ptr> args_;
	variant_type_ptr return_;
	int min_args_;
	bool return_type_specified_;
};

class variant_type_function_overload : public variant_type
{
public:
	variant_type_function_overload(variant_type_ptr overloaded_fn, const std::vector<variant_type_ptr>& fn) : overloaded_(overloaded_fn), fn_(fn)
	{}

	bool is_function(std::vector<variant_type_ptr>* args, variant_type_ptr* return_type, int* min_args, bool* return_type_specified) const override
	{
		return overloaded_->is_function(args, return_type, min_args, return_type_specified);
	}

	bool match(const variant& v) const override {
		return overloaded_->match(v);
	}

	bool is_equal(const variant_type& o) const override {
		const variant_type_function_overload* f = dynamic_cast<const variant_type_function_overload*>(&o);
		if(!f) {
			return false;
		}

		if(overloaded_->is_equal(*f->overloaded_) == false) {
			return false;
		}

		if(fn_.size() != f->fn_.size()) {
			return false;
		}

		for(int n = 0; n != fn_.size(); ++n) {
			if(fn_[n]->is_equal(*f->fn_[n]) == false) {
				return false;
			}
		}

		return true;
	}

	std::string to_string_impl() const override {
		std::string result = "overload(";
		for(const variant_type_ptr& p : fn_) {
			result += p->to_string() + ",";
		}

		result[result.size()-1] = ')';
		return result;
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		return overloaded_->is_compatible(type);
	}

	variant_type_ptr function_return_type_with_args(const std::vector<variant_type_ptr>& parms) const override
	{
		std::vector<variant_type_ptr> result_types;
		for(const variant_type_ptr& fn : fn_) {
			variant_type_ptr result;
			std::vector<variant_type_ptr> args;
			int min_args = 0;
			if(!fn->is_function(&args, &result, &min_args) 
				|| static_cast<unsigned>(min_args) > args.size() 
				|| parms.size() > args.size()) {
				continue;
			}

			bool maybe_match = true, definite_match = true;
			for(int n = 0; n != parms.size(); ++n) {
				if(variant_types_might_match(args[n], parms[n]) == false) {
					maybe_match = definite_match = false;
					break;
				}

				definite_match = definite_match && variant_types_compatible(args[n], parms[n]);
			}

			if(result_types.empty() && definite_match) {
				return result;
			}

			if(maybe_match) {
				result_types.push_back(result);
			}
		}

		return get_union(result_types);
	}

private:
	variant_type_ptr overloaded_;
	std::vector<variant_type_ptr> fn_;
};

class variant_type_enum : public variant_type
{
public:
	explicit variant_type_enum(const std::vector<std::string>& values) : values_(values)
	{
		std::sort(values_.begin(), values_.end());
	}


	bool match(const variant& v) const override {
		if(v.is_enum()) {
			return std::find(values_.begin(), values_.end(), v.as_enum()) != values_.end();
		}

		return false;
	}

	bool is_type(variant::TYPE type) const override {
		return type == variant::VARIANT_TYPE_ENUM;
	}

	bool is_equal(const variant_type& o) const override {
		const variant_type_enum* other = dynamic_cast<const variant_type_enum*>(&o);
		if(!other) {
			return false;
		}

		return values_ == other->values_;
	}

	bool is_compatible(variant_type_ptr type, std::ostringstream* why) const override {
		const variant_type_enum* other = dynamic_cast<const variant_type_enum*>(type.get());
		return other && is_subset(other->values_);
	}
private:
	std::string to_string_impl() const override {
		return "enum{" + util::join(values_) + "}";
	}

	bool is_subset(const std::vector<std::string>& values) const {
		for(const std::string& s : values) {
			if(std::find(values_.begin(), values_.end(), s) == values_.end()) {
				return false;
			}
		}

		return true;
	}

	std::vector<std::string> values_;
};

class variant_type_generic : public variant_type
{
public:
	explicit variant_type_generic(const std::string& id) : id_(id)
	{}

	bool is_equal(const variant_type& o) const override {
		const variant_type_generic* gen = dynamic_cast<const variant_type_generic*>(&o);
		return gen && gen->id_ == id_;
	}

	variant_type_ptr map_generic_types(const std::map<std::string, variant_type_ptr>& mapping) const override {
		auto itor = mapping.find(id_);
		if(itor != mapping.end()) {
			return itor->second;
		}

		return variant_type_ptr();
	}

	bool match(const variant& v) const override {
		return false;
	}

	bool is_generic(std::string* id) const override {
		if(id) {
			*id = id_;
		}

		return true;
	}
private:
	std::string to_string_impl() const override {
		return id_;
	}

	std::string id_;
};

}

bool variant_type::may_be_null(variant_type_ptr type)
{
	return type->is_any() || variant_type::get_null_excluded(type) != type;
}

variant_type_ptr get_variant_type_from_value(const variant& value) {
	using namespace game_logic;
	if(value.is_enum()) {
		std::vector<std::string> v;
		v.push_back(value.as_enum());
		return variant_type_ptr(new variant_type_enum(v));
	} else if(value.try_convert<FormulaObject>()) {
		return variant_type::get_class(value.try_convert<FormulaObject>()->getClassName());
	} else if(value.try_convert<CustomObject>()) {
		const CustomObject* obj = value.try_convert<CustomObject>();
		return variant_type::get_custom_object(obj->queryValue("type").as_string());
// XXX Removed for re-work
//	} else if(value.try_convert<voxel_object>()) {
//		const voxel_object* obj = value.try_convert<voxel_object>();
//		return variant_type::get_voxel_object(obj->type());
	} else if(value.is_list()) {
		std::vector<variant_type_ptr> types;
		for(const variant& item : value.as_list()) {
			variant_type_ptr new_type = get_variant_type_from_value(item);
			types.push_back(new_type);
		}

		return variant_type::get_specific_list(types);
	} else if(value.is_map()) {

		bool all_string_keys = true;
		for(const variant::map_pair& p : value.as_map()) {
			if(p.first.is_string() == false) {
				all_string_keys = false;
				break;
			}
		}

		if(all_string_keys && !value.as_map().empty()) {
			std::map<variant, variant_type_ptr> type_map;
			for(const variant::map_pair& p : value.as_map()) {
				type_map[p.first] = get_variant_type_from_value(p.second);
			}

			return variant_type::get_specific_map(type_map);
		}

		std::vector<variant_type_ptr> key_types, value_types;

		for(const variant::map_pair& p : value.as_map()) {
			variant_type_ptr new_key_type = get_variant_type_from_value(p.first);
			variant_type_ptr new_value_type = get_variant_type_from_value(p.second);

			for(const variant_type_ptr& existing : key_types) {
				if(existing->is_equal(*new_key_type)) {
					new_key_type.reset();
					break;
				}
			}

			if(new_key_type) {
				key_types.push_back(new_key_type);
			}

			for(const variant_type_ptr& existing : value_types) {
				if(existing->is_equal(*new_value_type)) {
					new_value_type.reset();
					break;
				}
			}

			if(new_value_type) {
				value_types.push_back(new_value_type);
			}
		}

		variant_type_ptr key_type, value_type;

		if(key_types.size() == 1) {
			key_type = variant_type::get_list(key_types[0]);
		} else {
			key_type = variant_type::get_list(variant_type::get_union(key_types));
		}

		if(value_types.size() == 1) {
			value_type = variant_type::get_list(value_types[0]);
		} else {
			value_type = variant_type::get_list(variant_type::get_union(value_types));
		}

		return variant_type::get_map(key_type, value_type);
	} else if(value.is_callable() && value.as_callable()->isCommand()) {
		return variant_type::get_commands();
	} else if(value.is_callable() && game_logic::get_formula_callable_definition(value.as_callable()->queryId())) {
		return variant_type::get_builtin(value.as_callable()->queryId());
	} else if(value.is_function()) {
		return variant_type::get_function_type(value.function_arg_types(), value.function_return_type(), value.min_function_arguments());
	} else {
		return variant_type::get_type(value.type());
	}
}

std::string variant_type_is_class_or_null(variant_type_ptr type)
{
	std::string class_name;
	if(type->is_class(&class_name)) {
		return class_name;
	}

	const std::vector<variant_type_ptr>* union_vec = type->is_union();
	if(union_vec) {
		for(variant_type_ptr t : *union_vec) {
			bool found_class = false;
			if(class_name.empty()) {
				class_name = variant_type_is_class_or_null(t);
				if(class_name.empty() == false) {
					found_class = true;
				}
			}

			if(found_class == false && t->is_type(variant::VARIANT_TYPE_NULL) == false) {
				return "";
			}
		}
	}

	return class_name;
}

bool variant_types_compatible(variant_type_ptr to, variant_type_ptr from, std::ostringstream* why)
{
	if(from->is_union()) {
		for(variant_type_ptr from_type : *from->is_union()) {
			if(variant_types_compatible(to, from_type) == false) {
				return false;
			}
		}

		return true;
	}

	if(to->is_union()) {
		for(variant_type_ptr to_type : *to->is_union()) {
			if(variant_types_compatible(to_type, from)) {
				return true;
			}
		}

		return false;
	}

	return to->is_compatible(from, why);
}

bool variant_types_might_match(variant_type_ptr to, variant_type_ptr from)
{
	if(from->is_union()) {
		for(variant_type_ptr from_type : *from->is_union()) {
			if(variant_types_might_match(to, from_type)) {
				return true;
			}
		}

		return false;
	}

	if(to->is_union()) {
		for(variant_type_ptr to_type : *to->is_union()) {
			if(variant_types_might_match(to_type, from)) {
				return true;
			}
		}

		return false;
	}

	return to->is_compatible(from) || from->is_compatible(to) || from->maybe_convertible_to(to);
}

bool parse_variant_constant(const variant& original_str,
                               const formula_tokenizer::Token*& i1,
                               const formula_tokenizer::Token* i2,
							   bool allow_failure, variant& result)
{
#define ASSERT_COND(cond, msg) if(cond) {} else if(allow_failure) { return false; } else { ASSERT_LOG(cond, msg); }

	using namespace formula_tokenizer;

	const Token* begin = i1;
	const bool res = TokenMatcher().add(FFL_TOKEN_TYPE::COMMA).add(FFL_TOKEN_TYPE::RBRACKET).add(FFL_TOKEN_TYPE::ELLIPSIS).find_match(i1, i2);

	ASSERT_COND(res, "Unexpected end of input while parsing value: " << game_logic::pinpoint_location(original_str, begin->begin));

	std::string formula_str(begin->begin, (i1-1)->end);

	variant formula_var(formula_str);
	try {
		game_logic::Formula f(formula_var);
		result = f.execute();
		return true;
	} catch(...) {
		ASSERT_COND(false, "Could not parse value in enum: " << game_logic::pinpoint_location(original_str, begin->begin));
		return false;
	}

#undef ASSERT_COND
}

variant_type_ptr parse_variant_type(const variant& original_str,
                                    const formula_tokenizer::Token*& i1,
                                    const formula_tokenizer::Token* i2,
									bool allow_failure)
{
#define ASSERT_COND(cond, msg) if(cond) {} else if(allow_failure) { return variant_type_ptr(); } else { ASSERT_LOG(cond, msg); }

	using namespace formula_tokenizer;

	std::vector<variant_type_ptr> v;

	const Token* begin_token = i1;

	for(;;) {
		ASSERT_COND(i1 != i2, "EXPECTED TYPE BUT FOUND EMPTY EXPRESSION:" << original_str.debug_location());
		if(i1->type == FFL_TOKEN_TYPE::CONST_IDENTIFIER || (i1->type == FFL_TOKEN_TYPE::IDENTIFIER && util::c_isupper(*i1->begin) && g_generic_variant_names.count(std::string(i1->begin, i1->end)))) {
			v.push_back(variant_type::get_generic_type(std::string(i1->begin, i1->end)));
			++i1;
		} else if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && util::c_isupper(*i1->begin) && get_named_variant_type(std::string(i1->begin, i1->end))) {
			v.push_back(get_named_variant_type(std::string(i1->begin, i1->end)));
			++i1;
		} else if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && i1->equals("interface") && i1+1 != i2 && (i1+1)->equals("{")) {
			i1 += 2;

			std::map<std::string, variant_type_ptr> types;

			while(i1 != i2 && !i1->equals("}")) {
				if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER) {
					std::string id(i1->begin, i1->end);
					++i1;

					ASSERT_COND(i1 != i2 && i1->equals(":"), "Expected : after " << id << " in interface definition: " << original_str.debug_location());

					++i1;
					variant_type_ptr type = parse_variant_type(original_str, i1, i2, allow_failure);
					if(!type) {
						return variant_type_ptr();
					}

					types[id] = type;

					if(i1 != i2 && i1->equals(",")) {
						++i1;
					}
				} else {
					ASSERT_COND(false, "Expected identifier or } in interface definition" << original_str.debug_location());
				}
			}

			if(i1 != i2) {
				++i1;
			}

			v.push_back(variant_type_ptr(new variant_type_interface(
			    ConstFormulaInterfacePtr(new game_logic::FormulaInterface(types)))));
		} else if(i1->type == FFL_TOKEN_TYPE::KEYWORD && i1->equals("enum")) {
			++i1;
			ASSERT_COND(i1 != i2 && i1->type == FFL_TOKEN_TYPE::LBRACKET, "Expected '{' after enum in type: " << game_logic::pinpoint_location(original_str, (i1-1)->begin));
			++i1;
			std::vector<std::string> enums;

			while(i1 != i2 && i1->type != FFL_TOKEN_TYPE::RBRACKET) {
				ASSERT_COND(i1->type == FFL_TOKEN_TYPE::IDENTIFIER, "Expected identifier after enum in type: " << game_logic::pinpoint_location(original_str, (i1-1)->begin));
				enums.push_back(i1->str());
				++i1;
				ASSERT_COND(i1 != i2 && (i1->type == FFL_TOKEN_TYPE::COMMA || i1->type == FFL_TOKEN_TYPE::RBRACKET), "Unexpected token when parsing enum type" << game_logic::pinpoint_location(original_str, (i1-1)->begin));
				if(i1->type == FFL_TOKEN_TYPE::COMMA) {
					++i1;
				}
			}

			ASSERT_COND(i1 != i2, "Unexpected end of enum type" << game_logic::pinpoint_location(original_str, (i1-1)->begin));

			v.push_back(variant_type_ptr(new variant_type_enum(enums)));
			++i1;
		} else if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && i1->equals("function") && i1+1 != i2 && (i1+1)->equals("(")) {
			i1 += 2;

			int min_args = -1;
			std::vector<variant_type_ptr> arg_types;
			while(i1 != i2 && !i1->equals(")")) {
				arg_types.push_back(parse_variant_type(original_str, i1, i2, allow_failure));
				if(allow_failure && !arg_types.back()) {
					return variant_type_ptr();
				}

				if(i1->equals("=")) {
					++i1;
					if(i1 != i2) {
						if(min_args == -1) {
							min_args = static_cast<int>(arg_types.size()-1);
						}
						++i1;
					}
				}

				ASSERT_COND(i1 == i2 || i1->equals(")") || i1->equals(","), "UNEXPECTED TOKENS WHEN PARSING FUNCTION: " << std::string(i1->begin, i1->end) << "\n" << game_logic::pinpoint_location(original_str, (i1-1)->end));

				if(i1->equals(",")) {
					++i1;
				}
			}

			ASSERT_COND(i1 != i2, "UNEXPECTED END OF INPUT WHILE PARSING FUNCTION DEF:\n" << game_logic::pinpoint_location(original_str, (i1-1)->end));

			++i1;

			variant_type_ptr return_type;

			if(i1 != i2 && i1->equals("->")) {
				++i1;

				ASSERT_COND(i1 != i2, "UNEXPECTED END OF INPUT WHILE PARSING FUNCTION DEF:\n" << game_logic::pinpoint_location(original_str, (i1-1)->end));
				
				return_type = parse_variant_type(original_str, i1, i2, allow_failure);
			} else {
				return_type = variant_type::get_any();
			}

			if(min_args == -1) {
				min_args = static_cast<int>(arg_types.size());
			}

			v.push_back(variant_type::get_function_type(arg_types, return_type, min_args));
		} else if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && (i1->equals("custom_obj") || i1->equals("object_type"))) {
			++i1;
			v.push_back(variant_type_ptr(new variant_type_custom_object("")));
		} else if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && (i1->equals("voxel_obj"))) {
			++i1;
			v.push_back(variant_type::get_builtin("voxel_object"));
		} else if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && (i1->equals("class") || i1->equals("obj") || i1->equals("vox"))) {
			const bool is_class = i1->equals("class");
			const bool is_vox = i1->equals("vox");
			++i1;
			ASSERT_COND(i1 != i2, "EXPECTED TYPE NAME BUT FOUND EMPTY EXPRESSION:\n" << game_logic::pinpoint_location(original_str, (i1-1)->end));
			std::string class_name(i1->begin, i1->end);

			ASSERT_COND(is_class || std::binary_search(CustomObjectType::possibleIdsIncludingPrototypes().begin(), CustomObjectType::possibleIdsIncludingPrototypes().end(), class_name), "Type obj " << class_name << "is invalid because there is no definition for a '" << class_name << "' object or object prototype:\n" << game_logic::pinpoint_location(original_str, i1->begin));

			while(i1+1 != i2 && i1+2 != i2 && (i1+1)->equals(".")) {
				class_name += ".";
				i1 += 2;
				class_name += std::string(i1->begin, i1->end);
			}

			if(is_class) {
				v.push_back(variant_type_ptr(new variant_type_class(class_name)));
// XXX -- removed for re-work
//			} else if(is_vox) {
//				v.push_back(variant_type_ptr(new variant_type_voxel_object(class_name)));
			} else {
				v.push_back(variant_type_ptr(new variant_type_custom_object(class_name)));
			}

			++i1;
		} else if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && i1->equals("any")) {
			v.push_back(variant_type_ptr(new variant_type_any));
			++i1;
		} else if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && i1->equals("commands")) {
			v.push_back(variant_type_ptr(new variant_type_commands));
			++i1;
		} else if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && i1->equals("cairo_commands")) {
			v.push_back(variant_type_ptr(new variant_type_cairo_commands));
			++i1;
		} else if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && i1->equals("builtin") && i1+1 != i2) {
			++i1;

			v.push_back(variant_type_ptr(new variant_type_builtin(std::string(i1->begin, i1->end), game_logic::ConstFormulaCallableDefinitionPtr())));
			++i1;
		} else if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && game_logic::get_formula_callable_definition(std::string(i1->begin, i1->end)).get()) {
			v.push_back(variant_type::get_builtin(std::string(i1->begin, i1->end)));
			++i1;
		} else if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER || (i1->type == FFL_TOKEN_TYPE::KEYWORD && std::equal(i1->begin, i1->end, "null"))) {
			ASSERT_COND(variant::string_to_type(std::string(i1->begin, i1->end)) != variant::VARIANT_TYPE_INVALID,
			  "INVALID TOKEN WHEN PARSING TYPE: " << std::string(i1->begin, i1->end) << " AT:\n" << game_logic::pinpoint_location(original_str, i1->begin, i1->end));

	  		variant::TYPE t(variant::string_to_type(std::string(i1->begin, i1->end)));
			ASSERT_LOG(t != variant::VARIANT_TYPE_INVALID, "INVALID TYPE: " << std::string(i1->begin, i1->end) << " AT:\n" << game_logic::pinpoint_location(original_str, i1->begin, i1->end));
			v.push_back(variant_type::get_type(t));
			++i1;
		} else if(i1->type == FFL_TOKEN_TYPE::LBRACKET) {
			const Token* end = i1+1;
			const bool res = TokenMatcher().add(FFL_TOKEN_TYPE::RBRACKET).find_match(end, i2);
			ASSERT_COND(res, "ERROR PARSING MAP TYPE: " << original_str.debug_location());

			++i1;
			ASSERT_COND(i1 != end, "ERROR PARSING MAP TYPE: " << original_str.debug_location());

			if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && i1 != end && (i1+1)->equals(":")) {
				//a specific map type.
				std::map<variant, variant_type_ptr> types;

				for(;;) {
					ASSERT_COND(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && i1+1 != end && i1+2 != end && (i1+1)->equals(":"), "ERROR PARSING MAP TYPE: " << original_str.debug_location());
					variant key(std::string(i1->begin, i1->end));
					i1 += 2;
					variant_type_ptr value_type = parse_variant_type(original_str, i1, end, allow_failure);
					ASSERT_COND(value_type, "");

					types[key] = value_type;

					if(i1 == end) {
						++i1;
						break;
					}

					ASSERT_COND(i1->equals(","), "ERROR PARSING MAP TYPE: " << original_str.debug_location() << " expected ',' but found " << std::string(i1->begin, i1->end));
					++i1;

					if(i1 == end) {
						++i1;
						break;
					}
				}

				if(types.size() == 1 && types.begin()->first.is_string()) {
					std::string type = types.begin()->first.as_string();
					//this seems suspicious, specific maps are rarely one
					//element. Check for built-in types and fail on them.
					for(int n = 0; n < variant::VARIANT_TYPE_INVALID; ++n) {
						ASSERT_COND(type != variant::variant_type_to_string(variant::TYPE(n)), "Error parsing map type. Did you mean '->' rather than ':' in " << original_str.as_string() << "?\n" << original_str.debug_location());
					}
				}

				v.push_back(variant_type::get_specific_map(types));
			} else {

				const variant_type_ptr key_type = parse_variant_type(original_str, i1, end, allow_failure);
				ASSERT_COND(key_type, "");
				ASSERT_COND(i1->type == FFL_TOKEN_TYPE::POINTER, "ERROR PARSING MAP TYPE, NO ARROW FOUND: " << original_str.debug_location());
		
				++i1;
				ASSERT_COND(i1 != end, "ERROR PARSING MAP TYPE: " << original_str.debug_location());

				const variant_type_ptr value_type = parse_variant_type(original_str, i1, end, allow_failure);
				ASSERT_COND(value_type, "");
				ASSERT_COND(i1 == end, "ERROR PARSING MAP TYPE: " << original_str.debug_location());

				v.push_back(variant_type_ptr(new variant_type_map(key_type, value_type)));

				++i1;
			}

		} else if(i1->type == FFL_TOKEN_TYPE::LSQUARE) {
			const Token* end = i1+1;
			const bool res = TokenMatcher().add(FFL_TOKEN_TYPE::RSQUARE).find_match(end, i2);
			ASSERT_COND(res, "ERROR PARSING ARRAY TYPE: " << original_str.debug_location());
	
			++i1;
			ASSERT_COND(i1 != end, "ERROR PARSING ARRAY TYPE: " << original_str.debug_location());
			
			const variant_type_ptr value_type = parse_variant_type(original_str, i1, end, allow_failure);
			if(!value_type) {
				return value_type;
			}
			if(i1 != end && i1->type == FFL_TOKEN_TYPE::COMMA) {
				std::vector<variant_type_ptr> types;
				types.push_back(value_type);
				++i1;
				while(i1 != end) {
					const variant_type_ptr value_type = parse_variant_type(original_str, i1, end, allow_failure);
					if(!value_type) {
						return value_type;
					}

					types.push_back(value_type);

					ASSERT_COND(i1 == end || i1->type == FFL_TOKEN_TYPE::COMMA, "Error parsing array type: " << original_str.debug_location());
					if(i1->type == FFL_TOKEN_TYPE::COMMA) {
						++i1;
					}
				}

				v.push_back(variant_type::get_specific_list(types));
				++i1;
			} else {
				ASSERT_COND(i1 == end, "ERROR PARSING ARRAY TYPE: " << std::string(i1->begin, i1->end) << " " << original_str.debug_location());
				v.push_back(variant_type_ptr(new variant_type_list(value_type)));
				++i1;
			}
		} else {
			ASSERT_COND(false, "UNEXPECTED TOKENS WHEN PARSING TYPE: " << std::string(i1->begin, (i2-1)->end) << " AT " << original_str.debug_location());
		}

		if(i1 != i2 && i1->type == FFL_TOKEN_TYPE::PIPE) {
			++i1;
		} else {
			break;
		}
	}

	if(v.size() == 1) {
		v.front()->setStr(std::string(begin_token->begin, (i1-1)->end));
		return v.front();
	} else {
		variant_type_ptr result(new variant_type_union(v));
		result->setStr(std::string(begin_token->begin, (i1-1)->end));
		return result;
	}
#undef ASSERT_COND
}

variant_type_ptr parse_variant_type(const variant& type)
{
	using namespace formula_tokenizer;
	const std::string& s = type.as_string();
	std::vector<Token> tokens;
	std::string::const_iterator i1 = s.begin();
	std::string::const_iterator i2 = s.end();
	while(i1 != i2) {
		try {
			Token tok = get_token(i1, i2);
			if(tok.type != FFL_TOKEN_TYPE::WHITESPACE && tok.type != FFL_TOKEN_TYPE::COMMENT) {
				tokens.push_back(tok);
			}
		} catch(TokenError& e) {
			ASSERT_LOG(false, "ERROR PARSING TYPE: " << e.msg << " IN '" << s << "' AT " << type.debug_location());
		}
	}

	ASSERT_LOG(tokens.empty() == false, "ERROR PARSING TYPE: EMPTY STRING AT " << type.debug_location());

	const Token* begin = &tokens[0];
	variant_type_ptr result = parse_variant_type(type, begin, begin + tokens.size());
	ASSERT_LOG(begin == &tokens[0] + tokens.size(), "Unexpected characters after type: '" << begin->str() << ": " << type.debug_location());
	return result;
}

variant_type_ptr
parse_optional_function_type(const variant& original_str,
                             const formula_tokenizer::Token*& i1,
                             const formula_tokenizer::Token* i2)
{
	using namespace formula_tokenizer;

	if(i1 == i2 || !i1->equals("def")) {
		return variant_type_ptr();
	}

	++i1;
	if(i1 == i2 || i1->type != FFL_TOKEN_TYPE::LPARENS) {
		return variant_type_ptr();
	}

	int optional_args = 0;

	std::vector<variant_type_ptr> args;

	++i1;
	while(i1 != i2 && i1->type != FFL_TOKEN_TYPE::RPARENS) {
		if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER && i1+1 != i2 &&
		   ((i1+1)->type == FFL_TOKEN_TYPE::COMMA ||
		    (i1+1)->type == FFL_TOKEN_TYPE::RPARENS ||
			(i1+1)->equals("="))) {
			args.push_back(variant_type::get_any());
			++i1;
			if(i1->type == FFL_TOKEN_TYPE::COMMA) {
				++i1;
			} else if(i1->equals("=")) {
				++optional_args;
				while(i1 != i2 && !i1->equals(",") && !i1->equals(")")) {
					++i1;
				}

				if(i1 != i2 && i1->type == FFL_TOKEN_TYPE::COMMA) {
					++i1;
				}
			}
			continue;
		}

		variant_type_ptr arg_type = parse_variant_type(original_str, i1, i2);
		args.push_back(arg_type);
		ASSERT_LOG(i1 != i2, "UNEXPECTED END OF EXPRESSION: " << original_str.debug_location());
		if(i1->type == FFL_TOKEN_TYPE::IDENTIFIER) {
			++i1;

			if(i1 != i2 && i1->equals("=")) {
				++optional_args;

				while(i1 != i2 && !i1->equals(",") && !i1->equals(")")) {
					++i1;
				}
			}
		}

		if(i1 != i2 && i1->type == FFL_TOKEN_TYPE::RPARENS) {
			break;
		}

		ASSERT_LOG(i1 != i2 && i1->type == FFL_TOKEN_TYPE::COMMA, "ERROR PARSING FUNCTION SIGNATURE: " << original_str.debug_location());

		++i1;
	}

	ASSERT_LOG(i1 != i2 && i1->type == FFL_TOKEN_TYPE::RPARENS, "UNEXPECTED END OF FUNCTION SIGNATURE: " << original_str.debug_location());

	variant_type_ptr return_type;

	++i1;
	if(i1 != i2 && i1->type == FFL_TOKEN_TYPE::POINTER) {
		++i1;
		return_type = parse_variant_type(original_str, i1, i2);
	}

	return variant_type::get_function_type(args, return_type, static_cast<int>(args.size()) - optional_args);
}

variant_type_ptr parse_optional_function_type(const variant& type)
{
	using namespace formula_tokenizer;
	const std::string& s = type.as_string();
	std::vector<Token> tokens;
	std::string::const_iterator i1 = s.begin();
	std::string::const_iterator i2 = s.end();
	while(i1 != i2) {
		try {
			Token tok = get_token(i1, i2);
			if(tok.type != FFL_TOKEN_TYPE::WHITESPACE && tok.type != FFL_TOKEN_TYPE::COMMENT) {
				tokens.push_back(tok);
			}
		} catch(TokenError& e) {
			ASSERT_LOG(false, "ERROR PARSING TYPE: " << e.msg << " IN '" << s << "' AT " << type.debug_location());
		}
	}

	ASSERT_LOG(tokens.empty() == false, "ERROR PARSING TYPE: EMPTY STRING AT " << type.debug_location());

	const Token* begin = &tokens[0];
	return parse_optional_function_type(type, begin, begin + tokens.size());
}

variant_type_ptr
parse_optional_formula_type(const variant& original_str,
                            const formula_tokenizer::Token*& i1,
                            const formula_tokenizer::Token* i2)
{
	variant_type_ptr result = parse_variant_type(original_str, i1, i2, true);
	if(i1 != i2 && (i1->equals("<-") || i1->equals("::"))) {
		return result;
	}

	return variant_type_ptr();
}

variant_type_ptr parse_optional_formula_type(const variant& type)
{
	using namespace formula_tokenizer;
	const std::string& s = type.as_string();
	std::vector<Token> tokens;
	std::string::const_iterator i1 = s.begin();
	std::string::const_iterator i2 = s.end();
	while(i1 != i2) {
		try {
			Token tok = get_token(i1, i2);
			if(tok.type != FFL_TOKEN_TYPE::WHITESPACE && tok.type != FFL_TOKEN_TYPE::COMMENT) {
				tokens.push_back(tok);
			}
		} catch(TokenError& e) {
			ASSERT_LOG(false, "ERROR PARSING TYPE: " << e.msg << " IN '" << s << "' AT " << type.debug_location());
		}
	}

	ASSERT_LOG(tokens.empty() == false, "ERROR PARSING TYPE: EMPTY STRING AT " << type.debug_location());

	const Token* begin = &tokens[0];
	return parse_optional_formula_type(type, begin, begin + tokens.size());
}

variant_type_ptr variant_type::get_none()
{
	static const variant_type_ptr result(new variant_type_none);
	return result;
}

variant_type_ptr variant_type::get_any()
{
	static const variant_type_ptr result(new variant_type_any);
	return result;
}

variant_type_ptr variant_type::get_commands()
{
	static const variant_type_ptr result(new variant_type_commands);
	return result;
}

variant_type_ptr variant_type::get_cairo_commands()
{
	static const variant_type_ptr result(new variant_type_cairo_commands);
	return result;
}

variant_type_ptr variant_type::get_type(variant::TYPE type)
{
	static std::vector<variant_type_ptr> result;
	if(static_cast<size_t>(type) >= result.size()) {
		result.resize(static_cast<size_t>(type)+1);
	}

	if(!result[type]) {
		result[type].reset(new variant_type_simple(type));
	}

	return result[type];
}

variant_type_ptr variant_type::get_union(const std::vector<variant_type_ptr>& elements_input)
{
	//Any type that is compatible with another type in the union is
	//redundant, so remove it here.
	std::vector<variant_type_ptr> elements = elements_input;
	{
		int nitem_to_delete = -1;
		do {
			nitem_to_delete = -1;

			for(int i = 0; i != elements.size() && nitem_to_delete == -1; ++i) {
				for(int j = 0; j != elements.size(); ++j) {
					if(j == i) {
						continue;
					}

					if(variant_types_compatible(elements[j], elements[i])) {
						nitem_to_delete = i;
						break;
					}
				}
			}

			if(nitem_to_delete != -1) {
				elements.erase(elements.begin() + nitem_to_delete);
			}
		} while(nitem_to_delete != -1);
	}

	for(variant_type_ptr el : elements) {
		const std::vector<variant_type_ptr>* items = el->is_union();
		if(items) {
			std::vector<variant_type_ptr> v = elements;
			v.erase(std::find(v.begin(), v.end(), el));
			v.insert(v.end(), items->begin(), items->end());
			return get_union(v);
		}
	}

	std::vector<variant_type_ptr> items;
	for(variant_type_ptr el : elements) {
		for(variant_type_ptr item : items) {
			if(el->is_equal(*item)) {
				el = variant_type_ptr();
				break;
			}
		}

		if(el) {
			items.push_back(el);
		}
	}

	if(items.size() == 1) {
		return items[0];
	}

	return variant_type_ptr(new variant_type_union(items));
}

variant_type_ptr variant_type::get_list(variant_type_ptr element_type)
{
	if(!element_type) {
		element_type = get_any();
	}
	return variant_type_ptr(new variant_type_list(element_type));
}

variant_type_ptr variant_type::get_specific_list(const std::vector<variant_type_ptr>& types)
{
	return variant_type_ptr(new variant_type_specific_list(types));
}

variant_type_ptr variant_type::get_map(variant_type_ptr key_type, variant_type_ptr value_type)
{
	if(!key_type) {
		key_type = get_any();
	}

	if(!value_type) {
		value_type = get_any();
	}
	return variant_type_ptr(new variant_type_map(key_type, value_type));
}

variant_type_ptr variant_type::get_specific_map(const std::map<variant, variant_type_ptr>& type_map)
{
	std::vector<variant_type_ptr> keys, values;
	for(std::map<variant, variant_type_ptr>::const_iterator i = type_map.begin(); i != type_map.end(); ++i) {
		keys.push_back(get_variant_type_from_value(i->first));
		values.push_back(i->second);
	}

	return variant_type_ptr(new variant_type_specific_map(type_map, variant_type::get_union(keys), variant_type::get_union(values)));
}

variant_type_ptr variant_type::get_class(const std::string& class_name)
{
	return variant_type_ptr(new variant_type_class(class_name));
}

variant_type_ptr variant_type::get_custom_object(const std::string& name)
{
	return variant_type_ptr(new variant_type_custom_object(name));
}

// Removed for re-work
//variant_type_ptr variant_type::get_voxel_object(const std::string& name)
//{
//	if(name == "") {
//		return variant_type::get_builtin("voxel_object");
//	}
//	return variant_type_ptr(new variant_type_voxel_object(name));
//}

variant_type_ptr variant_type::get_builtin(const std::string& name)
{
	game_logic::ConstFormulaCallableDefinitionPtr def = game_logic::get_formula_callable_definition(name);
	if(def) {
		return variant_type_ptr(new variant_type_builtin(name, def));
	} else {
		return variant_type_ptr();
	}
}

variant_type_ptr variant_type::get_function_type(const std::vector<variant_type_ptr>& arg_types, variant_type_ptr return_type, int min_args)
{
	return variant_type_ptr(new variant_type_function(arg_types, return_type, min_args));
}

variant_type_ptr variant_type::get_function_overload_type(variant_type_ptr overloaded_fn, const std::vector<variant_type_ptr>& fn)
{
	return variant_type_ptr(new variant_type_function_overload(overloaded_fn, fn));
}

variant_type_ptr variant_type::get_null_excluded(variant_type_ptr input)
{
	variant_type_ptr result = input->null_excluded();
	if(result) {
		return result;
	} else {
		return input;
	}
}

variant_type_ptr variant_type::get_with_exclusion(variant_type_ptr input, variant_type_ptr subtract)
{
	variant_type_ptr result = input->subtract(subtract);
	if(result) {
		return result;
	} else {
		return input;
	}
}

variant_type_ptr variant_type::get_generic_type(const std::string& id)
{
	return variant_type_ptr(new variant_type_generic(id));
}

generic_variant_type_scope::~generic_variant_type_scope()
{
	clear();
}

void generic_variant_type_scope::register_type(const std::string& id)
{
	g_generic_variant_names.insert(id);
	entries_.push_back(id);
}

void generic_variant_type_scope::clear()
{
	for(auto id : entries_) {
		g_generic_variant_names.erase(id);
	}
	entries_.clear();
}

UNIT_TEST(variant_type) {
#define TYPES_COMPAT(a, b) CHECK_EQ(variant_types_compatible(parse_variant_type(variant(a)), parse_variant_type(variant(b))), true)
#define TYPES_INCOMPAT(a, b) CHECK_EQ(variant_types_compatible(parse_variant_type(variant(a)), parse_variant_type(variant(b))), false)

	TYPES_COMPAT("int|bool", "int");
	TYPES_COMPAT("int|bool|string", "string");
	TYPES_COMPAT("decimal", "int");
	TYPES_COMPAT("list", "[int]");
	TYPES_COMPAT("list", "[int|string]");
	TYPES_COMPAT("list", "[any]");
	TYPES_COMPAT("[any]", "[int|string]");
	TYPES_COMPAT("[any]", "list");
	TYPES_COMPAT("{int|string -> string}", "{int -> string}");
	TYPES_COMPAT("map", "{int -> string}");

	TYPES_COMPAT("[int]", "[int,int]");
	TYPES_COMPAT("[int,int|decimal]", "[int,decimal]");

	TYPES_COMPAT("[{keys: [string], sound: commands}]", "[{keys: [string,], sound: commands}, {keys: [string,], sound: commands}]");

	TYPES_COMPAT("enum{abc}", "enum{abc}");
	TYPES_COMPAT("enum{abc,dee}", "enum{abc}");

	TYPES_COMPAT("int|function(int)->int", "int");

	TYPES_INCOMPAT("int", "int|bool");
	TYPES_INCOMPAT("int", "decimal");
	TYPES_INCOMPAT("int", "decimal");
	TYPES_INCOMPAT("[int]", "list");
	TYPES_INCOMPAT("{int -> int}", "map");
	TYPES_INCOMPAT("{int -> int}", "{string -> int}");
	TYPES_INCOMPAT("[int]", "[int,int,decimal]");
	TYPES_INCOMPAT("[int,int]", "[int]");
	TYPES_INCOMPAT("enum{abc,dec}", "enum{abc,deg}");
	TYPES_INCOMPAT("enum{abc}", "enum{abc,dee}");
	TYPES_INCOMPAT("enum{abc}", "enum{abcd}");
	TYPES_INCOMPAT("enum{abc}", "int");
	TYPES_INCOMPAT("{int -> string}", "{int|string -> string}");

#undef TYPES_COMPAT	
}
