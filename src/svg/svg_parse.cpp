/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <sstream>

#include "asserts.hpp"
#include "geometry.hpp"

#include "svg_element.hpp"
#include "svg_paint.hpp"
#include "svg_parse.hpp"
#include "svg_path_parse.hpp"
#include "svg_shapes.hpp"

#ifndef M_PI
#	define M_PI		3.1415926535897932384626433832795
#endif

namespace KRE
{
	namespace SVG
	{
		using namespace boost::property_tree;

		namespace 
		{
			void display_ptree(ptree const& pt)
			{
				for(auto& v : pt) {
					LOG_DEBUG(v.first << ": " << v.second.get_value<std::string>());
					display_ptree( v.second );
				}
			}

			void print_matrix(const cairo_matrix_t& mat)
			{
				LOG_DEBUG("MAT(" << mat.xx << " " << mat.yx << " " << mat.xy << " " << mat.yy << " " << mat.x0 << " " << mat.y0 << ")");
			}
		}


		parse::parse(const std::string& filename)
		{
			ptree pt;
			read_xml(filename, pt);

			svg_data_.emplace_back(element::factory(nullptr, pt));
			// Resolve all the references.
			for(auto p : svg_data_) {
				p->resolve();
			}
		}

		parse::~parse()
		{
		}

		void parse::render(render_context& ctx) const
		{
			cairo_set_source_rgb(ctx.cairo(), 0.0, 0.0, 0.0);
			cairo_set_line_cap(ctx.cairo(), CAIRO_LINE_CAP_BUTT);
			cairo_set_line_join(ctx.cairo(), CAIRO_LINE_JOIN_MITER);
			cairo_set_miter_limit(ctx.cairo(), 4.0);
			cairo_set_fill_rule(ctx.cairo(), CAIRO_FILL_RULE_WINDING);
			cairo_set_line_width(ctx.cairo(), 1.0);
			ctx.fill_color_push(paint_ptr(new paint(0,0,0,255)));
			ctx.stroke_color_push(paint_ptr(new paint()));
			ctx.opacity_push(1.0);
			ctx.letter_spacing_push(0);
			ctx.fa().push_font_size(12);

			for(auto p : svg_data_) {
				p->render(ctx);
			}

			ctx.letter_spacing_pop();
			ctx.opacity_pop();
			ctx.stroke_color_pop();
			ctx.fill_color_pop();
		}
	}
}
