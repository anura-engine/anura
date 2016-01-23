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
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include "asserts.hpp"
#include "DisplayDevice.hpp"
#include "RenderTarget.hpp"
#include "variant_utils.hpp"
#include "WindowManager.hpp"

namespace KRE
{
	RenderTarget::RenderTarget(int width, int height, 
		int color_plane_count, 
		bool depth, 
		bool stencil, 
		bool use_multi_sampling, 
		int multi_samples)
		: width_(width),
		  height_(height),
		  color_attachments_(color_plane_count),
		  depth_attachment_(depth),
		  stencil_attachment_(stencil),
		  multi_sampling_(use_multi_sampling),
		  multi_samples_(multi_samples),
		  clear_color_(0.0f, 0.0f, 0.0f, 1.0f),
		  size_change_observer_handle_(-1)
	{
	}

	RenderTarget::RenderTarget(const variant& node)
		: width_(0),
		  height_(0),
		  color_attachments_(1),
		  depth_attachment_(false),
		  stencil_attachment_(false),
		  multi_sampling_(false),
		  multi_samples_(0),
		  clear_color_(0.0f, 0.0f, 0.0f, 1.0f),
		  size_change_observer_handle_(-1)
	{
		ASSERT_LOG(node.is_map(), "RenderTarget definitions must be maps: " << node.to_debug_string());
		ASSERT_LOG(node.has_key("width"), "Render target must have a 'width' attribute.");
		ASSERT_LOG(node.has_key("height"), "Render target must have a 'height' attribute.");
		width_ = node["width"].as_int32();
		height_ = node["height"].as_int32();
		if(node.has_key("color_planes")) {
			color_attachments_ = node["color_planes"].as_int32();
			ASSERT_LOG(color_attachments_ >= 0, "Number of 'color_planes' must be zero or greater: " << color_attachments_);
		}
		if(node.has_key("depth_buffer")) {
			depth_attachment_ = node["depth_buffer"].as_bool();
		}
		if(node.has_key("stencil_buffer")) {
			stencil_attachment_ = node["stencil_buffer"].as_bool();
		}
		if(node.has_key("use_multisampling")) {
			multi_sampling_ = node["use_multisampling"].as_bool();
			if(node.has_key("samples")) {
				multi_samples_ = node["samples"].as_int32();
			}
		}
		// XXX Maybe we need to add some extra filtering from min to max values based on order ?
	}

	RenderTarget::~RenderTarget()
	{
		if(size_change_observer_handle_ != -1) {
			auto wnd = WindowManager::getMainWindow();
			if(wnd) {
				wnd->unregisterSizeChangeObserver(size_change_observer_handle_);
			}
		}
	}

	void RenderTarget::on_create()
	{
		handleCreate();
	}
	
	void RenderTarget::apply(const rect& r) const
	{
		handleApply(r);
	}

	void RenderTarget::unapply() const
	{
		handleUnapply();
	}

	void RenderTarget::clear() const
	{
		handleClear();
	}

	void RenderTarget::onSizeChange(int width, int height, int flags)
	{
		if(!(flags & WindowSizeChangeFlags::NOTIFY_CANVAS_ONLY)) {
			width_ = width;
			height_ = height;
			handleSizeChange(width, height);
		}
	}

	void RenderTarget::setClearColor(int r, int g, int b, int a)
	{
		clear_color_ = Color(r,g,b,a);
	}

	void RenderTarget::setClearColor(float r, float g, float b, float a)
	{
		clear_color_ = Color(r,g,b,a);
	}

	void RenderTarget::setClearColor(const Color& color)
	{
		clear_color_ = color;
	}

	RenderTargetPtr RenderTarget::clone()
	{
		return handleClone();
	}

	std::vector<uint8_t> RenderTarget::readPixels() const
	{
		return handleReadPixels();
	}

	SurfacePtr RenderTarget::readToSurface(SurfacePtr s) const
	{
		return handleReadToSurface(s);
	}

	variant RenderTarget::write()
	{
		variant_builder res;
		res.add("width", width_);
		res.add("height", height_);
		if(color_attachments_ != 1) {
			res.add("color_planes", color_attachments_);
		}
		if(depth_attachment_) {
			res.add("depth_buffer", variant::from_bool(depth_attachment_));
		}
		if(stencil_attachment_) {
			res.add("stencil_buffer", variant::from_bool(stencil_attachment_));
		}
		if(multi_sampling_) {
			res.add("use_multisampling", variant::from_bool(multi_sampling_));
			res.add("samples", multi_samples_);
		}
		return res.build();
	}

	RenderTargetPtr RenderTarget::create(const variant& node)
	{
		return DisplayDevice::renderTargetInstance(node);
	}

	RenderTargetPtr RenderTarget::create(int width, int height, 
		unsigned color_plane_count, 
		bool depth, 
		bool stencil, 
		bool use_multi_sampling, 
		unsigned multi_samples)
	{
		auto rt = DisplayDevice::renderTargetInstance(width, height, color_plane_count, depth, stencil, use_multi_sampling, multi_samples);
		auto wnd = WindowManager::getMainWindow();
		rt->size_change_observer_handle_ = wnd->registerSizeChangeObserver(std::bind(&RenderTarget::onSizeChange, rt.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		return rt;
	}

}
