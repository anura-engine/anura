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

#include <limits>

#include "Canvas.hpp"
#include "poly_line_widget.hpp"

namespace gui
{
	PolyLineWidget::PolyLineWidget(const std::vector<point>& points, const KRE::Color& c, float width)
		: color_(c), width_(width)
	{
		for(auto& p : points) {
			points_.emplace_back(static_cast<float>(p.x), static_cast<float>(p.y));
		}
		calcCoords();
	}

	PolyLineWidget::PolyLineWidget(const point& p1, const point& p2, const KRE::Color& c, float width)
		: color_(c), width_(width)
	{
		points_.emplace_back(static_cast<float>(p1.x), static_cast<float>(p1.y));
		points_.emplace_back(static_cast<float>(p2.x), static_cast<float>(p2.y));
		calcCoords();
	}

	PolyLineWidget::PolyLineWidget(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v,e)
	{
		width_ = v.has_key("width") ? v["width"].as_int() : 1.0f;
		color_ = v.has_key("color") 
			? KRE::Color(v["color"]) 
			: KRE::Color::colorWhite();
		if(v.has_key("points")) {
			for(const variant& pp : v["points"].as_list()) {
				points_.emplace_back(pp[0].as_float(), pp[1].as_float());
			}
			calcCoords();
		}
	}
	
	PolyLineWidget::~PolyLineWidget()
	{
	}

	void PolyLineWidget::addPoint(const glm::vec2& p)
	{
		points_.emplace_back(p);
		calcCoords();
	}

	bool PolyLineWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
		return claimed;
	}

	void PolyLineWidget::handleDraw() const
	{
		KRE::Canvas::getInstance()->drawLineStrip(points_, width_, color_);
	}

	void PolyLineWidget::calcCoords()
	{
		float min_x = std::numeric_limits<float>::max();
		float max_x = std::numeric_limits<float>::min();
		float min_y = std::numeric_limits<float>::max();
		float max_y = std::numeric_limits<float>::min();
		for(const glm::vec2& p : points_) {
			if(p.x < min_x) {
				min_x = p.x;
			}
			if(p.x > max_x) {
				max_x = p.x;
			}
			if(p.y < min_y) {
				min_y = p.y;
			}
			if(p.y > max_y) {
				max_y = p.y;
			}
		}
		setLoc(static_cast<int>(min_x), static_cast<int>(min_y));
		setDim(static_cast<int>(max_x-min_x), static_cast<int>(max_y-min_y));
	}

	BEGIN_DEFINE_CALLABLE(PolyLineWidget, Widget)
		DEFINE_FIELD(points, "[[int,int]]")
			std::vector<variant> v;
			for(auto& p : obj.points_) {
				std::vector<variant> pp;
				pp.emplace_back(variant(p.x));
				pp.emplace_back(variant(p.y));
				v.emplace_back(variant(&pp));
			}
			return variant(&v);
		DEFINE_SET_FIELD
			obj.points_.clear();
			for(const variant& pp : value.as_list()) {				
				obj.points_.emplace_back(pp[0].as_float(), pp[1].as_float());
			}
			obj.calcCoords();
		
		DEFINE_FIELD(width, "decimal")
			return variant(obj.width_);
		DEFINE_SET_FIELD_TYPE("int|decimal")
			obj.width_ = value.as_float();
		
		DEFINE_FIELD(color, "[int,int,int,int]")
			return obj.color_.write();
		DEFINE_SET_FIELD_TYPE("[int,int,int]|[int,int,int,int]|string") // Can also me a map
			obj.color_ = KRE::Color(value);
	END_DEFINE_CALLABLE(PolyLineWidget)

}
