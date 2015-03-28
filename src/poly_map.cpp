/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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
#include <memory>

#include "Canvas.hpp"

#include "poly_map.hpp"
#include "simplex_noise.hpp"
#include "VoronoiDiagramGenerator.h"

namespace geometry
{
	namespace
	{
		static bool draw_borders = true;

		// XXX: centralise the hsv->rgb, rgb->hsv conversion functions 
		// somewhere (maybe add to graphics::color as well)
		struct rgb
		{
			uint8_t r, g, b;
		};

		struct hsv
		{
			uint8_t h, s, v;
		};

		hsv rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b)
		{
			hsv out;
			uint8_t min_color, max_color, delta;

			min_color = std::min(r, std::min(g, b));
			max_color = std::max(r, std::max(g, b));

			delta = max_color - min_color;
			out.v = max_color;
			if(out.v == 0) {
				out.s = 0;
				out.h = 0;
				return out;
			}

			out.s = uint8_t(255.0 * delta / out.v);
			if(out.s == 0) {
				out.h = 0;
				return out;
			}

			if(r == max_color) {
				out.h = uint8_t(43.0 * (g-b)/delta);
			} else if(g == max_color) {
				out.h = 85 + uint8_t(43.0 * (b-r)/delta);
			} else {
				out.h = 171 + uint8_t(43.0 * (r-g)/delta);
			}
			return out;
		}

		rgb hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v)
		{
			rgb out;
			uint8_t region, remainder, p, q, t;

			if(s == 0) {
				out.r = out.g = out.b = v;
			} else {
				region = h / 43;
				remainder = (h - (region * 43)) * 6; 

				p = (v * (255 - s)) >> 8;
				q = (v * (255 - ((s * remainder) >> 8))) >> 8;
				t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

				switch(region)
				{
					case 0:  out.r = v; out.g = t; out.b = p; break;
					case 1:  out.r = q; out.g = v; out.b = p; break;
					case 2:  out.r = p; out.g = v; out.b = t; break;
					case 3:  out.r = p; out.g = q; out.b = v; break;
					case 4:  out.r = t; out.g = p; out.b = v; break;
					default: out.r = v; out.g = p; out.b = q; break;
				}
			}
			return out;
		}
	}

	namespace voronoi
	{
		Wrapper::Wrapper(const fpoint_list& pts, int relaxations, double left, double top, double right, double bottom)
			: left_(left), right_(right), top_(top), bottom_(bottom)
		{
			if(left == 0 && right == 0 && top == 0 && bottom == 0) {
				left_ = std::numeric_limits<double>::max();
				top_ = std::numeric_limits<double>::max();
				right_ = std::numeric_limits<double>::min();
				bottom_ = std::numeric_limits<double>::min();
				calculateBoundingBox(pts);
			}

			ASSERT_LOG(relaxations > 0, "Number of relaxation cycles must be at least 1: " << relaxations);

			sites_.assign(pts.begin(), pts.end());
			for(int n = 0; n != relaxations; ++n) {
				generate(sites_);
			}
		}

		Wrapper::~Wrapper()
		{
		}

		void Wrapper::generate(fpoint_list& pts)
		{
			polygons_.clear();
			std::unique_ptr<SourcePoint[]> srcpts(new SourcePoint[pts.size()]);

			for(int n = 0; n != pts.size(); ++n) {
				srcpts[n].x = pts[n].x;
				srcpts[n].y = pts[n].y;
				srcpts[n].id = n;
				srcpts[n].weight = 0.0;
			}

			VoronoiDiagramGenerator v;
			v.generateVoronoi(srcpts.get(), static_cast<int>(pts.size()), static_cast<float>(left_), static_cast<float>(right_), static_cast<float>(top_), static_cast<float>(bottom_));
			for(int n = 0; n != pts.size(); ++n) {
				int npoints = 0;
				PolygonPoint* pp = nullptr;
				v.getSitePoints(n, &npoints, &pp);
				Polygon* poly = new Polygon(n);
				for(int m = 0; m != npoints; ++m) {
					poly->addPoint(pp[m].coord.x, pp[m].coord.y);
				}
				poly->normalise();
				poly->calculateCentroid(pts[n]);
				poly->setCentroid(pts[n]);
				polygons_.emplace_back(poly);
			}
		}

		void Wrapper::calculateBoundingBox(const fpoint_list& pts)
		{
			for(auto& pt : pts) {
				if(pt.x < left_) {
					left_ = pt.x;
				}
				if(pt.x > right_) {
					right_ = pt.x;
				}
				if(pt.y < top_) {
					top_ = pt.y;
				}
				if(pt.y > bottom_) {
					bottom_ = pt.y;
				}
			}
			// enlarge the bounding box a little.
			double dx = (right_-left_+1.0)/5.0;
			double dy = (bottom_-top_+1.0)/5.0;
			left_ -= dx;
			right_ += dx;
			top_ -= dy;
			bottom_ += dy;
		}

		std::ostream& operator<<(std::ostream& os, const Wrapper& obj)
		{
			os << "Bounding box: " << obj.left() << "," << obj.top() << "," << obj.right() << "," << obj.bottom() << std::endl;
			auto& segs = obj.getEdges();
			for(auto& s : segs) {
				os << s.p1.x << "," << s.p1.y << " " << s.p2.x << "," << s.p2.y << std::endl;
			}
			return os;
		}
	}

	std::ostream& operator<<(std::ostream& os, const Polygon& poly)
	{
		os << "POLYGON(" << poly.getId() << "," << poly.getPoints().size() << "," << poly.height() << ") :" << std::endl;
		for(auto& p : poly.getPoints()) {
			os << "  " << p.x << "," << p.y << std::endl;
		}
		return os;
	}

	PolyMap::PolyMap(int npts, int relaxations, int width, int height) 
		: npts_(npts), relaxations_(relaxations), noise_seed_(0)
	{
		setEnvironment();
		setDim(width, height);
		init();
	}

	PolyMap::PolyMap(const variant& v, game_logic::FormulaCallable* e) 
		: Widget(v,e), npts_(v["points"].as_int(10)), relaxations_(v["relaxations"].as_int(2)),
		noise_seed_(v["noise_seed"].as_int(0)), noise_multiplier_(1.5f)
	{
		if(v.has_key("island_multiplier")) {
			noise_multiplier_ = float(v["island_multiplier"].as_decimal().as_float());
		}

		init();
	}

	void PolyMap::init()
	{
		// Generate an intial random series of points
		pts_.clear();
		for(int n = 0; n != npts_; ++n) {
			pts_.push_back(fpoint(std::rand() % (width()-4)+2, std::rand() % (height()-4)+2));
		}

		// Calculate voronoi polygons, running multiple Lloyd relaxation cycles.
		geometry::voronoi::Wrapper v(pts_, relaxations_, 0, 0, width(), height());

		hsv base_color = rgb_to_hsv(112, 144, 95);

		// Set heights via simplex noise
		noise::simplex::init(noise_seed_);
		for(auto& p : v.getPolys()) {
			std::vector<float> vec(2,0);
			vec[0] = static_cast<float>(p->getCentroid().x/width()*noise_multiplier_);
			vec[1] = static_cast<float>(p->getCentroid().y/height()*noise_multiplier_);
			p->setHeight(static_cast<int>(noise::simplex::noise2(&vec[0])*256.0f));
			
			if(p->height() < 0) {
				p->setColor(KRE::Color(52, 58, 94));
			} else {
				rgb col = hsv_to_rgb(base_color.h, base_color.s, static_cast<uint8_t>(base_color.v * p->height()/200.0f+128.0f));
				p->setColor(KRE::Color(col.r, col.g, col.b));
			}
		}

		for(auto& p : v.getPolys()) {
			auto& points = p->getPoints();
			for(int n = 1; n != points.size(); ++n) {
				edges_.emplace_back(static_cast<float>(points[n-1].x), static_cast<float>(points[n-1].y));
				edges_.emplace_back(static_cast<float>(points[n].x), static_cast<float>(points[n].y));
			}
		}
		pts_.assign(v.getSites().begin(), v.getSites().end());

		auto& polys = v.getPolys();
		polygons_.assign(polys.begin(), polys.end());

		std::for_each(polygons_.begin(), polygons_.end(), [](PolygonPtr p){
			p->init();
		});
	}

	PolyMap::~PolyMap()
	{
	}

	void PolyMap::handleDraw() const
	{
		auto canvas = KRE::Canvas::getInstance();
		canvas->drawHollowRect(rect(x(), y(), width(), height()), KRE::Color::colorWhite());

		for(auto& p : polygons_) {
			p->draw(x()&~1, y()&~1, getRotation(), getScale());
		}
	}

	BEGIN_DEFINE_CALLABLE(PolyMap, Widget)
		DEFINE_FIELD(dummy, "null")
			return variant();
	END_DEFINE_CALLABLE(PolyMap)

	void Polygon::draw(int xt, int yt, float rotate, float scale) const 
	{
		auto canvas = KRE::Canvas::getInstance();
		if(varray_.size() > 0) {
			canvas->drawPolygon(varray_, color_);
			if(draw_borders) {
				canvas->drawLines(vedges_, 1.0f, KRE::Color::colorBlack());
				canvas->drawSolidCircle(pointf(static_cast<float>(getCentroid().x),static_cast<float>(getCentroid().y)), 2.0f, KRE::Color::colorBlack());
			}
		}
	}

	void Polygon::init() 
	{
		if(pts_.size() > 0) {
			varray_.emplace_back(static_cast<float>(getCentroid().x), static_cast<float>(getCentroid().y)); 
			for(auto& p : pts_) {
				varray_.emplace_back(static_cast<float>(p.x), static_cast<float>(p.y));
			}
			// close the loop
			varray_.emplace_back(varray_[1]);

			for(int n = 1; n != pts_.size(); ++n) {
				vedges_.emplace_back(static_cast<float>(pts_[n-1].x), static_cast<float>(pts_[n-1].y));
				vedges_.emplace_back(static_cast<float>(pts_[n].x), static_cast<float>(pts_[n].y));
			}
		}
	}

}
