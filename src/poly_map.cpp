#include <iostream>
#include <memory>

#include "poly_map.hpp"
#include "raster.hpp"
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
		wrapper::wrapper(const fpoint_list& pts, int relaxations, double left, double top, double right, double bottom)
			: left_(left), right_(right), top_(top), bottom_(bottom)
		{
			if(left == 0 && right == 0 && top == 0 && bottom == 0) {
				left_ = std::numeric_limits<double>::max();
				top_ = std::numeric_limits<double>::max();
				right_ = std::numeric_limits<double>::min();
				bottom_ = std::numeric_limits<double>::min();
				calculate_bounding_box(pts);
			}

			ASSERT_LOG(relaxations > 0, "Number of relaxation cycles must be at least 1: " << relaxations);

			sites_.assign(pts.begin(), pts.end());
			for(int n = 0; n != relaxations; ++n) {
				generate(sites_);
			}
		}

		wrapper::~wrapper()
		{
		}

		void wrapper::generate(fpoint_list& pts)
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
			v.generateVoronoi(srcpts.get(), pts.size(), float(left_), float(right_), float(top_), float(bottom_));
			for(int n = 0; n != pts.size(); ++n) {
				int npoints = 0;
				PolygonPoint* pp = NULL;
				v.getSitePoints(n, &npoints, &pp);
				polygon* poly = new polygon(n);
				for(int m = 0; m != npoints; ++m) {
					poly->add_point(pp[m].coord.x, pp[m].coord.y);
				}
				poly->normalise();
				//std::cerr << "XXX: " << pts[n].x << "," << pts[n].y << " -- ";
				poly->calculate_centroid(pts[n]);
				poly->set_centroid(pts[n]);
				//std::cerr << pts[n].x << "," << pts[n].y << std::endl;
				//std::cerr << *poly;
				polygons_.push_back(polygon_ptr(poly));
			}
		}

		void wrapper::calculate_bounding_box(const fpoint_list& pts)
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

		std::ostream& operator<<(std::ostream& os, const wrapper& obj)
		{
			os << "Bounding box: " << obj.left() << "," << obj.top() << "," << obj.right() << "," << obj.bottom() << std::endl;
			auto& segs = obj.get_edges();
			for(auto& s : segs) {
				os << s.p1.x << "," << s.p1.y << " " << s.p2.x << "," << s.p2.y << std::endl;
			}
			return os;
		}
	}

	std::ostream& operator<<(std::ostream& os, const polygon& poly)
	{
		os << "POLYGON(" << poly.id() << "," << poly.points().size() << "," << poly.height() << ") :" << std::endl;
		for(auto& p : poly.points()) {
			os << "  " << p.x << "," << p.y << std::endl;
		}
		return os;
	}

	poly_map::poly_map(int npts, int relaxations, int width, int height) 
		: npts_(npts), relaxations_(relaxations), noise_seed_(0)
	{
		setEnvironment();
		setDim(width, height);
		init();
	}

	poly_map::poly_map(const variant& v, game_logic::FormulaCallable* e) 
		: widget(v,e), npts_(v["points"].as_int(10)), relaxations_(v["relaxations"].as_int(2)),
		noise_seed_(v["noise_seed"].as_int(0)), noise_multiplier_(1.5f)
	{
		if(v.has_key("island_multiplier")) {
			noise_multiplier_ = float(v["island_multiplier"].as_decimal().as_float());
		}

		init();
	}

	void poly_map::init()
	{
		// Generate an intial random series of points
		pts_.clear();
		for(int n = 0; n != npts_; ++n) {
			pts_.push_back(fpoint(std::rand() % (width()-4)+2, std::rand() % (height()-4)+2));
		}

		// Calculate voronoi polygons, running multiple Lloyd relaxation cycles.
		geometry::voronoi::wrapper v(pts_, relaxations_, 0, 0, width(), height());

		hsv base_color = rgb_to_hsv(112, 144, 95);

		// Set heights via simplex noise
		noise::simplex::init(noise_seed_);
		for(auto& p : v.get_polys()) {
			std::vector<float> vec(2,0);
			vec[0] = p->centroid().x/width()*noise_multiplier_;
			vec[1] = p->centroid().y/height()*noise_multiplier_;
			p->set_height(int(noise::simplex::noise2(&vec[0])*256.0f));
			
			if(p->height() < 0) {
				p->setColor(graphics::color(52, 58, 94));
			} else {
				rgb col = hsv_to_rgb(base_color.h, base_color.s, base_color.v * p->height()/200.0f+128.0f);
				p->setColor(graphics::color(col.r, col.g, col.b));
			}
		}

		for(auto& p : v.get_polys()) {
			//std::cerr << *p;
			auto& points = p->points();
			for(int n = 1; n != points.size(); ++n) {
				edges_.push_back(GLfloat(points[n-1].x));
				edges_.push_back(GLfloat(points[n-1].y));
				edges_.push_back(GLfloat(points[n].x));
				edges_.push_back(GLfloat(points[n].y));
			}
		}
		pts_.assign(v.sites().begin(), v.sites().end());

		auto& polys = v.get_polys();
		polygons_.assign(polys.begin(), polys.end());

		std::for_each(polygons_.begin(), polygons_.end(), [](polygon_ptr p){
			p->init();
		});
	}

	poly_map::~poly_map()
	{
	}

	void poly_map::handleDraw() const
	{
		gui::color_save_context ctx;
		graphics::draw_hollow_rect(rect(x(), y(), width(), height()), graphics::color(255,255,255,255));

		glPushMatrix();
		glTranslatef(x() & ~1, y() & ~1, 0.0f);

		for(auto& p : polygons_) {
			p->draw();
		}
		//polygons_.front()->draw();

		glPopMatrix();

		/*std::vector<GLfloat> varray;
		for(auto& p : pts_) {
			varray.push_back(p.x);
			varray.push_back(p.y);
		}
#if defined(USE_SHADERS)
		glPushMatrix();
		glTranslatef(x() & ~1, y() & ~1, 0.0f);

		glColor4f(1.0f, 0.0f, 0.0f, 1.0);
		gles2::manager gles2_manager(gles2::get_simple_shader());
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray.front());
		glDrawArrays(GL_POINTS, 0, varray.size()/2);

		//edges_
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &edges_.front());
		glDrawArrays(GL_LINES, 0, edges_.size()/2);

		glPopMatrix();
#endif
		*/
	}

	BEGIN_DEFINE_CALLABLE(poly_map, widget)
		DEFINE_FIELD(dummy, "null")
			return variant();
	END_DEFINE_CALLABLE(poly_map)

	void polygon::draw() const 
	{
		gui::color_save_context ctx;
#if defined(USE_SHADERS)
		if(varray_.size() > 0) {
			glColor4ub(color_.r(), color_.g(), color_.b(), color_.a());
			gles2::manager gles2_manager(gles2::get_simple_shader());
			gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray_.front());
			glDrawArrays(GL_TRIANGLE_FAN, 0, varray_.size()/2);

			if(draw_borders) {
				glUniform4f(gles2::active_shader()->shader()->get_fixed_uniform("color"), 0, 0, 0, 255);
				gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &vedges_.front());
				glDrawArrays(GL_LINES, 0, vedges_.size()/2);

				std::vector<GLfloat> pt;
				pt.push_back(centroid().x);
				pt.push_back(centroid().y);
				glPointSize(2.0f);
				gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &pt.front());
				glDrawArrays(GL_POINTS, 0, pt.size()/2);
			}
		}
#endif
	}
}
