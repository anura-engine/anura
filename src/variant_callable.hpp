/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#ifndef VARIANT_CALLABLE_HPP_INCLUDED
#define VARIANT_CALLABLE_HPP_INCLUDED

#include <boost/intrusive_ptr.hpp>

#include "formula_callable.hpp"
#include "variant.hpp"

class variant_callable;

typedef boost::intrusive_ptr<variant_callable> variant_callable_ptr;
typedef boost::intrusive_ptr<const variant_callable> const_variant_callable_ptr;

class variant_callable : public game_logic::formula_callable
{
public:
	static variant create(variant* v);

	const variant& get_value() const { return value_; }
private:
	variant_callable(const variant& v);

	variant get_value(const std::string& key) const;
	void set_value(const std::string& key, const variant& value);

	variant create_for_list(const variant& list) const;

	variant value_;
};

#endif
