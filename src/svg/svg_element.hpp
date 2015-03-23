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

#include "geometry.hpp"
#include "svg_fwd.hpp"
#include "svg_render.hpp"
#include "svg_style.hpp"
#include "utils.hpp"

namespace KRE
{
	namespace SVG
	{
		typedef geometry::Rect<double> view_box_rect;

		// container elements are as follows.
		//  'g', 'svg', 'defs', 'a', 'glyph', 'marker', 'mask', 'missing-glyph', 'pattern', 'switch', 'symbol'
		// structural elements
		//  'g', 'symbol', 'svg', 'defs', 'use'
		// shape elements
		//  'path', 'rect', 'circle', 'ellipse', 'line', 'polyline' and 'polygon'
		// animation elements
		//  'animateColor', 'animateMotion', 'animateTransform', 'animate' and 'set'
		// descriptive elements
		//  'desc', 'metadata', 'title'
		// gradient elements
		//  'linearGradient', 'radialGradient'
		// graphics element
		//  'circle', 'ellipse', 'image', 'path', 'polygon', 'polyline', 'rect', 'text', 'use'

		//, public conditional_processing_attribs, public graphical_event_attributes, public presentation_attributes
		class element : public core_attribs
		{
		public:
			element(element* parent, const boost::property_tree::ptree& svg_data);
			virtual ~element();

			void render(render_context& ctx) const;

			void apply_transforms(render_context& ctx) const;

			element_ptr find_child(const std::string& id) const {
				return handle_find_child(id);
			}

			const svg_length& x() const { return x_; }
			const svg_length& y() const { return y_; }
			const svg_length& width() const { return width_; }
			const svg_length& height() const { return height_; }

			static element_ptr factory(element* parent, const boost::property_tree::ptree& svg_data);

			void resolve();
			void clip(render_context& ctx) const;
			void clip_render(render_context& ctx) const;

			const element* parent() const { return parent_; }
		protected:
			const visual_attribs* va() const { return &visual_attribs_; }
			const clipping_attribs* ca() const { return &clipping_attribs_; }
			const filter_effect_attribs* fea() const { return &filter_effect_attribs_; }
			const painting_properties* pp() const { return &painting_properties_; }
			const marker_attribs* ma() const { return &marker_attribs_; }
			const font_attribs* fa() const { return &font_attribs_; }
			const text_attribs* ta() const { return &text_attribs_; }
		private:
			DISALLOW_COPY_ASSIGN_AND_DEFAULT(element);

			virtual void handle_render(render_context& ctx) const = 0;
			//virtual void handle_clip(render_context& ctx) const = 0;
			virtual element_ptr handle_find_child(const std::string& id) const { return element_ptr(); }
			virtual void handle_resolve();
			virtual void handle_clip(render_context& ctx) const;
			virtual void handle_clip_render(render_context& ctx) const = 0;

			// top level parent element. if nullptr then this is the top level element.
			element* parent_;

			visual_attribs visual_attribs_;
			clipping_attribs clipping_attribs_;
			filter_effect_attribs filter_effect_attribs_;
			painting_properties painting_properties_;
			marker_attribs marker_attribs_;
			font_attribs font_attribs_;
			text_attribs text_attribs_;

			// list of transforms
			std::vector<transform_ptr> transforms_;
			// CSS stylesheets aren't supported, so we don't support 'class'/'style'  attributes.
			// std::string class_;
			// std::string style_;
			bool external_resources_required_;

			svg_length x_;
			svg_length y_;
			svg_length width_;
			svg_length height_;

			view_box_rect view_box_;
		};

		// can only hold animation and descriptive elements.
		class use_element : public element
		{
		public:
			use_element(element* parent, const boost::property_tree::ptree& pt);
			virtual ~use_element();
		private:
			DISALLOW_COPY_ASSIGN_AND_DEFAULT(use_element);
			void handle_render(render_context& ctx) const override;
			void handle_clip_render(render_context& ctx) const override;
			void handle_resolve() override;
			std::string xlink_href_;
			element_ptr xlink_ref_;
		};

	}
}
