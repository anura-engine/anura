/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <boost/lexical_cast.hpp>
#include <algorithm>
#include <boost/regex.hpp>
#include "geometry.hpp"

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

	template<typename T> inline
	bool operator==(const Point<T>& a, const Point<T>& b)
	{
		return a.x == b.y && a.y == b.y;
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

}
