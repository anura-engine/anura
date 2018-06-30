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

#include <cstring>
#include <stdio.h>
#include <sstream>

#include "decimal.hpp"
#include "unit_test.hpp"

#define DECIMAL(num) static_cast<int64_t>(num##LL)

/**
 * Can handle most strings in the form of `-?\d+(\.\d+)?`.
 *
 * Should also handle well all of `-?(\d+)?\.\d+` I think, by
 * documentation of `strtol(3)`.
 *
 * Undefined behavior for `-?\d+\.`?
 */
decimal decimal::from_string(const std::string& s)
{
	bool negative = false;
	const char* ptr = s.c_str();
	if(*ptr == '-') {
		negative = true;
		++ptr;
	}
	char* endptr = nullptr, *enddec = nullptr;
	int64_t n = strtol(ptr, &endptr, 10);

	if (* endptr != '.') {
		if (negative) {
			n = -n;
		}
		return decimal::from_raw_value(n * DECIMAL_PRECISION);
	}

	int64_t m = strtol(endptr+1, &enddec, 10);
	//   XXX Can `m` be not a part of `s`? For instance, passing `-5446.`
	// to this function?
	auto dist = enddec - endptr;
	while(dist > (DECIMAL_PLACES+1)) {
		m /= 10;
		--dist;
	}
	while(dist < (DECIMAL_PLACES+1)) {
		m *= 10;
		++dist;
	}

	if(negative) {
		n = -n;
		m = -m;
	}

	return decimal::from_raw_value(n*DECIMAL_PRECISION + m);
}

std::ostream& operator<<(std::ostream& s, decimal d)
{
	const char* minus = "";
	if(d.value() < 0 && d.value() > -DECIMAL_PRECISION) {
		//values between 0 and -1.0 won't have a minus sign, so correct that.
		minus = "-";
	}

	char buf[512];
	sprintf(buf, "%s%lld.%06lld", minus, static_cast<long long int>(d.value()/DECIMAL_PRECISION), static_cast<long long int>((d.value() > 0 ? d.value() : -d.value())%DECIMAL_PRECISION));
	char* ptr = buf + strlen(buf) - 1;
	while(*ptr == '0' && ptr[-1] != '.') {
		*ptr = 0;
		--ptr;
	}
	s << buf;
	return s;
}

decimal operator*(const decimal& a, const decimal& b)
{
	const int64_t va = a.value() > 0 ? a.value() : -a.value();
	const int64_t vb = b.value() > 0 ? b.value() : -b.value();

	const int64_t ia = va/DECIMAL_PRECISION;
	const int64_t ib = vb/DECIMAL_PRECISION;

	const int64_t fa = va%DECIMAL_PRECISION;
	const int64_t fb = vb%DECIMAL_PRECISION;

	const decimal result = decimal::from_raw_value(ia*ib*DECIMAL_PRECISION + fa*ib + fb*ia + (fa*fb)/DECIMAL_PRECISION);
	if((a.value() < 0 && b.value() > 0) || (b.value() < 0 && a.value() > 0)) {
		return -result;
	} else {
		return result;
	}
}

decimal operator/(const decimal& a, const decimal& b)
{
	int64_t va = a.value() > 0 ? a.value() : -a.value();
	int64_t vb = b.value() > 0 ? b.value() : -b.value();

	if(va == 0LL) {
		return a;
	}

	int64_t orders_of_magnitude_shift = 0;
	const int64_t targetValue = DECIMAL(10000000000000);

	while(va < targetValue) {
		va *= DECIMAL(10);
		++orders_of_magnitude_shift;
	}

	const int64_t targetValue_b = DECIMAL(1000000);

	while(vb > targetValue_b) {
		vb /= DECIMAL(10);
		++orders_of_magnitude_shift;
	}

	int64_t value = (va/vb);

	while(orders_of_magnitude_shift > 6) {
		value /= DECIMAL(10);
		--orders_of_magnitude_shift;
	}

	while(orders_of_magnitude_shift < 6) {
		value *= DECIMAL(10);
		++orders_of_magnitude_shift;
	}

	const decimal result(decimal::from_raw_value(value));
	if((a.value() < 0 && b.value() > 0) || (b.value() < 0 && a.value() > 0)) {
		return -result;
	} else {
		return result;
	}
}

namespace {
struct TestCase {
	double value;
	std::string expected;
};
}

UNIT_TEST(decimal_from_string) {
	TestCase tests[] = {
		{ 0, "0" },
		{ 0.032993, "0.032993" },
		{ .032993, ".032993" },
		{ 0.32993, "0.32993" },
		{ .32993, ".32993" },
		{ 0.5, "0.5" },
		{ .5, ".5" },
		{ 5.5, "5.5" },
		{ -1.5, "-1.5" },
		{ 6, "6" },
		{ 500000, "500000" },
		{ 500000, "500000.000000" },
		{ -500000, "-500000" },
		{ -500000, "-500000.000000" },
		{ 999999, "999999" },
		{ -999999, "-999999" },
		{ 999999.999999, "999999.999999" },
		{ -999999.999999, "-999999.999999" },
// 		{ 999999999.999999, "999999999.999999" },
// 		{ -999999999.999999, "-999999999.999999" },
// 		{ 999999999999.999999, "999999999999.999999" },
// 		{ -999999999999.999999, "-999999999999.999999" },
	};

	for(int n = 0; n != sizeof(tests)/sizeof(tests[0]); ++n) {
		CHECK_EQ_M(
				tests[n].value,
				decimal::from_string(tests[n].expected).as_float(),
				"CASE: " << n);
	}
}

UNIT_TEST(decimal_output) {
	TestCase tests[] = {
		{ 5.5, "5.5" },
		{ 4.0, "4.0" },
		{ -0.5, "-0.5" },
		{ -2.5, "-2.5" },
	};

	for(int n = 0; n != sizeof(tests)/sizeof(tests[0]); ++n) {
		std::ostringstream s;
		s << decimal(tests[n].value);
		CHECK_EQ(s.str(), tests[n].expected);
	}
}

UNIT_TEST(decimal_mul) {
	for(int64_t n = 0; n < 45000; n += 1000) {
		CHECK_EQ(n*(n > 0 ? n : -n), (decimal::from_int(static_cast<int>(n))*decimal::from_int(static_cast<int>(n > 0 ? n : -n))).as_int());
	}


	//10934.54 * 7649.44
	CHECK_EQ(decimal::from_raw_value(DECIMAL(10934540000))*decimal::from_raw_value(DECIMAL(7649440000)), decimal::from_raw_value(DECIMAL(83643107657600)));
	CHECK_EQ(decimal::from_raw_value(-DECIMAL(10934540000))*decimal::from_raw_value(DECIMAL(7649440000)), -decimal::from_raw_value(DECIMAL(83643107657600)));

	CHECK_EQ(decimal::from_string("0.08")*decimal::from_string("0.5"), decimal::from_string("0.04"));
}

UNIT_TEST(decimal_assign_mul_0) {
	const uint_fast8_t a = 2;
	decimal b(decimal::from_int(3));
	const decimal c(decimal::from_int(6));
	b *= a;
	CHECK_EQ(c, b);
}

UNIT_TEST(decimal_assign_mul_1) {
	const decimal a(decimal::from_int(2));
	decimal b(decimal::from_string("3.0"));
	const decimal c(decimal::from_int(6));
	b *= a;
	CHECK_EQ(c, b);
}

UNIT_TEST(decimal_div) {
	//10934.54 / 7649.44
	CHECK_EQ(decimal::from_raw_value(DECIMAL(10934540000))/decimal::from_raw_value(DECIMAL(7649440000)), decimal::from_raw_value(DECIMAL(1429456)));
}

UNIT_TEST(decimal_assign_div_0) {
	const uint_fast8_t a = 2;
	decimal b(decimal::from_int(15));
	const decimal c(decimal::from_string("7.5"));
	b /= a;
	CHECK_EQ(c, b);
}

UNIT_TEST(decimal_assign_div_1) {
	const decimal a(decimal::from_int(2));
	decimal b(decimal::from_string("15.0"));
	const decimal c(decimal::from_string("7.5"));
	b /= a;
	CHECK_EQ(c, b);
}

BENCHMARK(decimal_div_bench) {
	BENCHMARK_LOOP {
		decimal res(decimal::from_raw_value(DECIMAL(0)));
		for(int n = 1; n < 1000000; ++n) {
			res += decimal::from_int(n)/decimal::from_int(1000100-n);
		}
	}
}
