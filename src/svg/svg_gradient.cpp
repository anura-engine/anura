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

#include "boost/lexical_cast.hpp"
#include "svg_gradient.hpp"

namespace KRE
{
	namespace SVG
	{
		using namespace boost::property_tree;

		gradient_stop::gradient_stop(element* doc, const ptree& pt)
			: core_attribs(pt), 
			offset_(0.0),
			opacity_(1.0),
			opacity_set_(false)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto opacity = attributes->get_child_optional("stop-opacity");
				auto color = attributes->get_child_optional("stop-color");
				auto offset = attributes->get_child_optional("offset");

				if(opacity) {
					opacity_set_ = true;
					const std::string alpha = opacity->data();
					try {
						opacity_ = boost::lexical_cast<double>(alpha);
					} catch(const boost::bad_lexical_cast&) {
						ASSERT_LOG(false, "Couldn't convert opacity value to number: " << alpha);
					}
				}

				if(color) {
					color_ = paint::from_string(color->data());
					if(opacity) {
						color_->set_opacity(opacity_);
					}
				}

				ASSERT_LOG(!offset, "No offset field given in gradient color stop");
				const std::string offs = offset->data();
				try {
					offset_ = boost::lexical_cast<double>(offs);
				} catch(const boost::bad_lexical_cast&) {
					ASSERT_LOG(false, "Couldn't convert opacity value to number: " << offs);
				}
				if(offs.find('%') != std::string::npos) {
					offset_ /= 100.0;
				}
				offset_ = std::max(std::min(offset_, 1.0), 0.0);
			}
		}

		gradient_stop::~gradient_stop()
		{
		}

		void gradient_stop::apply(render_context& ctx, cairo_pattern_t* pattern)
		{
			/*double r, g, b;

			// should this be fill or stroke? or specified by a currentColor.
			double a = opacity_set_ ? opacity_ : ctx.fill_color_top().a();

			if(color_) {
				r = color_->r();
				g = color_->g();
				b = color_->b();
			} else {
				r = ctx.fill_color_top().r();
				g = ctx.fill_color_top().g();
				b = ctx.fill_color_top().b();
			}

			cairo_pattern_add_color_stop_rgba(pattern, offset_, r, g, b, a);
			*/
			ASSERT_LOG(false, "XXX: fixme gradient_stop::apply");
		}

		gradient::gradient(element* doc, const ptree& pt)
			: core_attribs(pt),
			coord_system_(GradientCoordSystem::OBJECT_BOUNDING_BOX),
			spread_(GradientSpreadMethod::PAD)
		{
			// Process attributes
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto xlink_href = attributes->get_child_optional("xlink:xref");
				auto transforms = attributes->get_child_optional("gradientTransforms");
				auto units = attributes->get_child_optional("gradientUnits");
				auto spread = attributes->get_child_optional("spreadMethod");

				if(transforms) {
					transforms_ = transform::factory(transforms->data());
				}
				if(xlink_href) {
					xlink_href_ = xlink_href->data();
				}
				if(units) {
					std::string csystem = units->data();
					if(csystem == "userSpaceOnUse") {
						coord_system_ = GradientCoordSystem::USERSPACE_ON_USE;
					} else if(csystem =="objectBoundingBox") {
						coord_system_ = GradientCoordSystem::OBJECT_BOUNDING_BOX;
					} else {
						ASSERT_LOG(false, "Unrecognised 'gradientUnits' value: " << csystem);
					}
				}
				if(spread) {
					std::string spread_val = units->data();
					if(spread_val == "pad") {
						spread_ = GradientSpreadMethod::PAD;
					} else if(spread_val =="reflect") {
						spread_ = GradientSpreadMethod::REFLECT;
					} else if(spread_val =="repeat") {
						spread_ = GradientSpreadMethod::REPEAT;
					} else {
						ASSERT_LOG(false, "Unrecognised 'spreadMethod' value: " << spread_val);
					}
				}

				// Process child elements
				for(auto& v : pt) {
					if(v.first == "stop") {
						stops_.emplace_back(new gradient_stop(doc, v.second));
					} else if(v.first == "<xmlattr>") {
						// ignore
					} else if(v.first == "<xmlcomment>") {
						// ignore
					} else {
						ASSERT_LOG(false, "unexpected child element in gradient stop list: " << v.first);
					}
				}
			}
		}

		gradient::~gradient()
		{
		}

		void gradient::set_source(render_context& ctx) const
		{
			handle_set_source(ctx);
		}

		void gradient::apply_stops(render_context& ctx, cairo_pattern_t* pattern) const
		{
			for(auto& stp : stops_) {
				stp->apply(ctx, pattern);
			}
		}

		void gradient::apply_transforms(cairo_pattern_t* pattern) const
		{
			cairo_matrix_t mtx;
			cairo_matrix_init_identity(&mtx);
			for(auto& tr : transforms_) {
				tr->apply_matrix(&mtx);
			}
			cairo_pattern_set_matrix(pattern, &mtx);
		}

		linear_gradient::linear_gradient(element* doc, const ptree& pt)
			: gradient(doc, pt)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto x1 = attributes->get_child_optional("x1");
				auto y1 = attributes->get_child_optional("y1");
				auto x2 = attributes->get_child_optional("x2");
				auto y2 = attributes->get_child_optional("y2");
				if(x1) {
					x1_.from_string(x1->data());
				}
				if(y1) {
					y1_.from_string(y1->data());
				}
				if(x2) {
					x2_.from_string(x2->data());
				}
				if(y2) {
					y2_.from_string(y2->data());
				}
			}
		}

		linear_gradient::~linear_gradient()
		{
		}

		void linear_gradient::handle_set_source(render_context& ctx) const
		{
			cairo_pattern_t* pattern = cairo_pattern_create_linear(
				x1_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER),
				y1_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER),
				x2_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER),
				y2_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER));
			apply_stops(ctx, pattern);
			auto status = cairo_pattern_status(pattern);
			ASSERT_LOG(status == CAIRO_STATUS_SUCCESS, "Linear Gradient pattern couldn't be created: " << cairo_status_to_string(status));

			pattern_.reset(pattern, [](cairo_pattern_t* p) { cairo_pattern_destroy(p); });
			apply_transforms(pattern);
			cairo_set_source(ctx.cairo(), pattern);
		}

		radial_gradient::radial_gradient(element* doc, const ptree& pt)
			: gradient(doc, pt)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto cx = attributes->get_child_optional("cx");
				auto cy = attributes->get_child_optional("cy");
				auto radius = attributes->get_child_optional("r");
				auto fx = attributes->get_child_optional("fx");
				auto fy = attributes->get_child_optional("fy");
				if(cx) {
					cx_.from_string(cx->data());
				}
				if(cy) {
					cy_.from_string(cy->data());
				}
				if(radius) {
					r_.from_string(radius->data());
				}
				if(fx) {
					fx_.from_string(fx->data());
				}
				if(fy) {
					fy_.from_string(fy->data());
				}
			}
		}

		radial_gradient::~radial_gradient()
		{
		}

		void radial_gradient::handle_set_source(render_context& ctx) const
		{
			// XXX to do.
			//cairo_pattern_t* pattern = cairo_pattern_create_radial();
			//pattern_.reset(pattern, [](cairo_pattern_t* p) { cairo_pattern_destroy(p); });
			//apply_transforms(pattern);
			//cairo_set_source(ctx.cairo(), pattern);
		}
	}
}
