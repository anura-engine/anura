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
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include "../asserts.hpp"
#include "RenderTarget.hpp"

namespace KRE
{
	RenderTarget::RenderTarget(unsigned width, unsigned height, 
		unsigned color_plane_count, 
		bool depth, 
		bool stencil, 
		bool use_multi_sampling, 
		unsigned multi_samples)
		: width_(width),
		height_(height),
		color_attachments_(color_plane_count),
		depth_attachment_(depth),
		stencil_attachment_(stencil),
		multi_sampling_(use_multi_sampling),
		multi_samples_(multi_samples),
		clear_color_(0.0f, 0.0f, 0.0f, 1.0f)
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
		clear_color_(0.0f, 0.0f, 0.0f, 1.0f)
	{
		ASSERT_LOG(node.has_key("width"), "Render target must have a 'width' attribute.");
		ASSERT_LOG(node.has_key("height"), "Render target must have a 'height' attribute.");
		width_ = node["width"].as_int();
		height_ = node["height"].as_int();
		if(node.has_key("color_planes")) {
			color_attachments_ = node["color_planes"].as_int();
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
				multi_samples_ = node["samples"].as_int();
			}
		}
	}

	RenderTarget::~RenderTarget()
	{
	}

	void RenderTarget::Create()
	{
		HandleCreate();
	}
	
	void RenderTarget::Apply()
	{
		HandleApply();
	}

	void RenderTarget::Unapply()
	{
		HandleUnapply();
	}

	void RenderTarget::Clear()
	{
		HandleClear();
	}

	void RenderTarget::SetClearColor(int r, int g, int b, int a)
	{
		clear_color_ = Color(r,g,b,a);
	}

	void RenderTarget::SetClearColor(float r, float g, float b, float a)
	{
		clear_color_ = Color(r,g,b,a);
	}

	void RenderTarget::SetClearColor(const Color& color)
	{
		clear_color_ = color;
	}
}
