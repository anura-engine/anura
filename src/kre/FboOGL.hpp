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

#include <array>
#include <vector>
#include "AttributeSet.hpp"
#include "RenderTarget.hpp"
#include "Util.hpp"
#include "variant.hpp"

namespace KRE
{
	class FboOpenGL : public RenderTarget
	{
	public:
		explicit FboOpenGL(int width, int height, 
			int color_plane_count=1, 
			bool depth=false, 
			bool stencil=false, 
			bool use_multi_sampling=false, 
			int multi_samples=0);		
		explicit FboOpenGL(const variant& node);
		FboOpenGL(const FboOpenGL& op);
		virtual ~FboOpenGL();
		virtual void preRender(const WindowPtr&) override;
	private:
		void handleCreate() override;
		void handleApply(const rect& r) const override;
		void handleUnapply() const override;
		void handleClear() const override;
		void handleSizeChange(int width, int height) override;
		RenderTargetPtr handleClone() override;
		std::vector<uint8_t> handleReadPixels() const override;
		SurfacePtr handleReadToSurface(SurfacePtr s) const override;
		void getDSInfo(GLenum& ds_attachment, GLenum& depth_stencil_internal_format);
		bool uses_ext_;
		std::shared_ptr<GLuint> depth_stencil_buffer_id_;
		std::shared_ptr<GLuint> framebuffer_id_;
		std::shared_ptr<GLuint> sample_framebuffer_id_;
		std::shared_ptr<std::vector<GLuint>> renderbuffer_id_;

		int tex_width_;
		int tex_height_;
		mutable bool applied_;

		FboOpenGL();
		void operator=(const FboOpenGL&);
	};
}
