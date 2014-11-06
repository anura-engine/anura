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
#ifndef GEOMETRY_HPP_INCLUDED
#define GEOMETRY_HPP_INCLUDED

#include "graphics.hpp"
#include "variant.hpp"

#include <iostream>
#include <string>
#include <vector>

struct point {
	explicit point(const variant& v);
	explicit point(const std::string& str);
	explicit point(int x=0, int y=0) : x(x), y(y)
	{}

	explicit point(const std::vector<int>& v);

	variant write() const;
	std::string to_string() const;

	union {
		struct { int x, y; };
		int buf[2];
	};
};

bool operator==(const point& a, const point& b);
bool operator!=(const point& a, const point& b);
bool operator<(const point& a, const point& b);

namespace game_logic {
class formula_callable;
}

class rect {
public:
	static rect from_coordinates(int x1, int y1, int x2, int y2);
	explicit rect(const std::string& str);
	explicit rect(int x=0, int y=0, int w=0, int h=0);
	explicit rect(const std::vector<int>& v);
	explicit rect(const variant& v);
	int x() const;
	int y() const;
	int x2() const;
	int y2() const;
	int w() const;
	int h() const;

	int mid_x() const { return (x() + x2())/2; }
	int mid_y() const { return (y() + y2())/2; }

	const point& top_left() const { return top_left_; }
	const point& bottom_right() const { return bottom_right_; }

	std::string to_string() const;
	variant write() const;

	SDL_Rect sdl_rect() const;

	bool empty() const { return w() == 0 || h() == 0; }

	game_logic::formula_callable* callable() const;

	void operator+=(const point& p) {
		top_left_.x += p.x;
		top_left_.y += p.y;
		bottom_right_.x += p.x;
		bottom_right_.y += p.y;
	}
	void operator-=(const point& p) {
		top_left_.x -= p.x;
		top_left_.y -= p.y;
		bottom_right_.x -= p.x;
		bottom_right_.y -= p.y;
	}
private:
	point top_left_, bottom_right_;
};

// This is a helper class that has the internal representation in floating point co-ordinates.
class rectf
{
public:
	static rectf from_coordinates(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
	static rectf from_area(GLfloat x, GLfloat y, GLfloat w, GLfloat h);
	explicit rectf(const std::string& str);
	explicit rectf(int x=0, int y=0, int w=0, int h=0);
	explicit rectf(GLfloat x, GLfloat y, GLfloat w, GLfloat h);
	explicit rectf(const std::vector<GLfloat>& v);
	explicit rectf(const std::vector<int>& v);
	explicit rectf(const variant& value);
	int x() const { return int(x_); }
	int y() const { return int(y_); }
	int x2() const { return int(x2_); }
	int y2() const { return int(y2_); }
	int w() const { return int(w_); }
	int h() const { return int(h_); }

	GLfloat xf() const { return x_; }
	GLfloat yf() const { return y_; }
	GLfloat x2f() const { return x2_; }
	GLfloat y2f() const { return y2_; }
	GLfloat wf() const { return w_; }
	GLfloat hf() const { return h_; }

	int mid_x() const { return (x() + x2())/2; }
	int mid_y() const { return (y() + y2())/2; }

	std::string to_string() const;
	SDL_Rect sdl_rect() const { SDL_Rect r = {static_cast<Sint16>(x()), static_cast<Sint16>(y()), static_cast<Uint16>(w()), static_cast<Uint16>(h())}; return r; }

	bool empty() const { return w() == 0 || h() == 0; }
private:
	GLfloat x_;
	GLfloat y_;
	GLfloat w_;
	GLfloat h_;
	GLfloat x2_;
	GLfloat y2_;
};



bool point_in_rect(const point& p, const rect& r);
bool rects_intersect(const rect& a, const rect& b);
rect intersection_rect(const rect& a, const rect& b);
int rect_difference(const rect& a, const rect& b, rect* output); //returns a vector containing the parts of A that don't intersect B

rect rect_union(const rect& a, const rect& b);

inline bool operator==(const rect& a, const rect& b) {
	return a.x() == b.x() && a.y() == b.y() && a.w() == b.w() && a.h() == b.h();
}

inline bool operator!=(const rect& a, const rect& b) {
	return !operator==(a, b);
}

std::ostream& operator<<(std::ostream& s, const rect& r);

#endif
