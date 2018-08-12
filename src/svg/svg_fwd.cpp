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

#include "svg_fwd.hpp"

#include "unit_test.hpp"

UNIT_TEST(svg_fwd_clamp_0) {
	const uint_fast8_t val = 1;
	const uint_fast8_t min_val = 0;
	const uint_fast8_t max_val = 2;
	const uint_fast8_t expected_clamp = 1;
	LOG_DEBUG(expected_clamp);
	const uint_fast8_t actual_clamp = KRE::clamp(val, min_val, max_val);
	LOG_DEBUG(actual_clamp);
	CHECK_EQ(expected_clamp, actual_clamp);
}

UNIT_TEST(svg_fwd_clamp_1) {
	const int_fast16_t val = 0;
	const int_fast16_t min_val = !0x7fff;
	const int_fast16_t max_val = 0x7fff;
	const int_fast16_t expected_clamp = 0;
	LOG_DEBUG(expected_clamp);
	const int_fast16_t actual_clamp = KRE::clamp(val, min_val, max_val);
	LOG_DEBUG(actual_clamp);
	CHECK_EQ(expected_clamp, actual_clamp);
}
