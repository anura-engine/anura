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

#include <stack>

#include "xhtml_render_ctx.hpp"

namespace xhtml
{
	using namespace css;

	namespace 	
	{
		const int max_properties = static_cast<int>(Property::MAX_PROPERTIES);

		std::stack<KRE::FontHandlePtr>& get_font_handle_stack()
		{
			static std::stack<KRE::FontHandlePtr> res;
			return res;
		}

		KRE::FontHandlePtr get_font_handle()
		{
			RenderContext& ctx = RenderContext::get();
			KRE::FontHandlePtr parent_font = get_font_handle_stack().empty() ? nullptr : get_font_handle_stack().top();
			auto ff = ctx.getComputedValue(Property::FONT_FAMILY)->asType<FontFamily>()->getFontList();
			auto fs = ctx.getComputedValue(Property::FONT_SIZE)->asType<FontSize>()->compute(static_cast<FixedPoint>((parent_font ? parent_font->getFontSize() : 12.0f) * 65536.0f), ctx.getDPI());
			auto fw = ctx.getComputedValue(Property::FONT_WEIGHT)->asType<FontWeight>()->compute(400/*parent_font ? parent_font->getFontWeight() : 400*/);
			auto ft = ctx.getComputedValue(Property::FONT_STYLE)->getEnum<FontStyle>();
			return KRE::FontDriver::getFontHandle(ff, static_cast<float>(fs)/65536.0f*72.0f/static_cast<float>(ctx.getDPI())/*, fw, ft*/);
		}

		typedef std::vector<std::stack<StylePtr>> stack_array;
		stack_array& get_stack_array()
		{
			static stack_array res;
			if(res.empty()) {
				// initialise the stack array with default property values.
				res.resize(max_properties);
				for(int n = 0; n != max_properties; ++n) {
					auto& pi = get_default_property_info(static_cast<Property>(n));
					res[n].emplace(pi.obj);
				}
			}
			return res;
		}

		bool is_font_property(Property p)
		{
			return p == Property::FONT_FAMILY 
				|| p == Property::FONT_SIZE 
				|| p == Property::FONT_WEIGHT 
				|| p == Property::FONT_STYLE;
		}
	}

	RenderContextManager::RenderContextManager()
	{
		auto stk = get_stack_array();
		get_font_handle_stack().emplace(get_font_handle());
	}

	RenderContextManager::~RenderContextManager()
	{
	}

	RenderContext::RenderContext()
		: dpi_scale_(96)
	{
	}

	RenderContext& RenderContext::get()
	{
		static RenderContext res;
		return res;
	}

	const StylePtr& RenderContext::getComputedValue(css::Property p) const
	{
		int ndx = static_cast<int>(p);
		ASSERT_LOG(ndx < max_properties, "Index in property list: " << ndx << " is outside of legal bounds: 0-" << (max_properties-1));
		ASSERT_LOG(!get_stack_array()[ndx].empty(), "Logic error, computed value for Property " << ndx << "(" << get_property_name(p) << ") is empty.");
		return get_stack_array()[ndx].top();
	}

	std::vector<StylePtr> RenderContext::getCurrentStyles() const
	{
		std::vector<StylePtr> res;
		res.resize(max_properties);
		for(int ndx = 0; ndx != max_properties; ++ndx) {
			res[ndx] = get_stack_array()[ndx].top();
		}	
		return res;
	}

	KRE::FontHandlePtr RenderContext::getFontHandle() const
	{
		ASSERT_LOG(!get_font_handle_stack().empty(), "Logic error, font handle stack is empty.");
		return get_font_handle_stack().top();
	}

	RenderContext::Manager::Manager(const css::PropertyList& plist)
		: update_list(),
		  pushed_font_change_(false)
	{
		for(int n = 0; n != max_properties; ++n) {
			const Property p = static_cast<Property>(n);
			auto& stk = get_stack_array()[n];
			const StylePtr style = plist.getProperty(p);
			if(style == nullptr) {
				// get default property
				auto& pinfo = get_default_property_info(p);
				// default properties that aren't inherited pushed, if they aren't the current default.
				if(!pinfo.inherited) {
					update_list.emplace_back(n);
					get_stack_array()[n].emplace(pinfo.obj);
					if(is_font_property(p)) {
						pushed_font_change_ = true;
					}
				}
			} else if(!style->isInherited()) {
				// Is the property isn't marked as being inherited, we handle it here.
				if(style != get_stack_array()[n].top()) {
					update_list.emplace_back(n);				
					get_stack_array()[n].emplace(style);
					if(is_font_property(p)) {
						pushed_font_change_ = true;
					}
				}
			}
		}
		// If font parameters changed that would cause a new font handle to be allocated we do it here
		if(pushed_font_change_) {
			get_font_handle_stack().emplace(get_font_handle());
		}
	}

	RenderContext::Manager::~Manager()
	{
		for(auto n : update_list) {
			get_stack_array()[n].pop();
			ASSERT_LOG(!get_stack_array()[n].empty(), "Logical error, all the values in the property stack array are empty.");
		}
		if(pushed_font_change_) {
			get_font_handle_stack().pop();
		}
	}
}
