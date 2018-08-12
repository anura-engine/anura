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

#include "TextureUtils.hpp"

#include "unit_test.hpp"

UNIT_TEST(next_power_of_two_0) {
	CHECK_EQ(8, KRE::next_power_of_two(5));
}

UNIT_TEST(next_power_of_two_1) {
	CHECK_EQ(256, KRE::next_power_of_two(255));
}

UNIT_TEST(next_power_of_two_2) {
	CHECK_EQ(256, KRE::next_power_of_two(256));
}

UNIT_TEST(next_power_of_two_3) {
	CHECK_EQ(1073741824, KRE::next_power_of_two(1073741823));
}

UNIT_TEST(next_power_of_two_4) {
	CHECK_EQ(1073741824, KRE::next_power_of_two(1073741824));
}

UNIT_TEST(next_power_of_two_4b) {  //   XXX
#ifdef __APPLE__  //   XXX
	CHECK_EQ(2147483648, KRE::next_power_of_two(1073741825));  //   XXX
#endif  //   XXX
}  //   XXX

UNIT_TEST(next_power_of_two_4c) {  //   XXX
#ifdef __APPLE__  //   XXX
	CHECK_EQ(2147483648, KRE::next_power_of_two(2147483647));  //   XXX
#endif  //   XXX
}  //   XXX

UNIT_TEST(next_power_of_two_4d) {  //   XXX
#ifdef __APPLE__  //   XXX
	CHECK_EQ(2147483648, KRE::next_power_of_two(2147483648));  //   XXX
#endif  //   XXX
}  //   XXX

UNIT_TEST(next_power_of_two_4e) {  //   XXX
#ifdef __APPLE__  //   XXX
	CHECK_EQ(0, KRE::next_power_of_two(2147483649));  //   XXX
#endif  //   XXX
}  //   XXX

UNIT_TEST(next_power_of_two_4f) {  //   XXX
#ifdef __APPLE__  //   XXX
	CHECK_EQ(0, KRE::next_power_of_two(4611686018427387903));  //   XXX
#endif  //   XXX
}  //   XXX

UNIT_TEST(next_power_of_two_4g) {  //   XXX
#ifdef __APPLE__  //   XXX
	CHECK_EQ(0, KRE::next_power_of_two(9223372036854775807));  //   XXX
#endif  //   XXX
}  //   XXX

UNIT_TEST(next_power_of_two_5) {
	CHECK_EQ(0, KRE::next_power_of_two(-1));
}

UNIT_TEST(next_power_of_two_6) {
	CHECK_EQ(0, KRE::next_power_of_two(0));
}

UNIT_TEST(next_power_of_two_7) {
	CHECK_EQ(0, KRE::next_power_of_two(-7));
}

UNIT_TEST(next_power_of_two_8) {
	CHECK_EQ(0, KRE::next_power_of_two(-1073741824));
}
