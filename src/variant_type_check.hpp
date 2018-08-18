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

#include "variant.hpp"


namespace check
{
	//   Includes a `CHECK_EQ` that must check that the type of the input
	// variant `v` matches `expected_type` (the type expectation). This is
	// provided as an alternative to
	// `void variant::must_be(variant::TYPE) const` more centered around
	// unit tests (makes the test a failure, instead of aborting fatally).
	void type_is(const variant & v, const variant::TYPE expected_type);

	//   Includes a `CHECK_EQ` that must check that the type of the input
	// variant `v` matches the expectation. This is syntactic sugar for
	// `type_is(const variant &, const variant::TYPE)`.
	void type_is_null(const variant & v);

	//   Includes a `CHECK_EQ` that must check that the type of the input
	// variant `v` matches the expectation. This is syntactic sugar for
	// `type_is(const variant &, const variant::TYPE)`.
	void type_is_bool(const variant & v);

	//   Includes a `CHECK_EQ` that must check that the type of the input
	// variant `v` matches the expectation. This is syntactic sugar for
	// `type_is(const variant &, const variant::TYPE)`.
	void type_is_int(const variant & v);

	//   Includes a `CHECK_EQ` that must check that the type of the input
	// variant `v` matches the expectation. This is syntactic sugar for
	// `type_is(const variant &, const variant::TYPE)`.
	void type_is_decimal(const variant & v);

	//   Includes a `CHECK_EQ` that must check that the type of the input
	// variant `v` matches the expectation. This is syntactic sugar for
	// `type_is(const variant &, const variant::TYPE)`.
	void type_is_object(const variant & v);

	//   Includes a `CHECK_EQ` that must check that the type of the input
	// variant `v` matches the expectation. This is syntactic sugar for
	// `type_is(const variant &, const variant::TYPE)`.
	void type_is_list(const variant & v);

	//   Includes a `CHECK_EQ` that must check that the type of the input
	// variant `v` matches the expectation. This is syntactic sugar for
	// `type_is(const variant &, const variant::TYPE)`.
	void type_is_string(const variant & v);

	//   Includes a `CHECK_EQ` that must check that the type of the input
	// variant `v` matches the expectation. This is syntactic sugar for
	// `type_is(const variant &, const variant::TYPE)`.
	void type_is_dictionary(const variant & v);

	//   Includes a `CHECK_EQ` that must check that the type of the input
	// variant `v` matches the expectation. This is syntactic sugar for
	// `type_is(const variant &, const variant::TYPE)`.
	void type_is_function(const variant & v);

	//   Includes a `CHECK_EQ` that must check that the type of the input
	// variant `v` matches the expectation. This is syntactic sugar for
	// `type_is(const variant &, const variant::TYPE)`.
	void type_is_generic_function(const variant & v);

	//   Includes a `CHECK_EQ` that must check that the type of the input
	// variant `v` matches the expectation. This is syntactic sugar for
	// `type_is(const variant &, const variant::TYPE)`.
	void type_is_enum(const variant & v);
}
