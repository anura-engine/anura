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

#pragma once

#include <boost/property_tree/ptree.hpp>
#include <set>
#include "svg_container.hpp"
#include "svg_fwd.hpp"
#include "svg_element.hpp"
#include "svg_path_parse.hpp"
#include "svg_render.hpp"
#include "svg_transform.hpp"

namespace KRE
{
	namespace SVG
	{
		class shape : public container
		{
		public:
			shape(element* doc, const boost::property_tree::ptree& pt);
			virtual ~shape();			
		protected:
			void render_path(render_context& ctx) const;
			void clip_render_path(render_context& ctx) const;
			void stroke_and_fill(render_context& ctx) const;
		private:
			virtual void handle_render(render_context& ctx) const override;
			virtual void handle_clip_render(render_context& ctx) const override;
			std::vector<path_commandPtr> path_;
		};

		class rectangle : public shape
		{
		public:
			rectangle(element* doc, const boost::property_tree::ptree& pt);
			virtual ~rectangle();
		private:
			void render_rectangle(render_context& ctx) const;
			void handle_render(render_context& ctx) const override;
			void handle_clip_render(render_context& ctx) const override;
			svg_length x_;
			svg_length y_;
			svg_length rx_;
			svg_length ry_;
			svg_length width_;
			svg_length height_;
			bool is_rounded_;
		};
		
		class circle : public shape
		{
		public:
			circle(element* doc, const boost::property_tree::ptree& pt);
			virtual ~circle();
		private:
			void render_circle(render_context& ctx) const;
			void handle_render(render_context& ctx) const override;
			void handle_clip_render(render_context& ctx) const override;
			svg_length cx_;
			svg_length cy_;
			svg_length radius_;
		};

		class ellipse : public shape
		{
		public:
			ellipse(element* doc, const boost::property_tree::ptree& pt);
			virtual ~ellipse();
		private:
			void handle_render(render_context& ctx) const override;
			void handle_clip_render(render_context& ctx) const override;
			svg_length cx_;
			svg_length cy_;
			svg_length rx_;
			svg_length ry_;
		};

		class line : public shape
		{
		public:
			line(element* doc, const boost::property_tree::ptree& pt);
			virtual ~line();
		private:
			void render_line(render_context& ctx) const;
			void handle_render(render_context& ctx) const override;
			void handle_clip_render(render_context& ctx) const override;
			svg_length x1_;
			svg_length y1_;
			svg_length x2_;
			svg_length y2_;
		};

		class polyline : public shape
		{
		public:
			polyline(element* doc, const boost::property_tree::ptree& pt);
			virtual ~polyline();
		private:
			void render_polyline(render_context& ctx) const;
			void handle_render(render_context& ctx) const override;
			void handle_clip_render(render_context& ctx) const override;
			point_list points_;
		};

		class polygon : public shape
		{
		public:
			polygon(element* doc, const boost::property_tree::ptree& pt);
			virtual ~polygon();
		private:
			void render_polygon(render_context& ctx) const;
			void handle_render(render_context& ctx) const override;
			void handle_clip_render(render_context& ctx) const override;
			point_list points_;
		};

		class text : public shape
		{
		public:
			text(element* doc, const boost::property_tree::ptree& pt);
			virtual ~text();
		private:
			void render_text(render_context& ctx) const;
			void handle_render(render_context& ctx) const override;
			void handle_clip_render(render_context& ctx) const override;
			std::string text_;
			std::vector<svg_length> x1_;
			std::vector<svg_length> y1_;
			std::vector<svg_length> dx_;
			std::vector<svg_length> dy_;
			std::vector<double> rotate_;
			svg_length text_length_;
			enum class LengthAdjust {
				SPACING,
				SPACING_AND_GLYPHS,
			};
			LengthAdjust adjust_;
		};
	}
}
