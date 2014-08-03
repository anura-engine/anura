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

#pragma once

#include <string>

#include <iosfwd>
#include <boost/cstdint.hpp>
#if defined(TARGET_BLACKBERRY)
#include <math.h>
#endif

static const int64_t DECIMAL_PRECISION = 1000000;
static const int64_t DECIMAL_PLACES = 6;

class decimal
{
public:
	static decimal from_string(const std::string& s);
	static decimal from_int(int v) { return decimal(v); }
	static decimal from_raw_value(int64_t v) { decimal d; d.value_ = v; return d; }
	static decimal epsilon() { return decimal::from_raw_value(static_cast<int64_t>(1)); }
	decimal() : value_(0) {}
	explicit decimal(int value) : value_(static_cast<int64_t>(value)*DECIMAL_PRECISION) {}
#if defined(TARGET_BLACKBERRY)
	explicit decimal(double value) : value_(llround(value*DECIMAL_PRECISION)) {}
#else
	explicit decimal(double value) : value_(static_cast<int64_t>(value*DECIMAL_PRECISION)) {}
#endif

	int64_t value() const { return value_; }
	int as_int() const { return int( value_/DECIMAL_PRECISION ); }
	double as_float() const { return value_/static_cast<double>(DECIMAL_PRECISION); }
	int64_t fractional() const { return value_%DECIMAL_PRECISION; }

	decimal operator-() const {
		return decimal(from_raw_value(-value_));
	}

	friend decimal operator+(const decimal& a, const decimal& b);
	friend decimal operator-(const decimal& a, const decimal& b);
	friend decimal operator*(const decimal& a, const decimal& b);
	friend decimal operator/(const decimal& a, const decimal& b);

	void operator+=(decimal a) { *this = *this + a; } 
	void operator-=(decimal a) { *this = *this - a; } 
	void operator*=(decimal a) { *this = *this * a; } 
	void operator/=(decimal a) { *this = *this / a; }

	void operator+=(int a) { operator+=(decimal::from_int(a)); } 
	void operator-=(int a) { operator-=(decimal::from_int(a)); } 
	void operator*=(int a) { operator*=(decimal::from_int(a)); } 
	void operator/=(int a) { operator/=(decimal::from_int(a)); }

private:
	int64_t value_;
};

inline decimal operator+(const decimal& a, const decimal& b) {
	return decimal::from_raw_value(a.value() + b.value());
}

inline decimal operator-(const decimal& a, const decimal& b) {
	return decimal::from_raw_value(a.value() - b.value());
}

decimal operator*(const decimal& a, const decimal& b);
decimal operator/(const decimal& a, const decimal& b);

inline bool operator==(const decimal& a, const decimal& b) {
	return a.value() == b.value();
}

inline bool operator!=(const decimal& a, const decimal& b) {
	return !operator==(a, b);
}

inline bool operator<=(const decimal& a, const decimal& b) {
	return a.value() <= b.value();
}

inline bool operator>=(const decimal& a, const decimal& b) {
	return b <= a;
}

inline bool operator<(const decimal& a, const decimal& b) {
	return !(b <= a);
}

inline bool operator>(const decimal& a, const decimal& b) {
	return !(a <= b);
}

inline decimal operator+(decimal a, int b) { return operator+(a, decimal::from_int(b)); }
inline decimal operator-(decimal a, int b) { return operator-(a, decimal::from_int(b)); }
inline decimal operator*(decimal a, int b) { return operator*(a, decimal::from_int(b)); }
inline decimal operator/(decimal a, int b) { return operator/(a, decimal::from_int(b)); }
inline bool operator<(decimal a, int b) { return operator<(a, decimal::from_int(b)); }
inline bool operator>(decimal a, int b) { return operator>(a, decimal::from_int(b)); }
inline bool operator<=(decimal a, int b) { return operator<=(a, decimal::from_int(b)); }
inline bool operator>=(decimal a, int b) { return operator>=(a, decimal::from_int(b)); }

inline decimal operator+(int a, decimal b) { return operator+(decimal::from_int(a), b); }
inline decimal operator-(int a, decimal b) { return operator-(decimal::from_int(a), b); }
inline decimal operator*(int a, decimal b) { return operator*(decimal::from_int(a), b); }
inline decimal operator/(int a, decimal b) { return operator/(decimal::from_int(a), b); }
inline bool operator<(int a, decimal b) { return operator<(decimal::from_int(a), b); }
inline bool operator>(int a, decimal b) { return operator>(decimal::from_int(a), b); }
inline bool operator<=(int a, decimal b) { return operator<=(decimal::from_int(a), b); }
inline bool operator>=(int a, decimal b) { return operator>=(decimal::from_int(a), b); }

std::ostream& operator<<(std::ostream& s, decimal d);
