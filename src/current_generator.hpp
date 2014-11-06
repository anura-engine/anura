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
#ifndef CURRENT_GENERATOR_HPP_INCLUDED
#define CURRENT_GENERATOR_HPP_INCLUDED

#include <boost/intrusive_ptr.hpp>

#include "formula_callable.hpp"
#include "geometry.hpp"
#include "variant.hpp"

typedef boost::intrusive_ptr<class current_generator> current_generator_ptr;

class current_generator : public game_logic::formula_callable
{
public:
	static current_generator_ptr create(variant node);

	virtual ~current_generator();

	virtual void generate(int center_x, int center_y, int target_x, int target_y, int target_mass, int* velocity_x, int* velocity_y) = 0;
	virtual variant write() const = 0;
private:
	virtual variant get_value(const std::string& key) const;
};

class radial_current_generator : public current_generator
{
public:
	radial_current_generator(int intensity, int radius);
	explicit radial_current_generator(variant node);
	virtual ~radial_current_generator() {}

	virtual void generate(int center_x, int center_y, int target_x, int target_y, int target_mass, int* velocity_x, int* velocity_y);
	virtual variant write() const;
private:
	int intensity_;
	int radius_;
};

class rect_current_generator : public current_generator
{
public:
	rect_current_generator(const rect& r, int xvelocity, int yvelocity, int strength);
	explicit rect_current_generator(variant node);
	virtual void generate(int center_x, int center_y, int target_x, int target_y, int target_mass, int* velocity_x, int* velocity_y);

	virtual variant write() const;
private:
	rect rect_;
	int xvelocity_, yvelocity_;
	int strength_;
};

#endif
