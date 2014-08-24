/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <algorithm>
#include <ostream>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

namespace Geometry
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

	template<typename T> inline
	Point<T>::Point(const variant& v)
	{
		ASSERT_LOG(false, "No template specialisation for Parent<T>(const varaint&)");
	}

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
		return a.x < b.x || a.x == b.x && a.y < b.y;
	}

	template<typename T> inline
	Rect<T> Rect<T>::FromCoordinates(T x1, T y1, T x2, T y2)
	{
		if(x1 > x2+1) {
			std::swap(x1, x2);
		}

		if(y1 > y2+1) {
			std::swap(y1, y2);
		}
	return Rect(x1, y1, (x2 - x1) + 1, (y2 - y1) + 1);
	}

	template<typename T> inline
	Rect<T>::Rect(T x, T y, T w, T h)
	  : top_left_(std::min(x, x+w), std::min(y, y+h)),
		bottom_right_(std::max(x, x+w), std::max(y, y+h))
	{
	}

	template<typename T> inline
	Rect<T>::Rect(const std::vector<T>& v)
	{
		from_vector(v);
	}

	template<typename T> inline
	Rect<T>::Rect(const variant& v)
	{
		ASSERT_LOG(false, "No template specialisation for Rect<T>(const varaint&)");
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
				*this = Rect<T>::FromCoordinates(items[0], items[1], T(1), T(1));
				break;
			case 3:
				*this = Rect<T>::FromCoordinates(items[0], items[1], items[2], T(1));
				break;
			case 4:
				*this = Rect<T>::FromCoordinates(items[0], items[1], items[2], items[3]);
				break;
			default:
				*this = Rect<T>();
				break;
		}
	}

	template<typename T> inline
	int rect_difference(const Rect<T>& a, const Rect<T>& b, Rect<T>* output)
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
			*output++ = rect(a.x(), a.y(), b.x() - a.x(), a.h());
			}

		if(a.x() + a.w() > b.x() + b.w()) {
			*output++ = rect(b.x() + b.w(), a.y(), (a.x() + a.w()) - (b.x() + b.w()), a.h());
			}

		if(a.y() < b.y()) {
			const int x1 = std::max(a.x(), b.x());
			const int x2 = std::min(a.x() + a.w(), b.x() + b.w());
			*output++ = rect(x1, a.y(), x2 - x1, b.y() - a.y());
			}

		if(a.y() + a.h() > b.y() + b.h()) {
			const int x1 = std::max(a.x(), b.x());
			const int x2 = std::min(a.x() + a.w(), b.x() + b.w());
			*output++ = rect(x1, b.y() + b.h(), x2 - x1, (a.y() + a.h()) - (b.y() + b.h()));
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
		return p.x >= r.x() && p.y >= r.y() && p.x < r.x2() && p.y < r.y2();
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
