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

#include "svg_container.hpp"
#include "svg_shapes.hpp"

namespace KRE
{
	namespace SVG
	{
		using namespace boost::property_tree;

        container::container(element* parent, const ptree& pt)
            : element(parent, pt)
		{
			if(parent == nullptr) {
				parent = this;
			}
			// can contain graphics elements and other container elements.
			// 'a', 'defs', 'glyph', 'g', 'marker', 'mask', 'missing-glyph', 'pattern', 'svg', 'switch' and 'symbol'.
			// 'circle', 'ellipse', 'image', 'line', 'path', 'polygon', 'polyline', 'rect', 'text' and 'use'.

            //auto attributes = pt.get_child_optional("<xmlattr>");
			for(auto& v : pt) {
				if(v.first == "path") {
					elements_.emplace_back(new shape(parent, v.second));
				} else if(v.first == "g") {
					elements_.emplace_back(new group(parent, v.second));
				} else if(v.first == "rect") {
					elements_.emplace_back(new rectangle(parent, v.second));
				} else if(v.first == "text") {
					elements_.emplace_back(new text(parent, v.second));
				} else if(v.first == "tspan") {
					elements_.emplace_back(new text(parent, v.second, true));
				} else if(v.first == "line") {
					elements_.emplace_back(new line(parent,v.second));
				} else if(v.first == "circle") {
					elements_.emplace_back(new circle(parent,v.second));
				} else if(v.first == "polygon") {
					elements_.emplace_back(new polygon(parent,v.second));
				} else if(v.first == "polyline") {
					elements_.emplace_back(new polyline(parent,v.second));
				} else if(v.first == "ellipse") {
					elements_.emplace_back(new ellipse(parent,v.second));
				} else if(v.first == "desc") {
					// ignore
				} else if(v.first == "title") {
					// ignore
				} else if(v.first == "use") {
					elements_.emplace_back(new use_element(parent,v.second));
				} else if(v.first == "defs") {
					elements_.emplace_back(new defs(parent,v.second));
				} else if(v.first == "clipPath") {
					elements_.emplace_back(new clip_path(parent,v.second));
				} else if(v.first == "<xmlattr>") {
					// ignore
				} else if(v.first == "<xmlcomment>") {
					// ignore
				} else {
					LOG_ERROR("SVG: svg unhandled child element: " << v.first << " : " << v.second.data());
				}
			}
		}

		container::~container()
		{
		}

		void container::handle_resolve()
		{
			for(auto e : elements_) {
				e->resolve();
			}
		}

		void container::render_children(render_context& ctx) const
		{
			cairo_push_group(ctx.cairo());
			for(auto s : elements_) {
				s->render(ctx);
			}
			cairo_pop_group_to_source(ctx.cairo());
			cairo_paint_with_alpha(ctx.cairo(), ctx.opacity_top());
		}

		void container::clip_render_children(render_context& ctx) const
		{
			for(auto s : elements_) {
				s->clip_render(ctx);
			}
		}

		void container::handle_render(render_context& ctx) const
		{
			ASSERT_LOG(false, "Calling container::handle_render is probably an error.");
		}

		void container::handle_clip_render(render_context& ctx) const
		{
			// do nothing
		}

		element_ptr container::handle_find_child(const std::string& id) const
		{
			for(auto e : elements_) {
				if(e->id() == id) {
					return e;
				}
				auto child = e->find_child(id);
				if(child) {
					return child;
				}
			}
			return element_ptr();
		}

		svg::svg(element* parent, const ptree& pt)
			: container(parent, pt)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");

			if(attributes) {
				auto version = attributes->get_child_optional("version");
				if(version) {
					version_ = version->data();
				}

				auto base_profile = attributes->get_child_optional("baseProfile");
				if(base_profile) {
					base_profile_ = base_profile->data();
				}

				auto content_script_type = attributes->get_child_optional("contentScriptType");
				if(content_script_type) {
					content_script_type_ = content_script_type->data();
				}

				auto content_style_type = attributes->get_child_optional("contentStyleType");
				if(content_style_type) {
					content_style_type_ = content_style_type->data();
				}

				auto xml_ns = attributes->get_child_optional("xml:ns");
				if(xml_ns) {
					xmlns_ = xml_ns->data();
				}

				// todo: zoom_and_pan_
				// todo: preserve_aspect_ratio_

				/*
				auto version = attributes->get_child_optional("version");
				if(version) {
					version_ = version->data();
				}
				*/
			}
		}

		svg::~svg()
		{
		}

		void svg::handle_render(render_context& ctx) const
		{
			render_children(ctx);
		}

		void svg::handle_clip_render(render_context& ctx) const
		{
			clip_render_children(ctx);
		}

		group::group(element* parent, const ptree& pt)
			: container(parent, pt)
		{
		}

		group::~group()
		{
		}

		void group::handle_render(render_context& ctx) const
		{
			render_children(ctx);
		}

		void group::handle_clip_render(render_context& ctx) const
		{
			clip_render_children(ctx);
		}

		defs::defs(element* parent, const ptree& pt)
			: container(parent, pt)
		{
		}

		defs::~defs()
		{
		}

		void defs::handle_render(render_context& ctx) const
		{
			// nothing to be done
			// i.e. defs is not something directly rendered it is a container
			// for holding other definitions
		}

		void defs::handle_clip_render(render_context& ctx) const
		{
			// nothing to be done
		}

		clip_path::clip_path(element* parent, const ptree& pt)
			: container(parent, pt)
		{
		}

		clip_path::~clip_path()
		{
		}

		void clip_path::handle_render(render_context& ctx) const
		{
			// nothing to be done
			// i.e. clip_path should not be rendered directly instead
			// handle_clip should be called to clip to the correct path
		}

		void clip_path::handle_clip(render_context& ctx) const
		{
			// The only class that can handle this case.
			clip_render_children(ctx);
		}

		void clip_path::handle_clip_render(render_context& ctx) const
		{
			// nothing to be done.
		}

	}
}
