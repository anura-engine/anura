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

#include "kre/DisplayDevice.hpp"

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
	current_x_formula_(game_logic::Formula::create_optional_formula(water_node["current_x_formula"])),
	current_y_formula_(game_logic::Formula::create_optional_formula(water_node["current_y_formula"]))
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

	/// XXX Need to set appropriate blend equation/functions on each of these attribute sets
	auto ab = DisplayDevice::CreateAttributeSet(true);
	waterline_.reset(new Attribute<vertex_color>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
	waterline_->AddAttributeDescription(AttributeDesc(AttributeDesc::Type::POSITION, 2, AttributeDesc::VariableType::FLOAT, false, sizeof(vertex_color), offsetof(vertex_color, vertex)));
	waterline_->AddAttributeDescription(AttributeDesc(AttributeDesc::Type::COLOR, 4, AttributeDesc::VariableType::UNSIGNED_BYTE, true, sizeof(vertex_color), offsetof(vertex_color, color)));
	ab->AddAttribute(AttributeBasePtr(waterline_));
	ab->SetDrawMode(AttributeSet::DrawMode::TRIANGLE_STRIP);
	if(DisplayDevice::CheckForFeature(DisplayDeviceCapabilties::BLEND_EQUATION_SEPERATE)) {
		ab->setBlendEquation(BlendEquationConstants::BE_REVERSE_SUBTRACT);
	}
	ab->setBlendMode(BlendModeConstants::BM_ONE, BlendModeConstants::BM_ONE);
	AddAttributeSet(ab);

	auto seg1 = DisplayDevice::CreateAttributeSet(true);
	line1_.reset(new Attribute<vertex_color>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
	line1_->AddAttributeDescription(AttributeDesc(AttributeDesc::Type::POSITION, 2, AttributeDesc::VariableType::FLOAT, false, sizeof(vertex_color), offsetof(vertex_color, vertex)));
	line1_->AddAttributeDescription(AttributeDesc(AttributeDesc::Type::COLOR, 4, AttributeDesc::VariableType::UNSIGNED_BYTE, true, sizeof(vertex_color), offsetof(vertex_color, color)));
	seg1->AddAttribute(AttributeBasePtr(line1_));
	seg1->SetDrawMode(AttributeSet::DrawMode::LINE_STRIP);
	AddAttributeSet(seg1);

	auto seg2 = DisplayDevice::CreateAttributeSet(true);
	line2_.reset(new Attribute<vertex_color>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
	line2_->AddAttributeDescription(AttributeDesc(AttributeDesc::Type::POSITION, 2, AttributeDesc::VariableType::FLOAT, false, sizeof(vertex_color), offsetof(vertex_color, vertex)));
	line2_->AddAttributeDescription(AttributeDesc(AttributeDesc::Type::COLOR, 4, AttributeDesc::VariableType::UNSIGNED_BYTE, true, sizeof(vertex_color), offsetof(vertex_color, color)));
	seg2->AddAttribute(AttributeBasePtr(line2_));
	seg2->SetDrawMode(AttributeSet::DrawMode::LINE_STRIP);
	seg2->setColor(Color(0.0, 0.9, 0.75, 0.5));
	AddAttributeSet(seg2);
}

variant Water::write() const
{
	variant_builder result;
	result.add("zorder", write_zorder(zorder_));
	for(const area& a : areas_) {
		variant_builder area_node;
		area_node.add("rect", a.rect_.write());
		std::vector<variant> color_vec;
		color_vec.reserve(4);
		for(int n = 0; n != 4; ++n) {
			color_vec.push_back(variant(static_cast<int>(a.color_[n])));
		}
		area_node.add("color", variant(&color_vec));
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

KRE::DisplayDeviceDef Water::doAttach(const KRE::DisplayDevicePtr& dd)
{
	KRE::DisplayDeviceDef def(GetAttributeSet()/*, GetUniformSet()*/);
	// XXX
	return def;
}

void Water::preRender(const KRE::WindowManagerPtr& wm)
{
	for(const area& a : areas_) {
		drawArea(a);
	}
}

bool Water::drawArea(const Water::area& a) const
{
	const KRE::Color waterline_color(250, 240, 205, 255);
	const KRE::Color shallowwater_color(0, 51, 61, 140);
	const KRE::Color deepwater_color(0, 51, 61, 153);
	const rectf waterline_rect(a.rect_.x(), a.rect_.y(), a.rect_.w(), 2.0f);
	const rectf underwater_rect = a.rect_.as_type<float>();

	KRE::Color water_color = a.color_;

	if(KRE::DisplayDevice::CheckForFeature(KRE::DisplayDeviceCapabilties::BLEND_EQUATION_SEPERATE)) {
		const double max_color = std::max(water_color.r(), std::max(water_color.g(), water_color.b()));
		water_color.setRed((max_color - water_color.r())/8.0);
		water_color.setGreen((max_color - water_color.g())/8.0);
		water_color.setBlue((max_color - water_color.b())/8.0);
	}

	float vertices[] = {
		waterline_rect.x(), waterline_rect.y(), //shallow water colored
		waterline_rect.x() + waterline_rect.w(), waterline_rect.y(),
		
		waterline_rect.x(), waterline_rect.y() + std::min(100.0f, underwater_rect.h()), //deep water colored
		waterline_rect.x() + waterline_rect.w(), waterline_rect.y() + std::min(100.0f, underwater_rect.h()),
		waterline_rect.x(), underwater_rect.y() + underwater_rect.h(),
		waterline_rect.x() + waterline_rect.w(), underwater_rect.y() + underwater_rect.h()
	};

	std::vector<KRE::vertex_color> water_rect;
	glm::u8vec4 col(water_color.r_int(),water_color.g_int(),water_color.b_int(),water_color.a_int());
	water_rect.emplace_back(glm::vec2(waterline_rect.x(), waterline_rect.y()), col);
	water_rect.emplace_back(glm::vec2(waterline_rect.x() + waterline_rect.w(), waterline_rect.y()), col);
	water_rect.emplace_back(glm::vec2(waterline_rect.x(), waterline_rect.y() + std::min(100.0f, underwater_rect.h())), col);
	water_rect.emplace_back(glm::vec2(waterline_rect.x() + waterline_rect.w(), waterline_rect.y() + std::min(100.0f, underwater_rect.h())), col);
	water_rect.emplace_back(glm::vec2(waterline_rect.x(), underwater_rect.y() + underwater_rect.h()), col);
	water_rect.emplace_back(glm::vec2(waterline_rect.x() + waterline_rect.w(), underwater_rect.y() + underwater_rect.h()), col);
	waterline_->Update(&water_rect);

	// XXX set line width uniform to 2.0

	typedef std::pair<int, int> Segment;

	const int EndSegmentSize = 20;

	for(const Segment& seg : a.surface_segments_) {

		std::vector<KRE::vertex_color> line1;
		line1.emplace_back(glm::vec2(static_cast<float>(seg.first - EndSegmentSize), waterline_rect.y), glm::u8vec4(255, 255, 255, 0));
		line1.emplace_back(glm::vec2(static_cast<float>(seg.first), waterline_rect.y), glm::u8vec4(255, 255, 255, 255));
		line1.emplace_back(glm::vec2(static_cast<float>(seg.second), waterline_rect.y), glm::u8vec4(255, 255, 255, 255));
		line1.emplace_back(glm::vec2(static_cast<float>(seg.second + EndSegmentSize), waterline_rect.y), glm::u8vec4(255, 255, 255, 0));
		line1_->Update(&line1);

		std::vector<KRE::vertex_color> line2;
		line1.emplace_back(glm::vec2(static_cast<float>(seg.first - EndSegmentSize), waterline_rect.y+2.0f), glm::u8vec4(0, 230, 200, 0));
		line1.emplace_back(glm::vec2(static_cast<float>(seg.first), waterline_rect.y+2.0f), glm::u8vec4(0, 230, 200, 128));
		line1.emplace_back(glm::vec2(static_cast<float>(seg.second), waterline_rect.y+2.0f), glm::u8vec4(0, 230, 200, 128));
		line1.emplace_back(glm::vec2(static_cast<float>(seg.second + EndSegmentSize), waterline_rect.y+2.0f), glm::u8vec4(0, 230, 200, 0));
	}

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
