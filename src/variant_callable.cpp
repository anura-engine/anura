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

#include "variant_callable.hpp"

variant variant_callable::create(variant* v)
{
	v->make_unique();
	return variant(new variant_callable(*v));
}

variant_callable::variant_callable(const variant& v) : value_(v)
{
}

variant variant_callable::getValue(const std::string& key) const
{
	if(key == "self") {
		return variant(this);
	}

	variant result = value_[variant(key)];
	if(result.is_list()) {
		return create_for_list(result);
	} else if(result.is_map()) {
		return variant(new variant_callable(result));
	} else {
		return result;
	}
}

variant variant_callable::create_for_list(const variant& value) const
{
	std::vector<variant> v;
	for(int n = 0; n != value.num_elements(); ++n) {
		const variant& item = value[n];
		if(item.is_list()) {
			v.push_back(create_for_list(item));
		} else if(item.is_map()) {
			v.push_back(variant(new variant_callable(item)));
		} else {
			v.push_back(item);
		}
	}

	return variant(&v);
}

void variant_callable::setValue(const std::string& key, const variant& value)
{
	value_.add_attr_mutation(variant(key), value);
}
