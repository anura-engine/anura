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
#include "variant.hpp"
#include "Blittable.hpp"

namespace KRE
{
	class RenderTarget : public Blittable
	{
	public:
		virtual ~RenderTarget();

		variant write();

		void on_create();
		void apply(const rect& r=rect()) const;
		void unapply() const;
		void clear() const;

		void renderToThis(const rect& r=rect()) const { apply(r); }
		void renderToPrevious() const { unapply(); }

		struct RenderScope {
			RenderScope(RenderTargetPtr rt, const rect& r=rect()) : rt_(rt) {
				if(rt_) {
					rt_->clear();
					rt_->apply(r);
				}
			}
			~RenderScope() {
				if(rt_) {
					rt_->unapply();
				}
			}
			RenderTargetPtr rt_;
		};

		int width() const { return width_; }
		int height() const { return height_; }
		int getColorPlanes() const { return color_attachments_; }
		bool getDepthPlane() const { return depth_attachment_; }
		bool getStencilPlane() const { return stencil_attachment_; }
		bool usesMultiSampling() const { return multi_sampling_; }
		unsigned getMultiSamples() const { return multi_samples_; }

		void setClearColor(int r, int g, int b, int a=255);
		void setClearColor(float r, float g, float b, float a=1.0f);
		void setClearColor(const Color& color);
		const Color& getClearColor() const { return clear_color_; }

		RenderTargetPtr clone();

		// N.B. these function might be slow, not recommend for use in render pipeline.
		// will only work if the framebuffer has been written, obviously.
		std::vector<uint8_t> readPixels() const;
		SurfacePtr readToSurface(SurfacePtr s=nullptr) const;

		void onSizeChange(int width, int height, int flags);

		static RenderTargetPtr create(int width, int height, 
			unsigned color_plane_count=1, 
			bool depth=false, 
			bool stencil=false, 
			bool use_multi_sampling=false, 
			unsigned multi_samples=0);
		static RenderTargetPtr create(const variant& node);
	protected:
		explicit RenderTarget(int width, int height, 
			int color_plane_count, 
			bool depth, 
			bool stencil, 
			bool use_multi_sampling, 
			int multi_samples);
		explicit RenderTarget(const variant& node);
	private:
		virtual void handleCreate() = 0;
		virtual void handleApply(const rect& r) const = 0;
		virtual void handleUnapply() const = 0;
		virtual void handleClear() const = 0;
		virtual void handleSizeChange(int width, int height) = 0;
		virtual RenderTargetPtr handleClone() = 0;
		virtual std::vector<uint8_t> handleReadPixels() const = 0;
		virtual SurfacePtr handleReadToSurface(SurfacePtr s) const = 0;

		int width_;
		int height_;
		int color_attachments_;
		bool depth_attachment_;
		bool stencil_attachment_;
		bool multi_sampling_;
		int multi_samples_;

		Color clear_color_;

		int size_change_observer_handle_;

		RenderTarget();
	};

	typedef std::shared_ptr<RenderTarget> RenderTargetPtr;
}
