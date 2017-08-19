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

#include <iostream>
#include <math.h>

#include "DisplayDevice.hpp"
#include "Shaders.hpp"

#include "preferences.hpp"
#include "asserts.hpp"
#include "formatter.hpp"
#include "formula.hpp"
#include "level.hpp"
#include "string_utils.hpp"
#include "tile_map.hpp"
#include "variant_utils.hpp"
#include "water.hpp"

namespace 
{
	const int WaterZorder = 15;
}

Water::Water()
  : KRE::SceneObject("water"),
    zorder_(WaterZorder)
{
	init();
}

Water::Water(variant water_node) 
	: KRE::SceneObject("water"),
	zorder_(parse_zorder(water_node["zorder"], variant("water"))),
	current_x_formula_(game_logic::Formula::createOptionalFormula(water_node["current_x_formula"])),
	current_y_formula_(game_logic::Formula::createOptionalFormula(water_node["current_y_formula"]))
{
	for(variant area_node : water_node["area"].as_list()) {
		const rect r(area_node["rect"]);
		KRE::Color color(KRE::Color::colorWhite());
		if(area_node.has_key("color")) {
			color = KRE::Color(area_node["color"]);
		}

		variant obj = area_node["object"];
		areas_.emplace_back(r, color, obj);
	}
	init();
}

void Water::init()
{
	using namespace KRE;

	setShader(ShaderProgram::getProgram("attr_color_shader"));
	auto ab = DisplayDevice::createAttributeSet(true);
	waterline_.reset(new Attribute<vertex_color>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
	waterline_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_color), offsetof(vertex_color, vertex)));
	waterline_->addAttributeDesc(AttributeDesc(AttrType::COLOR, 4, AttrFormat::UNSIGNED_BYTE, true, sizeof(vertex_color), offsetof(vertex_color, color)));
	ab->addAttribute(AttributeBasePtr(waterline_));
	ab->setDrawMode(DrawMode::TRIANGLES);
	/// Set appropriate blend equation/functions on each of these attribute sets
	if(DisplayDevice::checkForFeature(DisplayDeviceCapabilties::BLEND_EQUATION_SEPERATE)) {
		//ab->setBlendEquation(BlendEquation(BlendEquationConstants::BE_REVERSE_SUBTRACT));
	}
	ab->setBlendMode(BlendModeConstants::BM_ONE, BlendModeConstants::BM_ONE);
	addAttributeSet(ab);

	auto seg1 = DisplayDevice::createAttributeSet(true);
	line1_.reset(new Attribute<vertex_color>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
	line1_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_color), offsetof(vertex_color, vertex)));
	line1_->addAttributeDesc(AttributeDesc(AttrType::COLOR, 4, AttrFormat::UNSIGNED_BYTE, true, sizeof(vertex_color), offsetof(vertex_color, color)));
	seg1->addAttribute(AttributeBasePtr(line1_));
	seg1->setDrawMode(DrawMode::LINE_STRIP);
	addAttributeSet(seg1);

	auto seg2 = DisplayDevice::createAttributeSet(true);
	line2_.reset(new Attribute<vertex_color>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
	line2_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_color), offsetof(vertex_color, vertex)));
	line2_->addAttributeDesc(AttributeDesc(AttrType::COLOR, 4, AttrFormat::UNSIGNED_BYTE, true, sizeof(vertex_color), offsetof(vertex_color, color)));
	seg2->addAttribute(AttributeBasePtr(line2_));
	seg2->setDrawMode(DrawMode::LINE_STRIP);
	seg2->setColor(Color(0.0f, 0.9f, 0.75f, 0.5f));
	addAttributeSet(seg2);
}

variant Water::write() const
{
	variant_builder result;
	result.add("zorder", write_zorder(zorder_));
	for(const area& a : areas_) {
		variant_builder area_node;
		area_node.add("rect", a.rect_.write());
		area_node.add("color", a.color_.write());
		area_node.add("object", a.obj_);

		result.add("area", area_node.build());
	}

	return result.build();
}

void Water::addRect(const rect& r, const KRE::Color& color, variant obj)
{
	LOG_INFO("ADD WATER: " << r);
	areas_.emplace_back(r, color, obj);
}

void Water::deleteRect(const rect& r)
{
	for(std::vector<area>::iterator i = areas_.begin(); i != areas_.end(); ) {
		if(r == i->rect_) {
			i = areas_.erase(i);
		} else {
			++i;
		}
	}
}


void Water::addWave(const point& p, double xvelocity, double height, double length, double delta_height, double delta_length)
{
	for(area& a : areas_) {
		if(pointInRect(p, a.rect_)) {
			std::pair<int, int> bounds(a.rect_.x(), a.rect_.x2());
			for(int n = 0; n != a.surface_segments_.size(); ++n) {
				if(p.x >= a.surface_segments_[n].first && p.x <= a.surface_segments_[n].second) {
					bounds = a.surface_segments_[n];
					break;
				}
			}
			wave wv = { (double)p.x, xvelocity, height, length, delta_height, delta_length, bounds.first, bounds.second };
			a.waves_.push_back(wv);
			return;
		}
	}
}

void Water::preRender(const KRE::WindowPtr& wm) const
{
	std::vector<KRE::vertex_color> water_rect;
	std::vector<KRE::vertex_color> line1;
	std::vector<KRE::vertex_color> line2;

	for(const area& a : areas_) {
		drawArea(a, &water_rect, &line1, &line2);
	}

	waterline_->update(&water_rect);
	line1_->update(&line1);
	line2_->update(&line2);
}

bool Water::drawArea(const Water::area& a, std::vector<KRE::vertex_color>* water_rect, std::vector<KRE::vertex_color>* line1, std::vector<KRE::vertex_color>* line2) const
{
	return true;
}

void Water::process(const Level& lvl)
{
	for(area& a : areas_) {
		initAreaSurfaceSegments(lvl, a);

		for(wave& w : a.waves_) {
			w.process();

			//if the wave has hit the edge, then turn it around.
			if(w.xpos < w.left_bound && w.xvelocity < 0) {
				w.xvelocity *= -1.0;
			}

			if(w.xpos > w.right_bound && w.xvelocity > 0) {
				w.xvelocity *= -1.0;
			}
		}

		a.waves_.erase(std::remove_if(a.waves_.begin(), a.waves_.end(), 
			[](const Water::wave& w){ return w.height <= 0.5 || w.length <= 0; }), 
			a.waves_.end());
	}
}

void Water::wave::process() 
{
	xpos += xvelocity;
	height *= 0.996;
	length += delta_length;
}

void Water::getCurrent(const Entity& e, int* velocity_x, int* velocity_y) const
{
	if(velocity_x && current_x_formula_) {
		*velocity_x += current_x_formula_->execute(e).as_int();
	}

	if(velocity_y && current_y_formula_) {
		*velocity_y += current_y_formula_->execute(e).as_int();
	}
}

bool Water::isUnderwater(const rect& r, rect* result_water_area, variant* e) const
{
	//we don't take the vertical midpoint, because doing so can cause problems
	//when objects change their animations and flip between not being
	//underwater. Instead we take the bottom and subtract a hardcoded amount.
	//TODO: potentially review this way of determinining if something is
	//underwater.
	const point p((r.x() + r.x2())/2, r.y2() - 20);
	for(const area& a : areas_) {
		if(pointInRect(p, a.rect_)) {
			if(result_water_area) {
				*result_water_area = a.rect_;
			}

			if(e) {
				*e = a.obj_;
			}
			return true;
		}
	}

	return false;
}

void Water::initAreaSurfaceSegments(const Level& lvl, Water::area& a)
{
	if(a.surface_segments_init_) {
		return;
	}

	a.surface_segments_init_ = true;

	bool prev_solid = true;
	int begin_segment = 0;
	for(int x = a.rect_.x(); x != a.rect_.x2(); ++x) {
		const bool solid = lvl.solid(x, a.rect_.y()) || x == a.rect_.x2()-1;
		if(solid && !prev_solid) {
			a.surface_segments_.push_back(std::make_pair(begin_segment, x));
		} else if(!solid && prev_solid) {
			begin_segment = x;
		}

		prev_solid = solid;
	}
}

Water::area::area(const rect& r, const KRE::Color& color, variant obj)
	: rect_(r), 
	color_(color), 
	surface_segments_init_(false), 
	obj_(obj)
{
}
