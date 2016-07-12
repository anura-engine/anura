/*
   Copyright 2014 Kristina Simpson <sweet.kristas@gmail.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <algorithm>
#include <ostream>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

namespace geometry
{
	namespace
	{
		std::vector<std::string> split(const std::string& input, const std::string& re) {
			// passing -1 as the submatch index parameter performs splitting
			boost::regex regex(re);
			boost::sregex_token_iterator first(input.begin(), input.end(), regex, -1), last;
			return std::vector<std::string>(first, last);
		}	
	}

	template<typename T> inline
	Point<T>::Point(const std::vector<T>& v)
		: x(0), y(0)
	{
		if(v.size() == 1) {
			x = v[0];
		} else if(!v.empty()) {
			x = v[0];
			y = v[1];
		}
	}

	/*template<typename T> inline
	Point<T>::Point(const variant& v)
	{
		ASSERT_LOG(false, "No template specialisation for Parent<T>(const varaint&)");
	}*/

	template<typename T> inline
	Point<T>::Point(const std::string& str)
	{
		if(str.empty()) {
			*this = Point<T>();
			return;
		}

		T items[2];
		int num_items = 0;
		std::vector<std::string> buf = split(str, ",| |;");
		for(int n = 0; n != 2 && n != buf.size(); ++n) {
			items[num_items++] = boost::lexical_cast<T>(buf[n]);
		}

		switch(num_items) {
			case 1: *this = Point<T>(items[0], T(0));		break;
			case 2: *this = Point<T>(items[0], items[1]);	break;
			default: *this = Point<T>();					break;
		}
	}

	template<typename T> inline
	bool operator==(const Point<T>& a, const Point<T>& b)
	{
		return a.x == b.x && a.y == b.y;
	}

	template<typename T> inline
	bool operator!=(const Point<T>& a, const Point<T>& b)
	{
		return !operator==(a, b);
	}
	
	template<typename T> inline
	bool operator<(const Point<T>& a, const Point<T>& b)
	{
		return (a.x < b.x) || (a.x == b.x && a.y < b.y);
	}

	template<typename T> inline
	Point<T> operator+(const Point<T>& lhs, const Point<T>& rhs)
	{
		return Point<T>(lhs.x+rhs.x,lhs.y+rhs.y);
	}

	template<typename T> inline
	Point<T> operator-(const Point<T>& lhs, const Point<T>& rhs)
	{
		return Point<T>(lhs.x-rhs.x,lhs.y-rhs.y);
	}

	template<typename T> inline
	std::ostream& operator<<(std::ostream& os, const Point<T>& p)
	{
		os << "(" << p.x << "," << p.y << ")";
		return os;
	}

	// Assumes that D is a scaling factor for T
	template<typename T, typename D> inline
	Point<T> operator*(const Point<T>& lhs, const Point<D>& rhs)
	{
		return Point<T>(static_cast<T>(lhs.x * rhs.x), static_cast<T>(lhs.y * rhs.y));
	}
	template<typename T> inline
	Point<T> operator*(const Point<T>& lhs, float scalar)
	{
		return Point<T>(static_cast<T>(lhs.x * scalar), static_cast<T>(lhs.y * scalar));
	}
	template<typename T> inline
	Point<T> operator*(const Point<T>& lhs, double scalar)
	{
		return Point<T>(static_cast<T>(lhs.x * scalar), static_cast<T>(lhs.y * scalar));
	}

	template<typename T> inline
	Point<T> normalize(const Point<T>& p)
	{
		T length = std::sqrt(p.x*p.x + p.y*p.y);
		return Point<T>(p.x/length, p.y/length);
	}

	template<typename T> inline
	Rect<T>::Rect(T x, T y, T w, T h)
		: top_left_(std::min<T>(x, x + w), std::min<T>(y, y + h)),
		  bottom_right_(std::max<T>(x, x + w), std::max<T>(y, y + h))
	{
	}

	template<typename T> inline 
	Rect<T>::Rect(const Point<T>& xy, T w, T h)
		: top_left_(std::min<T>(xy.x, xy.x + w), std::min<T>(xy.y, xy.y + h)),
		  bottom_right_(std::max<T>(xy.x, xy.x + w), std::max<T>(xy.y, xy.y + h))
	{
	}

	template<typename T> inline
	Rect<T>::Rect(const std::vector<T>& v)
	{
		from_vector(v);
	}

	template<typename T> inline
	bool operator<(const Rect<T>& a, const Rect<T>& b)
	{
		return a.top_left() == b.top_left() ? a.w() == b.w() ? a.h() < b.h() : a.w() < b.w() : a.top_left() < b.top_left();
	}

	template<typename T, typename D> inline
	Rect<T> operator*(const Rect<T>& lhs, const Rect<D>& rhs)
	{
		return Rect<T>::fromCoordinates(static_cast<T>(lhs.x()*rhs.x()), 
			static_cast<T>(lhs.y()*rhs.y()), 
			static_cast<T>(lhs.x2()*rhs.x2()), 
			static_cast<T>(lhs.y2()*rhs.y2()));
	}

	template<typename T, typename D> inline
	Rect<D> operator/(const Rect<T>& lhs, D scalar)
	{
		return Rect<D>::fromCoordinates(static_cast<D>(lhs.x1()/scalar), 
			static_cast<D>(lhs.y1()/scalar), 
			static_cast<D>(lhs.x2()/scalar), 
			static_cast<D>(lhs.y2()/scalar));
	}

	template<typename T> inline
	Rect<T> operator*(const Rect<T>& lhs, const Point<T>& p)
	{
		return Rect<T>(lhs.x(), lhs.y(), lhs.w()*p.x, lhs.h()*p.y);
	}

	template<typename T> inline
	Rect<T> operator*(const Rect<T>& lhs, float scalar)
	{
		return Rect<T>(lhs.x(), lhs.y(), static_cast<T>(lhs.w()*scalar), static_cast<T>(lhs.h()*scalar));
	}

	template<typename T> inline
	Rect<T> operator*(const Rect<T>& lhs, double scalar)
	{
		return Rect<T>(lhs.x(), lhs.y(), static_cast<T>(lhs.w()*scalar), static_cast<T>(lhs.h()*scalar));
	}

	/*template<typename T> inline
	Rect<T>::Rect(const variant& v)
	{
		ASSERT_LOG(false, "No template specialisation for Rect<T>(const varaint&)");
	}*/

	template<typename T> inline
	Rect<T> operator+(const Rect<T>& r, const Point<T>& p)
	{
		return Rect<T>(r.x()+p.x, r.y()+p.y, r.w(), r.h());
	}

	template<typename T> inline
	Rect<T>::Rect(const std::string& str)
	{
		if(str.empty()) {
			*this = Rect<T>();
			return;
		}

		T items[4];
		int num_items = 0;
		std::vector<std::string> buf = split(str, ",| |;");
		for(int n = 0; n != 4 && n != buf.size(); ++n) {
			items[num_items++] = boost::lexical_cast<T>(buf[n]);
		}

		switch(num_items) {
			case 2:
				*this = Rect<T>::fromCoordinates(items[0], items[1], T(1), T(1));
				break;
			case 3:
				*this = Rect<T>::fromCoordinates(items[0], items[1], items[2], T(1));
				break;
			case 4:
				*this = Rect<T>::fromCoordinates(items[0], items[1], items[2], items[3]);
				break;
			default:
				*this = Rect<T>();
				break;
		}
	}

	template<typename T> inline
	std::ptrdiff_t rect_difference(const Rect<T>& a, const Rect<T>& b, Rect<T>* output)
	{
		if (rects_intersect(a,b) == false){ //return empty if there's no intersection
			return -1;
		}

		/* returning 4 rectangles in this orientation:
		_________
		| |___| |
		| | | |
		| |___| |
		|_|___|_| */

		const Rect<T>* begin_output = output;

		if(a.x() < b.x()) {
			//get the left section of the source rectangle
			*output++ = Rect<T>(a.x(), a.y(), b.x() - a.x(), a.h());
		}

		if(a.x() + a.w() > b.x() + b.w()) {
			*output++ = Rect<T>(b.x() + b.w(), a.y(), (a.x() + a.w()) - (b.x() + b.w()), a.h());
		}

		if(a.y() < b.y()) {
			const int x1 = std::max(a.x(), b.x());
			const int x2 = std::min(a.x() + a.w(), b.x() + b.w());
			*output++ = Rect<T>(x1, a.y(), x2 - x1, b.y() - a.y());
		}

		if(a.y() + a.h() > b.y() + b.h()) {
			const int x1 = std::max(a.x(), b.x());
			const int x2 = std::min(a.x() + a.w(), b.x() + b.w());
			*output++ = Rect<T>(x1, b.y() + b.h(), x2 - x1, (a.y() + a.h()) - (b.y() + b.h()));
		}

		return output - begin_output;
	}

	template<typename T> inline
	Rect<T> rect_union(const Rect<T>& a, const Rect<T>& b)
	{
		if(a.w() == 0 || a.h() == 0) {
			return b;
		}
		
		if(b.w() == 0 || b.h() == 0) {
			return a;
		}

		const int x = std::min<int>(a.x(), b.x());
		const int y = std::min<int>(a.y(), b.y());
		const int x2 = std::max<int>(a.x2(), b.x2());
		const int y2 = std::max<int>(a.y2(), b.y2());

		return Rect<T>(x, y, x2 - x, y2 - y);
	}

	template<typename T> inline
	std::ostream& operator<<(std::ostream& os, const Rect<T>& r)
	{
		os << "rect(" << r.x() << ", " << r.y() << ", " << r.x2() << ", " << r.y2() << ")";
		return os;
	}

	template<typename T> inline
	variant Rect<T>::write() const
	{
		std::vector<variant> v;
		v.reserve(4);
		v.emplace_back(x());
		v.emplace_back(y());
		v.emplace_back(x2()-1);
		v.emplace_back(y2()-1);
		return variant(&v);
	}

	template<typename T> inline
	variant Point<T>::write() const
	{
		std::vector<variant> v;
		v.reserve(2);
		v.emplace_back(x);
		v.emplace_back(y);
		return variant(&v);
	}

	template<typename T> inline
	bool pointInRect(const Point<T>& p, const Rect<T>& r)
	{	
		return p.x >= r.x() && p.y >= r.y() && p.x <= r.x2() && p.y <= r.y2();
	}
	template<typename T> inline
	bool pointInRect(const T& x, const T& y, const Rect<T>& r)
	{	
		return x >= r.x() && y >= r.y() && x <= r.x2() && y <= r.y2();
	}

	template<typename T> inline
	bool operator==(const Rect<T>& a, const Rect<T>& b)
	{
		return a.top_left() == b.top_left() && a.bottom_right() == b.bottom_right();
	}

	template<typename T> inline
	bool operator!=(const Rect<T>& a, const Rect<T>& b)
	{
		return !operator==(a, b);
	}

	template<typename T> inline
	bool rects_intersect(const Rect<T>& a, const Rect<T>& b)
	{
		if(a.x2() <= b.x() || b.x2() <= a.x()) {
			return false;
		}

		if(a.y2() <= b.y() || b.y2() <= a.y()) {
			return false;
		}

		if(a.w() == 0 || a.h() == 0 || b.w() == 0 || b.h() == 0) {
			return false;
		}

		return true;
	}

	template<typename T> inline
	Rect<T> intersection_rect(const Rect<T>& a, const Rect<T>& b)
	{
		const int x = std::max(a.x(), b.x());
		const int y = std::max(a.y(), b.y());
		const int w = std::max(0, std::min(a.x2(), b.x2()) - x);
		const int h = std::max(0, std::min(a.y2(), b.y2()) - y);
		return Rect<T>(x, y, w, h);
	}
}
