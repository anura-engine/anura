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

#pragma once

#include <boost/property_tree/ptree.hpp>
#include "svg_attribs.hpp"
#include "svg_element.hpp"
#include "svg_length.hpp"
#include "svg_paint.hpp"
#include "svg_render.hpp"
#include "svg_transform.hpp"

namespace KRE
{
	namespace SVG
	{
		enum class GradientCoordSystem {
			USERSPACE_ON_USE,
			OBJECT_BOUNDING_BOX,
		};
		enum class GradientSpreadMethod {
			PAD,
			REFLECT,
			REPEAT,
		};

		class gradient_stop : public core_attribs// : public core_attribs, public presentation_attribs
		{
		public:
			gradient_stop(element* doc, const boost::property_tree::ptree& pt);
			virtual ~gradient_stop();
			void apply(render_context& ctx, cairo_pattern_t* pattern);
		private:
			double offset_;			// number or percent.
			paint_ptr color_;
			double opacity_;
			bool opacity_set_;
		};
		typedef std::shared_ptr<gradient_stop> gradient_stop_ptr;

		class gradient : public core_attribs
		{
		public:
			gradient(element* doc, const boost::property_tree::ptree& pt);
			virtual ~gradient();
			void set_source(render_context& ctx) const;
			void apply_stops(render_context& ctx, cairo_pattern_t* pattern) const;
			void apply_transforms(cairo_pattern_t* pattern) const;
		private:
			virtual void handle_set_source(render_context& ctx) const = 0;
			GradientCoordSystem coord_system_;
			GradientSpreadMethod spread_;
			std::string xlink_href_;
			std::vector<transform_ptr> transforms_;
			std::vector<gradient_stop_ptr> stops_;
		};
		typedef std::shared_ptr<gradient> gradient_ptr;

		class linear_gradient : public gradient
		{
		public:
			linear_gradient(element* doc, const boost::property_tree::ptree& pt);
			virtual ~linear_gradient();
		private:
			virtual void handle_set_source(render_context& ctx) const override;
			svg_length x1_;
			svg_length y1_;
			svg_length x2_;
			svg_length y2_;
			mutable std::shared_ptr<cairo_pattern_t> pattern_;
		};

		class radial_gradient : public gradient
		{
		public:
			radial_gradient(element* doc, const boost::property_tree::ptree& pt);
			virtual ~radial_gradient();
		private:
			virtual void handle_set_source(render_context& ctx) const override;
			svg_length cx_;
			svg_length cy_;
			svg_length r_;
			svg_length fx_;
			svg_length fy_;
			mutable std::shared_ptr<cairo_pattern_t> pattern_;
		};
	}
}
