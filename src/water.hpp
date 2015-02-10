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
#pragma once

#include <vector>

#include "Color.hpp"
#include "geometry.hpp"
#include "SceneObject.hpp"
#include "SceneUtil.hpp"

#include "entity_fwd.hpp"
#include "formula_fwd.hpp"
#include "variant.hpp"

class Level;

class Water : public KRE::SceneObject
{
public:
	Water();
	explicit Water(variant node);

	variant write() const;

	void addRect(const rect& r, const KRE::Color& color, variant obj);
	void deleteRect(const rect& r);

	int zorder() const { return zorder_; }

	void process(const Level& lvl);

	void getCurrent(const Entity& e, int* velocity_x, int* velocity_y) const;

	bool isUnderwater(const rect& r, rect* water_area=NULL, variant* obj=NULL) const;

	void addWave(const point& p, double xvelocity, double height, double length, double delta_height, double delta_length);

	struct wave {
		double xpos;
		double xvelocity;
		double height;
		double length;
		double delta_height;
		double delta_length;

		int left_bound, right_bound;

		void process();
	};
	
	void preRender(const KRE::WindowManagerPtr& wm) override;
private:
	void init();

	struct area {
		area(const rect& r, const KRE::Color& color, variant obj);
		rect rect_;
		std::vector<char> draw_detection_buf_;

		std::vector<wave> waves_;

		//segments of the surface without solid.
		std::vector<std::pair<int, int>> surface_segments_;
		bool surface_segments_init_;

		KRE::Color color_;
		variant obj_;
	};

	std::vector<area> areas_;

	static void initAreaSurfaceSegments(const Level& lvl, area& a);

	bool drawArea(const area& a) const;

	int zorder_;

	std::shared_ptr<KRE::Attribute<KRE::vertex_color>> waterline_;
	std::shared_ptr<KRE::Attribute<KRE::vertex_color>> line1_;
	std::shared_ptr<KRE::Attribute<KRE::vertex_color>> line2_;

	enum { BadOffset = -100000 };

	game_logic::ConstFormulaPtr current_x_formula_, current_y_formula_;
};
