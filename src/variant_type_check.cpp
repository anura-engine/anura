/*
 *  Copyright (C) 2003-2018 David White <davewx7@gmail.com>
 *                20??-2018 Kristina Simpson <sweet.kristas@gmail.com>
 *                2017-2018 galegosimpatico <galegosimpatico@outlook.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not
 *   claim that you wrote the original software. If you use this software
 *   in a product, an acknowledgement in the product documentation would be
 *   appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not be
 *   misrepresented as being the original software.
 *
 *   3. This notice may not be removed or altered from any source
 *   distribution.
 */

#include "variant_type_check.hpp"

#include "logger.hpp"
#include "variant.hpp"
#include "unit_test.hpp"


void check::type_is(const variant & v, const variant::TYPE expected_type)
{
	const variant::TYPE v_type = v.type();
	if (expected_type != v_type) {
		std::string serialized_v;
		v.serializeToString(serialized_v);
		LOG_INFO("unexpected type for variant '" + serialized_v + "'");
		const std::string expected_type_as_string =
				variant::variant_type_to_string(expected_type);
		LOG_INFO("expected type: '" + expected_type_as_string + '\'');
		const std::string v_type_as_string =
				variant::variant_type_to_string(v_type);
		LOG_INFO("actual type: '" + v_type_as_string + '\'');
		//   If the check is going to fail, check the type names
		// instead of the actual types, in order to have a pretty
		// failure message.
		const std::string actual_type_as_string = v_type_as_string;
		CHECK_EQ(expected_type_as_string, actual_type_as_string);
	}
	//   If the check is going to succeed, check the actual types, because
	// that's how it's supposed to be.
	CHECK_EQ(expected_type, v_type);
}

void check::type_is_null(const variant & v)
{
	type_is(v, variant::TYPE::VARIANT_TYPE_NULL);
}

void check::type_is_bool(const variant & v)
{
	type_is(v, variant::TYPE::VARIANT_TYPE_BOOL);
}

void check::type_is_int(const variant & v)
{
	type_is(v, variant::TYPE::VARIANT_TYPE_INT);
}

void check::type_is_decimal(const variant & v)
{
	type_is(v, variant::TYPE::VARIANT_TYPE_DECIMAL);
}

void check::type_is_object(const variant & v)
{
	type_is(v, variant::TYPE::VARIANT_TYPE_CALLABLE);
}

void check::type_is_list(const variant & v)
{
	type_is(v, variant::TYPE::VARIANT_TYPE_LIST);
}

void check::type_is_string(const variant & v)
{
	type_is(v, variant::TYPE::VARIANT_TYPE_STRING);
}

void check::type_is_dictionary(const variant & v)
{
	type_is(v, variant::TYPE::VARIANT_TYPE_MAP);
}

void check::type_is_function(const variant & v)
{
	type_is(v, variant::TYPE::VARIANT_TYPE_FUNCTION);
}

void check::type_is_generic_function(const variant & v)
{
	type_is(v, variant::TYPE::VARIANT_TYPE_GENERIC_FUNCTION);
}

void check::type_is_enum(const variant & v)
{
	type_is(v, variant::TYPE::VARIANT_TYPE_ENUM);
}
