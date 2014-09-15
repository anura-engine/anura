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

#pragma once

#include <memory>
#include "../variant.hpp"
#include "Blittable.hpp"

namespace KRE
{
	class RenderTarget : public Blittable
	{
	public:
		explicit RenderTarget(unsigned width, unsigned height, 
			unsigned color_plane_count=1, 
			bool depth=false, 
			bool stencil=false, 
			bool use_multi_sampling=false, 
			unsigned multi_samples=0);
		explicit RenderTarget(const variant& node);
		virtual ~RenderTarget();

		variant write();

		void create();
		void apply();
		void unapply();
		void clear();

		unsigned width() const { return width_; }
		unsigned height() const { return height_; }
		unsigned getColorPlanes() const { return color_attachments_; }
		bool getDepthPlane() const { return depth_attachment_; }
		bool getStencilPlane() const { return stencil_attachment_; }
		bool usesMultiSampling() const { return multi_sampling_; }
		unsigned getMultiSamples() const { return multi_samples_; }

		void setClearColor(int r, int g, int b, int a=255);
		void setClearColor(float r, float g, float b, float a=1.0f);
		void setClearColor(const Color& color);
		const Color& getClearColor() const { return clear_color_; }

		RenderTargetPtr clone();

		static RenderTargetPtr factory(const variant& node);
	private:
		virtual void handleCreate() = 0;
		virtual void handleApply() = 0;
		virtual void handleUnapply() = 0;
		virtual void handleClear() = 0;
		virtual RenderTargetPtr handleClone() = 0;

		unsigned width_;
		unsigned height_;
		unsigned color_attachments_;
		bool depth_attachment_;
		bool stencil_attachment_;
		bool multi_sampling_;
		unsigned multi_samples_;

		Color clear_color_;

		RenderTarget();
	};

	typedef std::shared_ptr<RenderTarget> RenderTargetPtr;
}
