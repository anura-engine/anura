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
#include "svg_fwd.hpp"
#include "svg_gradient.hpp"
#include "svg_render.hpp"
#include "svg_transform.hpp"

namespace KRE
{
	namespace SVG
	{
		enum class ZoomAndPan {
			DISABLE,
			MAGNIFY,
		};

		class container : public element
		{
		public:
			container(element* parent, const boost::property_tree::ptree& pt);
			virtual ~container();
		protected:
			void render_children(render_context& ctx) const;
			void clip_render_children(render_context& ctx) const;
		private:
			virtual void handle_resolve() override;
			virtual void handle_render(render_context& ctx) const override;
			virtual void handle_clip_render(render_context& ctx) const override;
			element_ptr handle_find_child(const std::string& id) const override;

			// Shape/Structural/Gradient elements
			std::vector<element_ptr> elements_;
		};

		class svg : public container
		{
		public:
			svg(element* parent, const boost::property_tree::ptree& pt);
			virtual ~svg();
		private:
			void handle_render(render_context& ctx) const override;
			void handle_clip_render(render_context& ctx) const override;

			std::string version_;
			std::string base_profile_;
			std::string content_script_type_;
			std::string content_style_type_;
			std::string xmlns_;
			//PreserveAspectRatio preserve_aspect_ratio_;
			ZoomAndPan zoom_and_pan_;
		};

		// Not rendered directly. Only rendered when called from a 'use' element.
		class symbol : public container
		{
		public:
			symbol(element* parent, const boost::property_tree::ptree& pt);
			virtual ~symbol();
		private:
			void handle_render(render_context& ctx) const override;
			void handle_clip_render(render_context& ctx) const override;
		};

		class group : public container
		{
		public:
			group(element* parent, const boost::property_tree::ptree& pt);
			virtual ~group();
		private:
			void handle_render(render_context& ctx) const override;
			void handle_clip_render(render_context& ctx) const override;
		};

		class clip_path : public container
		{
		public:
			clip_path(element* parent, const boost::property_tree::ptree& pt);
			virtual ~clip_path();
		private:
			void handle_render(render_context& ctx) const override;
			void handle_clip(render_context& ctx) const override;
			void handle_clip_render(render_context& ctx) const override;
		};

		// Used only for looking up child elements. Not rendered directly.
		class defs : public container
		{
		public:
			defs(element* parent, const boost::property_tree::ptree& pt);
			virtual ~defs();
		private:
			void handle_render(render_context& ctx) const override;
			void handle_clip_render(render_context& ctx) const override;
		};
	}
}
