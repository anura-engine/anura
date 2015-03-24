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

#include "svg_container.hpp"
#include "svg_element.hpp"
#include "svg_shapes.hpp"

namespace KRE
{
	namespace SVG
	{
		using namespace boost::property_tree;

		element::element(element* parent, const ptree& pt) 
			: core_attribs(pt), 
			  visual_attribs_(pt),
			  clipping_attribs_(pt),
			  filter_effect_attribs_(pt),
			  painting_properties_(pt),
			  marker_attribs_(pt),
			  font_attribs_(pt),
			  text_attribs_(pt),
              parent_(parent == nullptr ? this : parent),
              external_resources_required_(false),
			  x_(0,svg_length::SVG_LENGTHTYPE_NUMBER),
			  y_(0,svg_length::SVG_LENGTHTYPE_NUMBER),
			  width_(100,svg_length::SVG_LENGTHTYPE_PERCENTAGE),
			  height_(100,svg_length::SVG_LENGTHTYPE_PERCENTAGE),
			  view_box_(0.0,0.0,0.0,0.0)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto exts = attributes->get_child_optional("externalResourcesRequired");
				if(exts) {
					const std::string& s = exts->data();
					if(s == "true") {
						external_resources_required_ = true;
					} else if(s == "false") {
						external_resources_required_ = false;
					} else {
						ASSERT_LOG(false, "Unrecognised value in 'externalResourcesRequired' attribute: " << s);
					}
				}
				ASSERT_LOG(!external_resources_required_, "We don't support getting external resources.");

				auto xattr = attributes->get_child_optional("x");
				if(xattr) {
					x_ = svg_length(xattr->data());
				}
				auto yattr = attributes->get_child_optional("y");
				if(yattr) {
					y_ = svg_length(yattr->data());
				}
				auto wattr = attributes->get_child_optional("width");
				if(wattr) {
					width_ = svg_length(wattr->data());
				}
				auto hattr = attributes->get_child_optional("height");
				if(hattr) {
					height_ = svg_length(hattr->data());
				}
				auto trfs = attributes->get_child_optional("transform");
				if(trfs) {
					transforms_ = transform::factory(trfs->data());
				}
				auto vbox = attributes->get_child_optional("viewBox");
				if(vbox) {
					std::vector<std::string> buf = geometry::split(vbox->data(), ",| |;");
					ASSERT_LOG(buf.size() == 4, "viewBox should have four elements.");
					view_box_ = view_box_rect(boost::lexical_cast<double>(buf[0]),
						boost::lexical_cast<double>(buf[1]),
						boost::lexical_cast<double>(buf[2]),
						boost::lexical_cast<double>(buf[3]));
				}
			}
		}

		element::~element()
		{
		}

		struct context_save
		{
			context_save(cairo_t* ctx) : ctx_(ctx) {cairo_save(ctx);}
			~context_save() {cairo_restore(ctx_);}
			cairo_t* ctx_;
		};

		void element::render(render_context& ctx) const 
		{
			// XXX Need to do some normalising of co-ordinates to the viewBox.
			// XXX need to translate if x/y specified and use width/height from svg element if
			// overriding -- well map them to ctx.width()/ctx.height()
			// XXX also need to process preserveAspectRatio value.
			
			context_save cs(ctx.cairo());
			// disabled this as cairo scales stuff correctly. Still need to alter at some
			// point in the future.
			if(view_box_.w() != 0 && view_box_.h() != 0) {
				cairo_scale(ctx.cairo(), ctx.width()/view_box_.w(), ctx.height()/view_box_.h());
			}
			for(auto trf : transforms_) {
				trf->apply(ctx);
			}
			attribute_manager pp1(pp(), ctx);
			attribute_manager ca1(ca(), ctx);
			attribute_manager va1(va(), ctx);
			handle_render(ctx);
		}

		void element::resolve()
		{
			// Resolve any references in attributes.
			visual_attribs_.resolve(parent());
			clipping_attribs_.resolve(parent());
			filter_effect_attribs_.resolve(parent());
			painting_properties_.resolve(parent());
			marker_attribs_.resolve(parent());
			font_attribs_.resolve(parent());
			text_attribs_.resolve(parent());

			// Call derived class to fix-up any things that need resolved
			handle_resolve();
		}

		void element::handle_resolve()
		{
			// We provide a default which does nothing, overridable in base classes.
		}

		void element::clip(render_context& ctx) const
		{
			//attribute_manager pp1(pp(), ctx); <- this saves/restores cairo which kills the clipping path
			// Will need to think of something else.
			handle_clip(ctx);
		}

		void element::handle_clip(render_context& ctx) const
		{
			ASSERT_LOG(false, "handle_clip() called on non clip_path element");
		}
		
		void element::clip_render(render_context& ctx) const
		{
			handle_clip_render(ctx);
		}

        void element::apply_transforms(render_context& ctx) const
        {
            for(auto& trf : transforms_) {
                trf->apply(ctx);
            }
        }

		element_ptr element::factory(element* parent, const ptree& pt)
		{
			for(auto& v : pt) {
				if(v.first == "svg") {
					return element_ptr(new svg(parent, v.second));
				}
			}
			return element_ptr();
		}

		use_element::use_element(element* parent, const ptree& pt)
			: element(parent, pt)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto xlink_href = attributes->get_child_optional("xlink:href");
				if(xlink_href) {
					xlink_href_ = xlink_href->data();
					if(!xlink_href_.empty()) {
						if(xlink_href_[0] != '#') {
							LOG_ERROR("Only supporting inter-document cross-references: " << xlink_href_);
						} else {
							xlink_href_ = xlink_href_.substr(1);
						}
					}
				}
			}
		}

		use_element::~use_element()
		{
		}

		void use_element::handle_resolve()
		{
			if(xlink_href_.empty()) {
				return;
			}

			auto s = parent()->find_child(xlink_href_);
			if(s) {
				xlink_ref_ = s;
			} else {
				LOG_WARN("Couldn't find element '" << xlink_href_ << "' in document.");
			}
		}

		void use_element::handle_render(render_context& ctx) const
		{
			if(xlink_ref_ == nullptr) {
				return;
			}

			// Acts as a <g ...> attribute when rendered.
			double x1 = x().value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			double y1 = y().value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			//double w = width().value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			//double h = height().value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
			if(x1 != 0 || y1 != 0) {
				// The whole list_of could be more eloquently replaced by an
				// initialiser list. If certain compilers would actually bother supporting
				// C++11 features.
				std::vector<double> coords;
				coords.push_back(x1);
				coords.push_back(y1);
				auto tfr = transform::factory(TransformType::TRANSLATE,coords);
				tfr->apply(ctx);
			}
			xlink_ref_->render(ctx);
		}

		void use_element::handle_clip_render(render_context& ctx) const 
		{
			if(xlink_ref_) {
				xlink_ref_->clip_render(ctx);
			}
		}
    }
}
