/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <set>
#include <cstdint>

#include "svg_shapes.hpp"
#include "svg_element.hpp"

#include "unit_test.hpp"

namespace KRE
{
	namespace SVG
	{
		using namespace boost::property_tree;

		namespace
		{
			std::vector<svg_length> parse_list_of_lengths(const std::string& s)
			{
				std::vector<svg_length> res;
				boost::char_separator<char> seperators(" \n\t\r,");
				boost::tokenizer<boost::char_separator<char>> tok(s, seperators);
				for(auto it = tok.begin(); it != tok.end(); ++it) {
					res.emplace_back(*it);
				}
				return res;
			}

			point_list create_point_list(const std::string& s)
			{
				auto res = parse_list_of_lengths(s);
				ASSERT_LOG(res.size() % 2 == 0, "point list has an odd number of points.");
				auto it = res.begin();
				point_list points;
				while(it != res.end()) {
					svg_length p1 = *it;
					++it;
					svg_length p2 = *it;
					++it;
					points.emplace_back(p1, p2);
				}
				return points;
			}

			std::vector<double> parse_list_of_numbers(const std::string& s)
			{
				std::vector<double> res;
				boost::char_separator<char> seperators(" \n\t\r,");
				boost::tokenizer<boost::char_separator<char>> tok(s, seperators);
				for(auto it = tok.begin(); it != tok.end(); ++it) {
					try {
						res.push_back(boost::lexical_cast<double>(*it));
					} catch(boost::bad_lexical_cast&) {
						ASSERT_LOG(false, "Unable to convert value '" << *it << "' to a number");
					}
				}
				return res;
			}

			void elliptic_arc(cairo_t* ctx, double x2, double y2, double rx, double ry, double x_axis_rotation=0, bool large_arc_flag=false, bool sweep_flag=true)
			{
				double x1, y1;
				cairo_get_current_point(ctx, &x1, &y1);

				// calculate some ellipse stuff
				// a is the length of the major axis
				// b is the length of the minor axis
				double a = rx;
				double b = ry;

				// start and end points in the same location is equivalent to not drawing the arc.
				if(std::abs(x1-x2) < DBL_EPSILON && std::abs(y1-y2) < DBL_EPSILON) {
					return;
				}
			
				const double r1 = (x1-x2)/2.0;
				const double r2 = (y1-y2)/2.0;

				const double cosp = cos(x_axis_rotation);
				const double sinp = sin(x_axis_rotation);

				const double x1_prime = cosp*r1 + sinp*r2;
				const double y1_prime = -sinp * r1 + cosp*r2;

				double gamma = (x1_prime*x1_prime)/(a*a) + (y1_prime*y1_prime)/(b*b);
				if (gamma > 1) {
					a *= sqrt(gamma);
					b *= sqrt(gamma);
				}

				const double denom1 = a*a*y1_prime*y1_prime+b*b*x1_prime*x1_prime;
				if(std::abs(denom1) < DBL_EPSILON) {
					return;
				}
				const double root = std::sqrt(std::abs(a*a*b*b/denom1-1));
				double xc_prime = root * a * y1_prime / b;
				double yc_prime = -root * b * x1_prime / a;

				if((large_arc_flag && sweep_flag) || (!large_arc_flag && !sweep_flag)) {
					xc_prime = -1 * xc_prime;
					yc_prime = -1 * yc_prime;
				}

				const double xc = cosp * xc_prime - sinp * yc_prime + (x1+x2)/2.0;
				const double yc = sinp * xc_prime + cosp * yc_prime + (y1+y2)/2.0;

				const double k1 = (x1_prime - xc_prime)/a;
				const double k2 = (y1_prime - yc_prime)/b;
				const double k3 = (-x1_prime - xc_prime)/a;
				const double k4 = (-y1_prime - yc_prime)/b;

				const double k5 = sqrt(fabs(k1*k1 + k2*k2));
				if(std::abs(k5) < DBL_EPSILON) { 
					return;
				}

				const double t1 = (k2 < 0 ? -1 : 1) * std::acos(clamp(k1/k5, -1.0, 1.0));	// theta_1

				const double k7 = std::sqrt(fabs((k1*k1 + k2*k2)*(k3*k3 + k4*k4)));
				if(std::abs(k7) < DBL_EPSILON) {
					return;
				}

				const double theta_delta = (k1*k4 - k3*k2 < 0 ? -1 : 1) * acos(clamp((k1*k3 + k2*k4)/k7, -1.0, 1.0));
				const double t2 = theta_delta > 0 && !sweep_flag ? theta_delta-2.0*M_PI : theta_delta < 0 && sweep_flag ? theta_delta+2.0*M_PI : theta_delta;

				const int n_segs = int(std::ceil(std::abs(t2/(M_PI*0.5+0.001))));
				for(int i = 0; i < n_segs; i++) {
					const double th0 = t1 + i * t2 / n_segs;
					const double th1 = t1 + (i + 1) * t2 / n_segs;
					const double th_half = 0.5 * (th1 - th0);
					const double t = (8.0 / 3.0) * std::sin(th_half * 0.5) * std::sin(th_half * 0.5) / std::sin(th_half);
					const double x1 = a*(std::cos(th0) - t * std::sin(th0));
					const double y1 = b*(std::sin(th0) + t * std::cos(th0));
					const double x3 = a*std::cos(th1);
					const double y3 = b*std::sin(th1);
					const double x2 = x3 + a*(t * std::sin(th1));
					const double y2 = y3 + b*(-t * std::cos(th1));
					cairo_curve_to(ctx, 
						xc + cosp*x1 - sinp*y1, 
						yc + sinp*x1 + cosp*y1, 
						xc + cosp*x2 - sinp*y2, 
						yc + sinp*x2 + cosp*y2, 
						xc + cosp*x3 - sinp*y3, 
						yc + sinp*x3 + cosp*y3);
				}
			}
		}

		shape::shape(element* doc, const ptree& pt)
				: container(doc, pt)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto dpath = attributes->get_child_optional("d");
				if(dpath && !dpath->data().empty()) {
					path_ = parse_path(dpath->data());
				}
			}
		}

		shape::~shape() 
		{
		}

		void shape::handle_render(render_context& ctx) const 
		{
			render_path(ctx);
		}

		void shape::handle_clip_render(render_context& ctx) const
		{
			clip_render_path(ctx);
		}

		void shape::stroke_and_fill(render_context& ctx) const
		{
			auto fc = ctx.fill_color_top();
			if(fc && fc->apply(parent(), ctx)) {
				cairo_fill_preserve(ctx.cairo());
			}
			auto sc = ctx.stroke_color_top();
			if(sc && sc->apply(parent(), ctx)) {
				cairo_stroke(ctx.cairo());
			}
			// Clear the current path, regardless
			cairo_new_path(ctx.cairo());
		}

		void shape::render_path(render_context& ctx) const 
		{
			if(!path_.empty()) {
				path_cmd_context path_ctx(ctx.cairo());
				for(auto p : path_) {
					p->cairo_render(path_ctx);
				}
				stroke_and_fill(ctx);
			}
		}

		void shape::clip_render_path(render_context& ctx) const
		{
			if(!path_.empty()) {
				path_cmd_context path_ctx(ctx.cairo());
				for(auto p : path_) {
					p->cairo_render(path_ctx);
				}
				cairo_clip(ctx.cairo());
			}
		}

		// list_of here is a hack because MSVC doesn't support C++11 initialiser_lists
		circle::circle(element* doc, const ptree& pt) 
			: shape(doc, pt) 
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto cx = attributes->get_child_optional("cx");
				if(cx) {
					cx_ = svg_length(cx->data());
				}
				auto cy = attributes->get_child_optional("cy");
				if(cy) {
					cy_ = svg_length(cy->data());
				}
				auto r = attributes->get_child_optional("r");
				if(r) {
					radius_ = svg_length(r->data());
				}
			}
			if(0) {
				LOG_DEBUG("SVG: CIRCLE(" << cx_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER)
					<< "," << cy_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER)
					<< "," << radius_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER)
					<< ")");
			}
		}

		circle::~circle() 
		{
		}

		void circle::render_circle(render_context& ctx) const
		{
			double cx = cx_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double cy = cy_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double r  = radius_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			cairo_arc(ctx.cairo(), cx, cy, r, 0.0, 2 * M_PI);
		}
		
		void circle::handle_render(render_context& ctx) const 
		{
			render_circle(ctx);
			stroke_and_fill(ctx);
			shape::render_path(ctx);
		}

		void circle::handle_clip_render(render_context& ctx) const
		{
			render_circle(ctx);
			cairo_clip(ctx.cairo());
			shape::clip_render_path(ctx);
		}


		ellipse::ellipse(element* doc, const ptree& pt)
			: shape(doc, pt),
			cx_(0, svg_length::SVG_LENGTHTYPE_NUMBER),
			cy_(0, svg_length::SVG_LENGTHTYPE_NUMBER),
			rx_(0, svg_length::SVG_LENGTHTYPE_NUMBER),
			ry_(0, svg_length::SVG_LENGTHTYPE_NUMBER)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto cx = attributes->get_child_optional("cx");
				if(cx) {
					cx_ = svg_length(cx->data());
				}
				auto cy = attributes->get_child_optional("cy");
				if(cy) {
					cy_ = svg_length(cy->data());
				}
				auto rx = attributes->get_child_optional("rx");
				if(rx) {
					rx_ = svg_length(rx->data());
				}
				auto ry = attributes->get_child_optional("ry");
				if(ry) {
					ry_ = svg_length(ry->data());
				}
			}
		}

		ellipse::~ellipse()
		{
		}

		void ellipse::handle_render(render_context& ctx) const 
		{
			double cx = cx_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double cy = cy_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double rx = rx_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double ry = ry_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);

			cairo_save(ctx.cairo());
			cairo_translate(ctx.cairo(), cx+rx, cy+ry);
			cairo_scale(ctx.cairo(), rx, ry);
			cairo_arc_negative(ctx.cairo(), 0.0, 0.0, 1.0, 0.0, 2*M_PI);
			stroke_and_fill(ctx);
			cairo_restore(ctx.cairo());

			shape::render_path(ctx);
		}

		void ellipse::handle_clip_render(render_context& ctx) const
		{
			double cx = cx_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double cy = cy_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double rx = rx_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double ry = ry_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);

			cairo_save(ctx.cairo());
			cairo_translate(ctx.cairo(), cx+rx, cy+ry);
			cairo_scale(ctx.cairo(), rx, ry);
			cairo_arc_negative(ctx.cairo(), 0.0, 0.0, 1.0, 0.0, 2*M_PI);
			cairo_clip(ctx.cairo());	// XXX this may not be correct, since cairo_restore will kill the clip-path
			cairo_restore(ctx.cairo());

			shape::clip_render_path(ctx);
		}

		rectangle::rectangle(element* doc, const ptree& pt) 
			: shape(doc, pt), 
			is_rounded_(false) 
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto x = attributes->get_child_optional("x");
				if(x) {
					x_ = svg_length(x->data());
				}
				auto y = attributes->get_child_optional("y");
				if(y) {
					y_ = svg_length(y->data());
				}
				auto w = attributes->get_child_optional("width");
				if(w) {
					width_ = svg_length(w->data());
				}
				auto h = attributes->get_child_optional("height");
				if(h) {
					height_ = svg_length(h->data());
				}
				auto rx = attributes->get_child_optional("rx");
				if(rx) {
					rx_ = svg_length(rx->data());
				}
				auto ry = attributes->get_child_optional("ry");
				if(ry) {
					ry_ = svg_length(ry->data());
				}
				if(rx || ry) {
					is_rounded_ = true;
				}
			}
		}

		rectangle::~rectangle() 
		{
		}

		void rectangle::render_rectangle(render_context& ctx) const
		{
			//ASSERT_LOG(is_rounded_ == false, "XXX we don't support rounded rectangles -- yet");
			double x = x_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double y = y_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double rx = rx_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double ry = ry_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double w  = width_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double h  = height_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);

			if(is_rounded_) {
				if(rx > w/2.0) {
					rx = w/2.0;
				}
				if(ry > h/2.0) {
					ry = h/2.0;
				}
				if(rx < 0) {
					rx = 0;
				}
				if(ry < 0) {
					ry = 0;
				}
				
				cairo_new_path(ctx.cairo());
				cairo_move_to(ctx.cairo(), x + rx, y);
				cairo_line_to(ctx.cairo(), x + w - rx, y);
				elliptic_arc(ctx.cairo(), x + w, y + ry, rx, ry);
				cairo_line_to(ctx.cairo(), x + w, y + h - ry);
				elliptic_arc(ctx.cairo(), x + w - rx, y + h, rx, ry);
				cairo_line_to(ctx.cairo(), x + rx, y + h);
				elliptic_arc(ctx.cairo(), x, y + h - ry, rx, ry);
				cairo_line_to(ctx.cairo(), x, y + ry);
				elliptic_arc(ctx.cairo(), x + rx, y, rx, ry);
				cairo_close_path(ctx.cairo());
			} else {
				cairo_rectangle(ctx.cairo(), x, y, w, h);
			}
		}

		void rectangle::handle_render(render_context& ctx) const 
		{
			render_rectangle(ctx);
			stroke_and_fill(ctx);
			shape::render_path(ctx);
		}

		void rectangle::handle_clip_render(render_context& ctx) const
		{
			render_rectangle(ctx);
			cairo_clip(ctx.cairo());
			shape::clip_render_path(ctx);
		}

		polygon::polygon(element* doc, const ptree& pt) 
			: shape(doc, pt)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto points = attributes->get_child_optional("points");
				if(points) {
					points_ = create_point_list(points->data());
				}
			}
		}

		polygon::~polygon() 
		{
		}

		void polygon::render_polygon(render_context& ctx) const
		{
			auto it = points_.begin();
			cairo_move_to(ctx.cairo(), 
				it->first.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER), 
				it->second.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER));
			while(it != points_.end()) {
				cairo_line_to(ctx.cairo(), 
					it->first.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER), 
					it->second.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER));
				++it;
			}
			cairo_close_path(ctx.cairo());
		}

		void polygon::handle_render(render_context& ctx) const 
		{
			render_polygon(ctx);
			stroke_and_fill(ctx);
			shape::render_path(ctx);
		}

		void polygon::handle_clip_render(render_context& ctx) const
		{
			render_polygon(ctx);
			cairo_clip(ctx.cairo());
			shape::clip_render_path(ctx);
		}

		text::text(element* doc, const ptree& pt, bool is_tspan) 
			: shape(doc, pt),
			 adjust_(LengthAdjust::SPACING),
			 is_tspan_(is_tspan)
		{
			// XXX should we use provided <xmltext> instead?
			text_ = pt.get_value<std::string>();

			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto x = attributes->get_child_optional("x");
				if(x) {
					x1_ = parse_list_of_lengths(x->data());
				}
				auto y = attributes->get_child_optional("y");
				if(y) {
					y1_ = parse_list_of_lengths(y->data());
				}
				auto dx = attributes->get_child_optional("dx");
				if(dx) {
					dx_ = parse_list_of_lengths(dx->data());
				}
				auto dy = attributes->get_child_optional("dy");
				if(dy) {
					dy_ = parse_list_of_lengths(dy->data());
				}
				auto rotate = attributes->get_child_optional("rotate");
				if(rotate) {
					rotate_ = parse_list_of_numbers(rotate->data());
				}
				auto text_length = attributes->get_child_optional("textLength");
				if(text_length) {
					text_length_ = svg_length(text_length->data());
				}
				auto length_adjust = attributes->get_child_optional("lengthAdjust");
				if(length_adjust) {
					if(length_adjust->data() == "spacing") {
						adjust_ = LengthAdjust::SPACING;
					} else if(length_adjust->data() == "spacingAndGlyphs") {
						adjust_ = LengthAdjust::SPACING_AND_GLYPHS;
					} else {
						ASSERT_LOG(false, "Unrecognised spacing value: " << length_adjust->data());
					}
				}
			}
		}

		text::~text() 
		{
		}

		void text::render_text(render_context& ctx) const
		{
			attribute_manager ta1(ta(), ctx);
			attribute_manager fa1(fa(), ctx);

			// XXX if x/y/dx/dy lists of data are provided we should use it here.
			// XXX apply list of rotations as well.

			std::vector<cairo_glyph_t> glyphs;
			auto face = ctx.fa().top_font_face();
			std::vector<unsigned> glyph_indicies = face->getGlyphs(text_);
			double x = x1_.size() > 0 ? x1_[0].value_in_specified_units(svg_length::LengthUnit::SVG_LENGTHTYPE_NUMBER) : is_tspan_ ? ctx.get_text_x() : 0;
			double y = y1_.size() > 0 ? y1_[0].value_in_specified_units(svg_length::LengthUnit::SVG_LENGTHTYPE_NUMBER) : is_tspan_ ? ctx.get_text_y() : 0;
			const double letter_spacing = ctx.letter_spacing_top();
			for(auto g : glyph_indicies) {
				cairo_glyph_t cg;
				cg.index = g;
				cg.x = x;
				cg.y = y;
				glyphs.push_back(cg);

				cairo_text_extents_t extent;
				cairo_glyph_extents(ctx.cairo(), &cg, 1, &extent);
				x += extent.x_advance;
				if(letter_spacing > 0) {
					x += letter_spacing;
				} 
				y += extent.y_advance;
			}
			cairo_glyph_path(ctx.cairo(), &glyphs[0], static_cast<int>(glyphs.size()));
			stroke_and_fill(ctx);
			ctx.set_text_xy(x, y);
		}

		void text::handle_render(render_context& ctx) const 
		{
			if(!text_.empty()) {
				render_text(ctx);
			}
			render_children(ctx);
			shape::render_path(ctx);
		}

		void text::handle_clip_render(render_context& ctx) const
		{
			if(!text_.empty()) {
				render_text(ctx);
			}
			cairo_clip(ctx.cairo());
			clip_render_children(ctx);
			shape::clip_render_path(ctx);
		}

		line::line(element* doc, const ptree& pt)
			: shape(doc, pt),
			x1_(0, svg_length::SVG_LENGTHTYPE_NUMBER),
			y1_(0, svg_length::SVG_LENGTHTYPE_NUMBER),
			x2_(0, svg_length::SVG_LENGTHTYPE_NUMBER),
			y2_(0, svg_length::SVG_LENGTHTYPE_NUMBER)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto x1 = attributes->get_child_optional("x1");
				if(x1) {
					x1_ = svg_length(x1->data());
				}
				auto y1 = attributes->get_child_optional("y1");
				if(y1) {
					y1_ = svg_length(y1->data());
				}
				auto x2 = attributes->get_child_optional("x2");
				if(x2) {
					x2_ = svg_length(x2->data());
				}
				auto y2 = attributes->get_child_optional("y2");
				if(y2) {
					y2_ = svg_length(y2->data());
				}
			}
		}

		line::~line()
		{
		}

		void line::render_line(render_context& ctx) const
		{
			double x1 = x1_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double y1 = y1_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double x2 = x2_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double y2 = y2_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			
			cairo_move_to(ctx.cairo(), x1, y1);
			cairo_line_to(ctx.cairo(), x2, y2);
		}

		void line::handle_render(render_context& ctx) const
		{
			render_line(ctx);
			auto sc = ctx.stroke_color_top();
			if(sc && sc->apply(parent(), ctx)) {
				cairo_stroke(ctx.cairo());
			}
			shape::render_path(ctx);
		}

		void line::handle_clip_render(render_context& ctx) const
		{
			// XXX
			render_line(ctx);
			cairo_clip(ctx.cairo());
			shape::clip_render_path(ctx);
		}

		polyline::polyline(element* doc, const ptree& pt)
			: shape(doc,pt)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto points = attributes->get_child_optional("points");
				if(points) {
					points_ = create_point_list(points->data());
				}
			}
		}

		polyline::~polyline()
		{
		}

		void polyline::render_polyline(render_context& ctx) const
		{
			bool is_first = true;
			for(auto& p : points_) {
				double x = p.first.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
				double y = p.second.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
				if(is_first) {
					is_first = false;
					cairo_move_to(ctx.cairo(), x, y);
				} else {
					cairo_line_to(ctx.cairo(), x, y);
				}
			}
		}

		void polyline::handle_render(render_context& ctx) const
		{
			render_polyline(ctx);
			stroke_and_fill(ctx);
			shape::render_path(ctx);
		}

		void polyline::handle_clip_render(render_context& ctx) const
		{
			render_polyline(ctx);
			cairo_clip(ctx.cairo());
			shape::clip_render_path(ctx);
		}
	}
}

UNIT_TEST(parse_list_of_lengths_test_0) {
	const std::string list_of_lengths_input =
			"4 8 15 16 23 42";
	const std::vector<KRE::SVG::svg_length> list_of_lengths_output =
			KRE::SVG::parse_list_of_lengths(list_of_lengths_input);
	std::vector<KRE::SVG::svg_length> expected_output;
	expected_output.emplace_back(KRE::SVG::svg_length("4"));
	expected_output.emplace_back(KRE::SVG::svg_length("8"));
	expected_output.emplace_back(KRE::SVG::svg_length("15"));
	expected_output.emplace_back(KRE::SVG::svg_length("16"));
	expected_output.emplace_back(KRE::SVG::svg_length("23"));
	expected_output.emplace_back(KRE::SVG::svg_length("42"));
	const uint_fast8_t expected_output_size = expected_output.size();
	CHECK_EQ(expected_output_size, list_of_lengths_output.size());
	for (int i = 0; i < expected_output_size; i++) {
		const KRE::SVG::svg_length expected_length =
				expected_output[i];
		const KRE::SVG::svg_length actual_length =
				list_of_lengths_output[i];
		const float expected_length_number =
				expected_length.value_in_specified_units(
						KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_NUMBER);
		LOG_DEBUG(expected_length_number);
		const float actual_length_number =
				actual_length.value_in_specified_units(
						KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_NUMBER);
		LOG_DEBUG(actual_length_number);
		CHECK_EQ(expected_length_number, actual_length_number);
	}
}

/*   Might fail on Windows 10 x64. Should need reviewing before any
 * possible reenable. */
/*
UNIT_TEST(parse_list_of_lengths_test_1) {
	const std::string list_of_lengths_input =
			"lorem ipsum dolor sit amet 23 42 consectetur adipiscing elit";
	const std::vector<KRE::SVG::svg_length> list_of_lengths_output =
			KRE::SVG::parse_list_of_lengths(list_of_lengths_input);
	std::vector<KRE::SVG::svg_length> expected_output;
	expected_output.emplace_back(KRE::SVG::svg_length("0"));
	expected_output.emplace_back(KRE::SVG::svg_length("0"));
	expected_output.emplace_back(KRE::SVG::svg_length("0"));
	expected_output.emplace_back(KRE::SVG::svg_length("0"));
	expected_output.emplace_back(KRE::SVG::svg_length("0"));
	expected_output.emplace_back(KRE::SVG::svg_length("23"));
	expected_output.emplace_back(KRE::SVG::svg_length("42"));
	expected_output.emplace_back(KRE::SVG::svg_length("0"));
	expected_output.emplace_back(KRE::SVG::svg_length("0"));
	expected_output.emplace_back(KRE::SVG::svg_length("0"));
	const uint_fast8_t expected_output_size = expected_output.size();
	CHECK_EQ(expected_output_size, list_of_lengths_output.size());
	for (int i = 0; i < expected_output_size; i++) {
		const KRE::SVG::svg_length expected_length =
				expected_output[i];
		const KRE::SVG::svg_length actual_length =
				list_of_lengths_output[i];
		const float expected_length_number =
				expected_length.value_in_specified_units(
						KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_NUMBER);
		LOG_DEBUG(expected_length_number);
		const float actual_length_number =
				actual_length.value_in_specified_units(
						KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_NUMBER);
		LOG_DEBUG(actual_length_number);
		CHECK_EQ(expected_length_number, actual_length_number);
	}
}
*/

UNIT_TEST(create_point_list) {
	const std::string input = "0,0 1,1";
	std::vector<std::pair<
		KRE::SVG::svg_length, KRE::SVG::svg_length
	>> expected_output;
	{
		using namespace KRE::SVG;
		expected_output.emplace_back(
				std::pair<svg_length, svg_length>(
					svg_length("0"), svg_length("0")));
		expected_output.emplace_back(
				std::pair<svg_length, svg_length>(
					svg_length("1"), svg_length("1")));
	}
	const KRE::SVG::point_list actual_output =
			KRE::SVG::create_point_list(input);
	const uint_fast8_t actual_output_size = actual_output.size();
	CHECK_EQ(expected_output.size(), actual_output_size);
	for (int i = 0; i < actual_output_size; i++) {
		const std::pair<
			KRE::SVG::svg_length, KRE::SVG::svg_length
		> expected_point = expected_output[i];
		const float expected_point_x =
				expected_point.first.value_in_specified_units(
						KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_NUMBER);
		LOG_DEBUG(expected_point_x);
		const float expected_point_y =
				expected_point.second.value_in_specified_units(
						KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_NUMBER);
		LOG_DEBUG(expected_point_y);
		const std::pair<
			KRE::SVG::svg_length, KRE::SVG::svg_length
		> actual_point = actual_output[i];
		const float actual_point_x =
				actual_point.first.value_in_specified_units(
						KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_NUMBER);
		LOG_DEBUG(actual_point_x);
		const float actual_point_y =
				actual_point.second.value_in_specified_units(
						KRE::SVG::svg_length::LengthUnit::SVG_LENGTHTYPE_NUMBER);
		LOG_DEBUG(actual_point_y);
		CHECK_EQ(expected_point_x, actual_point_x);
		CHECK_EQ(expected_point_y, actual_point_y);
	}
}

UNIT_TEST(parse_list_of_numbers_good_input) {
	const std::string input = "0 1";
	std::vector<double> expected_output_vector;
	expected_output_vector.emplace_back(0);
	expected_output_vector.emplace_back(1);
	const std::vector<double> actual_output_vector =
			KRE::SVG::parse_list_of_numbers(input);
	const uint_fast8_t actual_output_vector_size = actual_output_vector.size();
	CHECK_EQ(expected_output_vector.size(), actual_output_vector_size);
	for (int i = 0; i < actual_output_vector_size; i++) {
		double expected = expected_output_vector[i];
		double actual = actual_output_vector[i];
		LOG_DEBUG(expected);
		LOG_DEBUG(actual);
		CHECK_EQ(expected, actual);
	}
}

UNIT_TEST(parse_list_of_numbers_bad_input) {
	const std::string input = "Milgram";
	std::vector<double> expected_output_vector;
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			KRE::SVG::parse_list_of_numbers(input);
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	ASSERT_LOG(excepted, "expected an exception that did not happen");
}
