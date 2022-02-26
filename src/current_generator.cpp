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

#include <iostream>
#include <math.h>

#include "current_generator.hpp"
#include "formatter.hpp"
#include "logger.hpp"
#include "variant.hpp"
#include "variant_utils.hpp"

CurrentGeneratorPtr CurrentGenerator::create(variant node)
{
	const std::string& type = node["type"].as_string();
	if(type == "radial") {
		return CurrentGeneratorPtr(new RadialCurrentGenerator(node));
	} else if(type == "rect") {
		return CurrentGeneratorPtr(new RectCurrentGenerator(node));
	} else {
		return nullptr;
	}
}

CurrentGenerator::~CurrentGenerator() {
}

variant CurrentGenerator::getValue(const std::string& key) const
{
	return variant();
}

RadialCurrentGenerator::RadialCurrentGenerator(int intensity, int radius)
  : intensity_(intensity), radius_(radius)
{}

RadialCurrentGenerator::RadialCurrentGenerator(variant node)
  : intensity_(node["intensity"].as_int()),
    radius_(node["radius"].as_int())
{}

void RadialCurrentGenerator::generate(int center_x, int center_y, int target_x, int target_y, int target_mass, int* velocity_x, int* velocity_y) {
	if(center_x == target_x && center_y == target_y) {
		return;
	}

	const float xdiff = static_cast<float>(target_x - center_x);
	const float ydiff = static_cast<float>(target_y - center_y);
	if(std::abs(xdiff) >= radius_ || std::abs(ydiff) > radius_) {
		return;
	}

	const float distance = sqrt(xdiff*xdiff + ydiff*ydiff);
	if(distance >= radius_) {
		return;
	}

	const float intensity = intensity_*(1.0f - distance/radius_);
	const float xdiff_normalized = xdiff/(std::abs(xdiff) + std::abs(ydiff));
	const float ydiff_normalized = ydiff/(std::abs(xdiff) + std::abs(ydiff));

	LOG_INFO("DO_CURRENT: " << center_x << "," << center_y << " ~ " << target_x << "," << target_y << ": "<< intensity << " x " << xdiff_normalized << "," << ydiff_normalized);
	*velocity_x += static_cast<int>(xdiff_normalized*intensity);
	*velocity_y += static_cast<int>(ydiff_normalized*intensity);
}

variant RadialCurrentGenerator::write() const
{
	variant_builder result;
	result.add("type", "radial");
	result.add("intensity", formatter() << intensity_);
	result.add("radius", formatter() << radius_);
	return result.build();
}

RectCurrentGenerator::RectCurrentGenerator(const rect& r, int xvelocity, int yvelocity, int strength)
  : rect_(r), xvelocity_(xvelocity), yvelocity_(yvelocity), strength_(strength)
{}

RectCurrentGenerator::RectCurrentGenerator(variant node)
  : rect_(node["rect"].as_string()), xvelocity_(node["xvelocity"].as_int()), yvelocity_(node["yvelocity"].as_int()), strength_(node["strength"].as_int())
{}

void RectCurrentGenerator::generate(int center_x, int center_y, int target_x, int target_y, int target_mass, int* velocity_x, int* velocity_y)
{
	const int strength = strength_;
	if(pointInRect(point(target_x, target_y), rect_)) {
		if(xvelocity_ > 0 && *velocity_x < xvelocity_) {
			int amount = (xvelocity_ - std::max(0, *velocity_x))*strength/(target_mass*1000);
			const int distance = rect_.x2() - target_x;
			amount = (amount*distance*distance)/(rect_.h()*rect_.h());
			*velocity_x += amount;
			if(*velocity_x > xvelocity_) {
				*velocity_x = xvelocity_;
			}
		} else if(xvelocity_ < 0 && *velocity_x > xvelocity_) {
			int amount = (xvelocity_ - std::min(0, *velocity_x))*strength/(target_mass*1000);
			const int distance = target_x - rect_.x();
			amount = (amount*distance*distance)/(rect_.h()*rect_.h());
			*velocity_x += amount;
			if(*velocity_x < xvelocity_) {
				*velocity_x = xvelocity_;
			}
		}

		if(yvelocity_ > 0 && *velocity_y < yvelocity_) {
			int amount = (yvelocity_ - std::max(0, *velocity_y))*strength/(target_mass*1000);
			const int distance = rect_.y2() - target_y;
			amount = (amount*distance*distance)/(rect_.h()*rect_.h());
			*velocity_y += amount;
			if(*velocity_y > yvelocity_) {
				*velocity_y = yvelocity_;
			}
		} else if(yvelocity_ < 0 && *velocity_y > yvelocity_) {
			int amount = yvelocity_*strength/(target_mass*1000);
			const int distance = target_y - rect_.y();
			//LOG_INFO("DIST: " << distance << "/" << rect_.h() << " " << *velocity_y);
			if(distance < rect_.h()/2 && *velocity_y > 0) {
				//LOG_INFO("CANCEL");
				amount = 0;
			}
			*velocity_y += amount;
		}
	}
}

variant RectCurrentGenerator::write() const
{
	variant_builder node;
	node.add("type", "rect");
	node.add("rect", rect_.write());
	node.add("xvelocity", formatter() << xvelocity_);
	node.add("yvelocity", formatter() << yvelocity_);
	node.add("strength", formatter() << strength_);
	return node.build();
}
