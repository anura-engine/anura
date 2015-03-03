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
				std::cerr << "SVG: CIRCLE(" << cx_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER)
					<< "," << cy_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER)
					<< "," << radius_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER)
					<< ")" << std::endl;
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
			ASSERT_LOG(is_rounded_ == false, "XXX we don't support rounded rectangles -- yet");
			double x = x_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double y = y_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double rx = rx_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double ry = ry_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double w  = width_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double h  = height_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);

			cairo_rectangle(ctx.cairo(), x, y, w, h);
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

		text::text(element* doc, const ptree& pt) 
			: shape(doc, pt),
			adjust_(LengthAdjust::SPACING)
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
			FT_Face face = ctx.fa().top_font_face();
			auto glyph_indicies = FT::get_glyphs_from_string(face, text_);
			double x = x1_.size() > 0 ? x1_[0].value_in_specified_units(svg_length::LengthUnit::SVG_LENGTHTYPE_NUMBER) : ctx.get_text_x();
			double y = y1_.size() > 0 ? y1_[0].value_in_specified_units(svg_length::LengthUnit::SVG_LENGTHTYPE_NUMBER) : ctx.get_text_y();
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
			cairo_glyph_path(ctx.cairo(), &glyphs[0], glyphs.size());
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
