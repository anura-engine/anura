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
#include "string_utils.hpp"
#include "variant_utils.hpp"

glm::vec3 variant_to_vec3(const variant& v)
{
	ASSERT_LOG(v.is_list() && v.num_elements() == 3, "Expected vec3 variant but found " << v.write_json());
	glm::vec3 result;
	result[0] = v[0].as_float();
	result[1] = v[1].as_float();
	result[2] = v[2].as_float();
	return result;
}

variant vec3_to_variant(const glm::vec3& v)
{
	std::vector<variant> result;
	result.emplace_back(decimal(v[0]));
	result.emplace_back(decimal(v[1]));
	result.emplace_back(decimal(v[2]));
	return variant(&result);
}

glm::ivec3 variant_to_ivec3(const variant& v)
{
	ASSERT_LOG(v.is_list() && v.num_elements() == 3, "Expected ivec3 variant but found " << v.write_json());
	return glm::ivec3(v[0].as_int(), v[1].as_int(), v[2].as_int());
}

variant ivec3_to_variant(const glm::ivec3& v)
{
	std::vector<variant> result;
	result.emplace_back(v.x);
	result.emplace_back(v.y);
	result.emplace_back(v.z);
	return variant(&result);
}

glm::quat variant_to_quat(const variant& v)
{
	ASSERT_LOG(v.is_list() && v.num_elements() == 4, "Expected vec4 variant but found " << v.write_json());
	glm::quat result;
	result.w = v[0].as_float();
	result.x = v[1].as_float();
	result.y = v[2].as_float();
	result.z = v[3].as_float();
	return result;
}

variant quat_to_variant(const glm::quat& v)
{
	std::vector<variant> result;
	result.emplace_back(decimal(v.w));
	result.emplace_back(decimal(v.x));
	result.emplace_back(decimal(v.y));
	result.emplace_back(decimal(v.z));
	return variant(&result);
}

glm::vec4 variant_to_vec4(const variant& v)
{
	ASSERT_LOG(v.is_list() && v.num_elements() == 4, "Expected vec4 variant but found " << v.write_json());
	glm::vec4 result;
	result[0] = v[0].as_float();
	result[1] = v[1].as_float();
	result[2] = v[2].as_float();
	result[3] = v[3].as_float();
	return result;
}

variant vec4_to_variant(const glm::vec4& v)
{
	std::vector<variant> result;
	result.emplace_back(v.x);
	result.emplace_back(v.y);
	result.emplace_back(v.z);
	result.emplace_back(v.w);
	return variant(&result);
}

game_logic::FormulaCallablePtr map_into_callable(variant v)
{
	if(v.is_callable()) {
		return game_logic::FormulaCallablePtr(v.mutable_callable());
	} else if(v.is_map()) {
		game_logic::MapFormulaCallable* res = new game_logic::MapFormulaCallable;
		for(const auto& p : v.as_map()) {
			res->add(p.first.as_string(), p.second);
		}

		return game_logic::FormulaCallablePtr(res);
	} else {
		return game_logic::FormulaCallablePtr();
	}
}

variant append_variants(variant a, variant b)
{
	if(a.is_null()) {
		return b;
	} else if(b.is_null()) {
		return a;
	} else if(a.is_list()) {
		if(b.is_list()) {
			if((b.num_elements() > 0 && (b[0].is_numeric() || b[0].is_string())) ||
			   (a.num_elements() > 0 && (a[0].is_numeric() || a[0].is_string()))) {
				//lists of numbers or strings are treated like scalars and we
				//set the value of b.
				return b;
			}

			return a + b;
		} else {
			std::vector<variant> v(1, b);
			return a + variant(&v);
		}
	} else if(b.is_list()) {
		std::vector<variant> v(1, a);
		return variant(&v) + b;
	} else if(a.is_map() && b.is_map()) {
		std::vector<variant> v;
		v.push_back(a);
		v.push_back(b);
		return variant(&v);
	} else {
		return b;
	}
}

std::vector<std::string> parse_variant_list_or_csv_string(variant v)
{
	if(v.is_string()) {
		return util::split(v.as_string());
	} else if(v.is_list()) {
		return v.as_list_string();
	} else {
		ASSERT_LOG(v.is_null(), "Unexpected value when expecting a string list: " << v.write_json());
		return std::vector<std::string>();
	}
}

void merge_variant_over(variant* aptr, variant b)
{
	variant& a = *aptr;

	for(variant key : b.getKeys().as_list()) {
		a = a.add_attr(key, append_variants(a[key], b[key]));
	}
	
	if(!a.get_debug_info() && b.get_debug_info()) {
		a.setDebugInfo(*b.get_debug_info());
	}
}

void smart_merge_variants(variant* dst_ptr, const variant& src)
{
	variant& dst = *dst_ptr;

	if(dst.is_map() && src.is_map()) {
		for(auto p : src.as_map()) {
			if(dst.as_map().count(p.first) == 0) {
				dst.add_attr(p.first, p.second);
			} else {
				smart_merge_variants(dst.get_attr_mutable(p.first), p.second);
			}
		}
	} else if(dst.is_list() && src.is_list()) {
		dst = dst + src;
	} else {
		ASSERT_LOG(src.type() == dst.type() || src.is_null() || dst.is_null(), "Incompatible types in merge: " << dst.write_json() << " and " << src.write_json() << " Destination from: " << dst.debug_location() << " Source from: " << src.debug_location());
		dst = src;
	}
}

void visitVariants(variant v, std::function<void (variant)> fn)
{
	fn(v);

	if(v.is_list()) {
		for(const variant& item : v.as_list()) {
			visitVariants(item, fn);
		}
	} else if(v.is_map()) {
		for(const auto& item : v.as_map()) {
			visitVariants(item.second, fn);
		}
	}
}

variant deep_copy_variant(variant v)
{
	if(v.is_map()) {
		std::map<variant,variant> m;
		for(variant key : v.getKeys().as_list()) {
			m[key] = deep_copy_variant(v[key]);
		}

		return variant(&m);
	} else if(v.is_list()) {
		std::vector<variant> items;
		for(variant item : v.as_list()) {
			items.push_back(deep_copy_variant(item));
		}

		return variant(&items);
	} else {
		return v;
	}
}

variant interpolate_variants(variant a, variant b, decimal ratio)
{
	if(a.is_numeric() && b.is_numeric()) {
		decimal inv_ratio = decimal::from_int(1) - ratio;

		variant result(a.as_decimal()*inv_ratio + b.as_decimal()*ratio);

		if(a.is_int() && b.is_int()) {
			return variant(result.as_int());
		} else {
			return result;
		}
	}

	if(a.is_list() && b.is_list()) {
		ASSERT_LOG(a.num_elements() == b.num_elements(), "Trying to interpolate invalid lists: " << a.write_json() << " vs " << b.write_json());
		std::vector<variant> v;
		v.resize(a.num_elements());
		for(int n = 0; n != a.num_elements(); ++n) {
			v[n] = interpolate_variants(a[n], b[n], ratio);
		}

		return variant(&v);
	}

	if(a.is_map() && b.is_map()) {
		const std::map<variant,variant>& am = a.as_map();
		const std::map<variant,variant>& bm = b.as_map();

		std::map<variant,variant> res;

		auto ia = am.begin();
		auto ib = bm.begin();
		while(ia != am.end() && ib != bm.end()) {
			ASSERT_LOG(ia->first == ib->first, "Trying to interpolate invalid maps: " << a.write_json() << " vs " << b.write_json());
			res[ia->first] = interpolate_variants(ia->second, ib->second, ratio);
			++ia;
			++ib;
		}

		ASSERT_LOG(ia == am.end() && ib == bm.end(), "Trying to interpolate invalid maps: " << a.write_json() << " vs " << b.write_json());
		return variant(&res);
	}
	ASSERT_LOG(false, "Trying to interpolate invalid variant values: " << a.write_json() << " vs " << b.write_json());
}

variant interpolate_variants(variant a, variant b, float ratiof)
{
	return interpolate_variants(a, b, decimal(ratiof));
}

variant_builder& variant_builder::add_value(const std::string& name, const variant& val)
{
	attr_[variant(name)].push_back(val);
	return *this;
}

variant_builder& variant_builder::setValue(const std::string& name, const variant& val)
{
	variant key(name);
	attr_.erase(key);
	attr_[key].push_back(val);
	return *this;
}

void variant_builder::merge_object(variant obj)
{
	for(variant key : obj.getKeys().as_list()) {
		setValue(key.as_string(), obj[key]);
	}
}

variant variant_builder::build()
{
	std::map<variant, variant> res;
	for(std::map<variant, std::vector<variant> >::iterator i = attr_.begin(); i != attr_.end(); ++i) {
		if(i->second.size() == 1) {
			res[i->first] = i->second[0];
		} else {
			res[i->first] = variant(&i->second);
		}
	}
	return variant(&res);
}

