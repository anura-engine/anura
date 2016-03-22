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

#pragma once

#include <algorithm>
#include <sstream>
#include <vector>

#include "asserts.hpp"
#include "variant.hpp"

namespace geometry
{
	template<typename T>
	struct Point {
		explicit Point(T x=0, T y=0) : x(x), y(y)
		{}
		explicit Point(const std::string& s);
		explicit Point(const variant& v);
		explicit Point(const std::vector<T>& v);
		variant write() const;
		void clear() { x = T(0); y = T(0); }
		void operator +=(const Point& p) {
			x += p.x;
			y += p.y;
		}
		void operator -=(const Point& p) {
			x -= p.x;
			y -= p.y;
		}
		union {
			struct { T x, y; };
			T buf[2];
		};
	};

	template<typename T> inline
	bool operator==(const Point<T>& a, const Point<T>& b);
	template<typename T> inline
	bool operator!=(const Point<T>& a, const Point<T>& b);
	template<typename T> inline
	bool operator<(const Point<T>& a, const Point<T>& b);
	template<typename T> inline
	Point<T> operator+(const Point<T>& lhs, const Point<T>& rhs);
	template<typename T> inline
	Point<T> operator-(const Point<T>& lhs, const Point<T>& rhs);
	template<typename T, typename D> inline
	Point<T> operator*(const Point<T>& lhs, const Point<D>& rhs);
	template<typename T> inline
	Point<T> operator*(const Point<T>& lhs, float scalar);
	template<typename T> inline
	Point<T> operator*(const Point<T>& lhs, double scalar);

	template<typename T> inline
	std::ostream& operator<<(std::ostream& os, const Point<T>& p);

	template<typename T> inline
	Point<T> normalize(const Point<T>& p);

	template<> inline Point<int>::Point(const variant& v)
	{
		*this = Point<int>(v.as_list_int());
	}

	template<typename T>
	class Rect
	{
	public:
		inline explicit Rect(T x=0, T y=0, T w=0, T h=0);
		inline explicit Rect(const Point<T>& xy, T w=0, T h=0);
		explicit Rect(const std::vector<T>& v);
		explicit Rect(const std::string& s);
		explicit Rect(const variant& v);
		explicit Rect(const Point<T>& p1, const Point<T>& p2) {
			top_left_ = p1;
			bottom_right_ = p2;
		}

	    static Rect<T> fromCoordinates(T x1, T y1, T x2, T y2)
		{
			if(x1 > x2+1) {
				std::swap(x1, x2);
			}

			if(y1 > y2+1) {
				std::swap(y1, y2);
			}
	    return Rect<T>(x1, y1, (x2 - x1) + 1, (y2 - y1) + 1);
		}

		static Rect<T> from_coordinates(T x1, T y1, T x2, T y2) {
			return fromCoordinates(x1,y1,x2,y2);
		}

		void from_vector(const std::vector<T>& v) {
			switch(v.size()) {
				case 2:
					*this = Rect<T>::fromCoordinates(v[0], v[1], v[0], v[1]);
					break;
				case 3:
					*this = Rect<T>::fromCoordinates(v[0], v[1], v[2], v[1]);
					break;
				case 4:
					*this = Rect<T>::fromCoordinates(v[0], v[1], v[2], v[3]);
					break;
				default:
					*this = Rect<T>();
					break;
			}
		}

		std::string toString() const {
			std::stringstream ss;
			ss << x() << "," << y() << "," << (x2()-1) << "," << (y2()-1);
			return ss.str();
		}

		T x() const { return top_left_.x; }
		T y() const { return top_left_.y; }
		T x1() const { return top_left_.x; }
		T y1() const { return top_left_.y; }
		T x2() const { return bottom_right_.x; }
		T y2() const { return bottom_right_.y; }
		T w() const { return bottom_right_.x - top_left_.x; }
		T h() const { return bottom_right_.y - top_left_.y; }
		Point<T> dimensions() const { return Point<T>(w(), h()); }

		T mid_x() const { return (x1() + x2())/static_cast<T>(2); }
		T mid_y() const { return (y1() + y2())/static_cast<T>(2); }

		void set_x(const T& new_x) { top_left_.x = new_x; bottom_right_.x += new_x; }
		void set_y(const T& new_y) { top_left_.y = new_y; bottom_right_.y += new_y; }
		void set_w(const T& new_w) { bottom_right_.x = top_left_.x + new_w; }
		void set_h(const T& new_h) { bottom_right_.y = top_left_.y + new_h; }

		void set_top_left(const T& new_x, const T& new_y) { 
			top_left_.x = new_x; 
			top_left_.y = new_y; 
		}
		void set_xy(const T& new_x, const T& new_y) { set_top_left(new_x, new_y); }
		void set_bottom_right(const T& new_x2, const T& new_y2) { 
			bottom_right_.x = new_x2; 
			bottom_right_.y = new_y2; 
		}
		void set_width_height(const T& new_w, const T& new_h) { 
			bottom_right_.x = top_left_.x + new_w; 
			bottom_right_.y = top_left_.y + new_h; 
		}
		void set_wh(const T& new_w, const T& new_h) { set_width_height(new_w, new_h); }
		void set(const T x, const T y, const T w, const T h) {
			*this = Rect<T>(x,y,w,h);
		}

		Point<T> mid() const { return Point<T>((x1() + x2())/static_cast<T>(2), (y1() + y2())/static_cast<T>(2)); }

		bool empty() const { return w() == 0 || h() == 0; }

		T perimeter(T line_thickness=T()) const { return w()*2+h()*2-line_thickness*2; }

		const Point<T>& top_left() const { return top_left_; }
		const Point<T>& bottom_right() const { return bottom_right_; }

		variant write() const;

		void operator+=(const Point<T>& p) {
			top_left_.x += p.x;
			top_left_.y += p.y;
			bottom_right_.x += p.x;
			bottom_right_.y += p.y;
		}
		void operator-=(const Point<T>& p) {
			top_left_.x -= p.x;
			top_left_.y -= p.y;
			bottom_right_.x -= p.x;
			bottom_right_.y -= p.y;
		}
		void expand(const Point<T>& p) {
			top_left_.x -= p.x;
			top_left_.y -= p.y;
			bottom_right_.x += p.x;
			bottom_right_.y += p.y;
		}
		void contract(const Point<T>& p) {
			expand(Point<T>(-p.x, -p.y));
		}
		void expand(T v) {
			top_left_.x -= v;
			top_left_.y -= v;
			bottom_right_.x += v;
			bottom_right_.y += v;
		}

		template<typename F>
		Rect<F> as_type() const {
			return Rect<F>::from_coordinates(F(top_left_.x), F(top_left_.y), F(bottom_right_.x), F(bottom_right_.y));
		}
	private:
		Point<T> top_left_;
		Point<T> bottom_right_;
	};

	template<> inline
	Rect<float> Rect<float>::fromCoordinates(float x1, float y1, float x2, float y2)
	{
		if(x1 > x2) {
			std::swap(x1, x2);
		}

		if(y1 > y2) {
			std::swap(y1, y2);
		}
		return Rect<float>(x1, y1, x2 - x1, y2 - y1);
	}

	template<> inline
	Rect<float> Rect<float>::from_coordinates(float x1, float y1, float x2, float y2)
	{
		return Rect<float>::fromCoordinates(x1, y1, x2, y2);
	}

	template<> inline
	Rect<double> Rect<double>::fromCoordinates(double x1, double y1, double x2, double y2)
	{
		if(x1 > x2) {
			std::swap(x1, x2);
		}

		if(y1 > y2) {
			std::swap(y1, y2);
		}
		return Rect<double>(x1, y1, x2 - x1, y2 - y1);
	}

	template<> inline
	Rect<double> Rect<double>::from_coordinates(double x1, double y1, double x2, double y2)
	{
		return Rect<double>::fromCoordinates(x1, y1, x2, y2);
	}

	template<typename T> inline
	bool operator<(const Rect<T>& a, const Rect<T>& b);

	template<typename T, typename D> inline
	Rect<T> operator*(const Rect<T>& lhs, D scalar);

	template<typename T, typename D> inline
	Rect<D> operator/(const Rect<T>& lhs, D scalar);

	template<typename T> inline
	Rect<T> operator*(const Rect<T>& lhs, const Point<T>& scalar);

	template<typename T> inline
	Rect<T> operator+(const Rect<T>& r, const Point<T>& p);

	template<> inline 
	Rect<int>::Rect(const variant& v)
	{
		if(v.is_list()) {
			from_vector(v.as_list_int());
			return;
		} else if(v.is_map()) {
			ASSERT_LOG((v.has_key("x") && v.has_key("y") && v.has_key("w") && v.has_key("h"))
				|| (v.has_key("x1") && v.has_key("y1") && v.has_key("x2") && v.has_key("y2")), 
				"map must have 'x','y','w','h' or 'x1','y1','x2','y2' attributes.");
			if(v.has_key("x")) {
				*this = Rect<int>(v["x"].as_int32(),v["y"].as_int32(),v["w"].as_int32(),v["h"].as_int32());
			} else {
				*this = Rect<int>::fromCoordinates(v["x1"].as_int32(),v["y1"].as_int32(),v["x2"].as_int32(),v["y2"].as_int32());
			}
		} else {
			ASSERT_LOG(false, "Creating a rect from a variant must be list or map");
		}
	}

	template<> inline 
	Rect<float>::Rect(const variant& v)
	{
		if(v.is_list()) {
			std::vector<float> vec;
			for(size_t n = 0; n != v.num_elements(); ++n) {
				vec.push_back(v[n].as_float());
			}
			from_vector(vec);
			return;
		} else if(v.is_map()) {
			ASSERT_LOG((v.has_key("x") && v.has_key("y") && v.has_key("w") && v.has_key("h"))
				|| (v.has_key("x1") && v.has_key("y1") && v.has_key("x2") && v.has_key("y2")), 
				"map must have 'x','y','w','h' or 'x1','y1','x2','y2' attributes.");
			if(v.has_key("x")) {
				*this = Rect<float>(v["x"].as_float(),v["y"].as_float(),v["w"].as_float(),v["h"].as_float());
			} else {
				*this = Rect<float>::fromCoordinates(v["x1"].as_float(),v["y1"].as_float(),v["x2"].as_float(),v["y2"].as_float());
			}
		} else {
			ASSERT_LOG(false, "Creating a rect from a variant must be list or map");
		}
	}
}

#include "geometry.inl"

typedef geometry::Point<int> point;
typedef geometry::Point<float> pointf;

typedef geometry::Rect<int> rect;
typedef geometry::Rect<float> rectf;
