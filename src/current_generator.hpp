/*
	Copyright (C) 2003-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "intrusive_ptr.hpp"

#include "formula_callable.hpp"
#include "geometry.hpp"
#include "variant.hpp"

typedef ffl::IntrusivePtr<class CurrentGenerator> CurrentGeneratorPtr;

class CurrentGenerator : public game_logic::FormulaCallable
{
public:
	static CurrentGeneratorPtr create(variant node);

	virtual ~CurrentGenerator();

	virtual void generate(int center_x, int center_y, int target_x, int target_y, int target_mass, int* velocity_x, int* velocity_y) = 0;
	virtual variant write() const = 0;
private:
	virtual variant getValue(const std::string& key) const override;
};

class RadialCurrentGenerator : public CurrentGenerator
{
public:
	explicit RadialCurrentGenerator(int intensity, int radius);
	explicit RadialCurrentGenerator(variant node);
	virtual ~RadialCurrentGenerator() {}

	virtual void generate(int center_x, int center_y, int target_x, int target_y, int target_mass, int* velocity_x, int* velocity_y) override;
	virtual variant write() const override;
private:
	int intensity_;
	int radius_;
};

class RectCurrentGenerator : public CurrentGenerator
{
public:
	explicit RectCurrentGenerator(const rect& r, int xvelocity, int yvelocity, int strength);
	explicit RectCurrentGenerator(variant node);
	virtual ~RectCurrentGenerator() {}

	virtual void generate(int center_x, int center_y, int target_x, int target_y, int target_mass, int* velocity_x, int* velocity_y) override;
	virtual variant write() const override;
private:
	rect rect_;
	int xvelocity_, yvelocity_;
	int strength_;
};
