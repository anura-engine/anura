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

#include "svg_length.hpp"

#include "unit_test.hpp"

UNIT_TEST(svg_length_0) {
	KRE::SVG::svg_length svg_length;
	const auto zero = svg_length.value_in_specified_units(KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_PERCENTAGE);
	CHECK_EQ(0, zero);
}

UNIT_TEST(svg_length_1) {
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			KRE::SVG::svg_length svg_length(99, KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_UNKNOWN);
			const auto zero = svg_length.value_in_specified_units(KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_UNKNOWN);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	ASSERT_LOG(excepted, "API changed, rewrite test accordingly");
}

UNIT_TEST(svg_length_2) {
	KRE::SVG::svg_length svg_length(99, KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_NUMBER);
	const auto same = svg_length.value_in_specified_units(KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_NUMBER);
	CHECK_EQ(99, same);
}

UNIT_TEST(svg_length_3) {
	KRE::SVG::svg_length svg_length(99, KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_PERCENTAGE);
	const auto same = svg_length.value_in_specified_units(KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_PERCENTAGE);
	CHECK_EQ(0, same);
}

UNIT_TEST(svg_length_4) {
	KRE::SVG::svg_length svg_length(99, KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_EMS);
	const auto same = svg_length.value_in_specified_units(KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_EMS);
	CHECK_EQ(0, same);
}

UNIT_TEST(svg_length_5) {
	KRE::SVG::svg_length svg_length(99, KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_EXS);
	const auto same = svg_length.value_in_specified_units(KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_EXS);
	CHECK_EQ(0, same);
}

UNIT_TEST(svg_length_6) {
	KRE::SVG::svg_length svg_length(99, KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_PX);
	const auto same = svg_length.value_in_specified_units(KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_PX);
	CHECK_EQ(0, same);
}

UNIT_TEST(svg_length_7) {
	KRE::SVG::svg_length svg_length(99, KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_CM);
	const auto same = svg_length.value_in_specified_units(KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_CM);
	CHECK_EQ(0, same);
}

UNIT_TEST(svg_length_8) {
	KRE::SVG::svg_length svg_length(99, KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_MM);
	const auto same = svg_length.value_in_specified_units(KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_MM);
	CHECK_EQ(0, same);
}

UNIT_TEST(svg_length_9) {
	KRE::SVG::svg_length svg_length(99, KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_IN);
	const auto same = svg_length.value_in_specified_units(KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_IN);
	CHECK_EQ(0, same);
}

UNIT_TEST(svg_length_10) {
	KRE::SVG::svg_length svg_length(99, KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_PT);
	const auto same = svg_length.value_in_specified_units(KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_PT);
	CHECK_EQ(0, same);
}

UNIT_TEST(svg_length_11) {
	KRE::SVG::svg_length svg_length(99, KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_PC);
	const auto same = svg_length.value_in_specified_units(KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_PC);
	CHECK_EQ(0, same);
}
