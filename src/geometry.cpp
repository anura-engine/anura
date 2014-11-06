/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#include <vector>

#include <algorithm>
#include "formatter.hpp"
#include "formula_callable.hpp"
#include "geometry.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"

point::point(const variant& v)
{
	*this = point(v.as_list_int());
}

point::point(const std::string& str)
{
	int buf_size = 2;
	util::split_into_ints(str.c_str(), buf, &buf_size);
	if(buf_size != 2) {
		x = y = 0;
	}
}

point::point(const std::vector<int>& v)
{
	if(v.empty()) {
		x = y = 0;
	} else if(v.size() == 1) {
		x = v[0];
		y = 0;
	} else {
		x = v[0];
		y = v[1];
	}
}

variant point::write() const
{
	std::vector<variant> v;
	v.reserve(2);
	v.push_back(variant(x));
	v.push_back(variant(y));
	return variant(&v);
}

std::string point::to_string() const
{
	return formatter() << x << "," << y;
}

bool operator==(const point& a, const point& b) {
	return a.x == b.x && a.y == b.y;
}

bool operator!=(const point& a, const point& b) {
	return !operator==(a, b);
}

bool operator<(const point& a, const point& b) {
	return a.x < b.x || a.x == b.x && a.y < b.y;
}

std::string rect::to_string() const
{
	return formatter() << x() << "," << y() << "," << (x2()-1) << "," << (y2()-1);
}

SDL_Rect rect::sdl_rect() const
{
	SDL_Rect r = {x(), y(), w(), h()};
	return r;
}

rect rect::from_coordinates(int x1, int y1, int x2, int y2)
{
	if(x1 > x2+1) {
		std::swap(x1, x2);
	}

	if(y1 > y2+1) {
		std::swap(y1, y2);
	}

	return rect(x1, y1, (x2 - x1) + 1, (y2 - y1) + 1);
}

rect::rect(const std::string& str)
{
	if(str.empty()) {
		*this = rect();
		return;
	}

	int items[4];
	int num_items = 4;
	util::split_into_ints(str.c_str(), items, &num_items);

	switch(num_items) {
	case 2:
		*this = rect::from_coordinates(items[0], items[1], 1, 1);
		break;
	case 3:
		*this = rect::from_coordinates(items[0], items[1], items[2], 1);
		break;
	case 4:
		*this = rect::from_coordinates(items[0], items[1], items[2], items[3]);
		break;
	default:
		*this = rect();
		break;
	}
}

rect::rect(int x, int y, int w, int h)
  : top_left_(std::min(x, x+w), std::min(y, y+h)),
    bottom_right_(std::max(x, x+w), std::max(y, y+h))
{
}

rect::rect(const std::vector<int>& v)
{
	switch(v.size()) {
	case 2:
		*this = rect::from_coordinates(v[0], v[1], v[0], v[1]);
		break;
	case 3:
		*this = rect::from_coordinates(v[0], v[1], v[2], v[1]);
		break;
	case 4:
		*this = rect::from_coordinates(v[0], v[1], v[2], v[3]);
		break;
	default:
		*this = rect();
		break;
	}
}

rect::rect(const variant& value)
{
	std::vector<int> v = value.as_list_int();
	*this = rect(v);
}

variant rect::write() const
{
	std::vector<variant> v;
	v.reserve(4);
	v.push_back(variant(x()));
	v.push_back(variant(y()));
	v.push_back(variant(x2()-1));
	v.push_back(variant(y2()-1));
	return variant(&v);
}

int rect::x() const
{
	return top_left_.x;
}

int rect::y() const
{
	return top_left_.y;
}

int rect::x2() const
{
	return bottom_right_.x;
}

int rect::y2() const
{
	return bottom_right_.y;
}

int rect::w() const
{
	return bottom_right_.x - top_left_.x;
}

int rect::h() const
{
	return bottom_right_.y - top_left_.y;
}

bool point_in_rect(const point& p, const rect& r)
{
	return p.x >= r.x() && p.y >= r.y() && p.x < r.x2() && p.y < r.y2();
}

bool rects_intersect(const rect& a, const rect& b)
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

rect intersection_rect(const rect& a, const rect& b)
{
	const int x = std::max(a.x(), b.x());
	const int y = std::max(a.y(), b.y());
	const int w = std::max(0, std::min(a.x2(), b.x2()) - x);
	const int h = std::max(0, std::min(a.y2(), b.y2()) - y);
	return rect(x, y, w, h);
}

int rect_difference(const rect& a, const rect& b, rect* output)
{
	if (rects_intersect(a,b) == false){  //return empty if there's no intersection
		return -1;
	}
		
	/* returning 4 rectangles in this orientation:
	 _________
	 | |___| |
	 | |   | |
	 | |___| |
	 |_|___|_|  */

	const rect* begin_output = output;

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

rect rect_union(const rect& a, const rect& b)
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

	return rect(x, y, x2 - x, y2 - y);
}

std::ostream& operator<<(std::ostream& s, const rect& r)
{
	s << "rect(" << r.x() << ", " << r.y() << ", " << r.x2() << ", " << r.y2() << ")";
	return s;
}

class rect_obj : public game_logic::formula_callable
{
	DECLARE_CALLABLE(rect_obj);
	rect rect_;
public:
	explicit rect_obj(const rect& r) : rect_(r)
	{}
};

BEGIN_DEFINE_CALLABLE_NOBASE(rect_obj)
DEFINE_FIELD(x, "int")
	return variant(obj.rect_.x());
DEFINE_FIELD(y, "int")
	return variant(obj.rect_.y());
DEFINE_FIELD(x2, "int")
	return variant(obj.rect_.x2());
DEFINE_FIELD(y2, "int")
	return variant(obj.rect_.y2());
DEFINE_FIELD(w, "int")
	return variant(obj.rect_.w());
DEFINE_FIELD(h, "int")
	return variant(obj.rect_.h());
END_DEFINE_CALLABLE(rect_obj)

game_logic::formula_callable* rect::callable() const
{
	return new rect_obj(*this);
}

rectf rectf::from_coordinates(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	if(x1 > x2+1) {
		std::swap(x1, x2);
	}

	if(y1 > y2+1) {
		std::swap(y1, y2);
	}
	return rectf(x1, y1, x2-x1+1, y2-y1+1);
}

rectf rectf::from_area(GLfloat x, GLfloat y, GLfloat w, GLfloat h)
{
	return rectf(x, y, w, h);
}

rectf::rectf(const std::string& str)
{
	if(str.empty()) {
		*this = rectf();
		return;
	}

	GLfloat items[4];
	int num_items = 0;
	std::vector<std::string> buf = util::split(str, ",");
	for(int n = 0; n != 4 && n != buf.size(); ++n) {
		items[num_items++] = boost::lexical_cast<GLfloat>(buf[n]);
	}

	switch(num_items) {
	case 2:
		*this = rectf::from_coordinates(items[0], items[1], 1, 1);
		break;
	case 3:
		*this = rectf::from_coordinates(items[0], items[1], items[2], 1);
		break;
	case 4:
		*this = rectf::from_coordinates(items[0], items[1], items[2], items[3]);
		break;
	default:
		*this = rectf();
		break;
	}
}

rectf::rectf(int x, int y, int w, int h)
	: x_(std::min(x, x+w)), y_(std::min(y, y+h)),
	x2_(std::max(x, x+w)), y2_(std::max(y, y+h)),
	w_(w), h_(h)
{}

rectf::rectf(GLfloat x, GLfloat y, GLfloat w, GLfloat h)
	: x_(std::min(x, x+w)), y_(std::min(y, y+h)),
	x2_(std::max(x, x+w)), y2_(std::max(y, y+h)),
	w_(w), h_(h)
{
}

rectf::rectf(const std::vector<GLfloat>& v)
{
	switch(v.size()) {
	case 2:
		*this = rectf::from_area(v[0], v[1], 0, 0);
		break;
	case 3:
		*this = rectf::from_area(v[0], v[1], v[2], 0);
		break;
	case 4:
		*this = rectf::from_area(v[0], v[1], v[2], v[3]);
		break;
	default:
		*this = rectf();
		break;
	}
}

rectf::rectf(const std::vector<int>& v)
{
	switch(v.size()) {
	case 2:
		*this = rectf::from_area(v[0], v[1], 0, 0);
		break;
	case 3:
		*this = rectf::from_area(v[0], v[1], v[2], 0);
		break;
	case 4:
		*this = rectf::from_area(v[0], v[1], v[2], v[3]);
		break;
	default:
		*this = rectf();
		break;
	}
}

rectf::rectf(const variant& value)
{
	std::vector<decimal> v = value.as_list_decimal();
	switch(v.size()) {
	case 2:
		*this = rectf::from_area(GLfloat(v[0].as_float()), GLfloat(v[1].as_float()), 0, 0);
		break;
	case 3:
		*this = rectf::from_area(GLfloat(v[0].as_float()), GLfloat(v[1].as_float()), GLfloat(v[2].as_float()), 0);
		break;
	case 4:
		*this = rectf::from_area(GLfloat(v[0].as_float()), GLfloat(v[1].as_float()), GLfloat(v[2].as_float()), GLfloat(v[3].as_float()));
		break;
	default:
		*this = rectf();
		break;
	}
}

std::string rectf::to_string() const
{
	std::stringstream ss;
	ss << x() << "," << y() << "," << (x2()-1) << "," << (y2()-1);
	return ss.str();
}


UNIT_TEST(rect)
{
	rect r(10, 10, 10, 10);
	rect r2(r.to_string());
	CHECK_EQ(r, r2);

	r = rect(10, 10, 10, 0);
	CHECK_NE(true, point_in_rect(point(15, 9), r));
	CHECK_NE(true, point_in_rect(point(15, 10), r));
	CHECK_NE(true, point_in_rect(point(15, 11), r));
	CHECK_EQ(r.h(), 0);
}

UNIT_TEST(rect_difference)
{
	rect r(100, 100, 200, 400);
	rect buf[4];

	CHECK_EQ(rect_difference(r, rect(0, 0, 100, 100), buf), -1);

	CHECK_EQ(rect_difference(r, rect(0, 0, 200, 1000), buf), 1);
	CHECK_EQ(buf[0], rect(200, 100, 100, 400));

	CHECK_EQ(rect_difference(r, rect(0, 0, 1000, 1000), buf), 0);

	CHECK_EQ(rect_difference(r, rect(150, 150, 50, 50), buf), 4);
	CHECK_EQ(buf[0], rect(100, 100, 50, 400));
	CHECK_EQ(buf[1], rect(200, 100, 100, 400));
	CHECK_EQ(buf[2], rect(150, 100, 50, 50));

	CHECK_EQ(rect_difference(rect(0, 891, 800, 1491), rect(-32, 1344, 1120, 2432), buf), 1);
	CHECK_EQ(buf[0], rect(0, 891, 800, 453));
}

UNIT_TEST(rect_intersect)
{
	rect r1(0, 0, 802, 610);
	rect r2(0, -128, 800, 64);
	rect r3 = intersection_rect(r1, r2);
	CHECK_EQ(r3.h(), 0);

	CHECK_EQ(r3, intersection_rect(r2, r1));
}

BENCHMARK(benchmark_rect_str)
{
	static const std::string str = "45,89,100, 120";
	BENCHMARK_LOOP {
		const rect r(str);
	}
}
