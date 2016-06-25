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

#include <GL/glew.h>

#include <cstddef>
#include <stack>

#include "asserts.hpp"
#include "DisplayDevice.hpp"
#include "FboOGL.hpp"
#include "TextureOGL.hpp"
#include "TextureUtils.hpp"
#include "WindowManager.hpp"

namespace KRE
{
	namespace
	{
		const int default_framebuffer_id = 0;

		struct fbo_info
		{
			explicit fbo_info(int i, const rect& vp) : id(i), viewport(vp) {}
			int id;
			rect viewport;
		};
		typedef std::stack<fbo_info> fbo_stack_type;
		fbo_stack_type& get_fbo_stack()
		{
			static fbo_stack_type res;
			//if(res.empty()) {
				// place the default on the stack
			//	WindowPtr wnd = WindowManager::getMainWindow();
			//	res.emplace(default_framebuffer_id, rect(0, 0, wnd->width(), wnd->height()));
			//}
			return res;
		}
	}

	FboOpenGL::FboOpenGL(int width, int height, 
		int color_plane_count, 
		bool depth, 
		bool stencil, 
		bool use_multi_sampling, 
		int multi_samples)
		: RenderTarget(width, height, color_plane_count, depth, stencil, use_multi_sampling, multi_samples),
		uses_ext_(false),
		depth_stencil_buffer_id_(0),
		tex_width_(0),
		tex_height_(0),
		applied_(false)
	{
		on_create();
	}

	FboOpenGL::FboOpenGL(const variant& node)
		: RenderTarget(node),
		uses_ext_(false),
		depth_stencil_buffer_id_(0),
		tex_width_(0),
		tex_height_(0),
		applied_(false)
	{
		on_create();
	}

	FboOpenGL::FboOpenGL(const FboOpenGL& op)
		: RenderTarget(op),
		uses_ext_(false),
		depth_stencil_buffer_id_(0),
		tex_width_(0),
		tex_height_(0),
		applied_(false)
	{
		if(op.tex_height_ != 0 && op.tex_width_ != 0) {
			on_create();
		}
	}

	void FboOpenGL::handleCreate()
	{
		GLenum depth_stencil_internal_format = GL_NONE;
		GLenum ds_attachment = GL_NONE;
		getDSInfo(ds_attachment, depth_stencil_internal_format);

		//tex_width_ = next_power_of_two(width());
		//tex_height_ = next_power_of_two(height());

		// check for fbo support
		if(GLEW_ARB_framebuffer_object) {
			// XXX we need to add some hints about what size depth and stencil buffers to use.
			if(usesMultiSampling() && !GLEW_EXT_framebuffer_multisample) {
				LOG_ERROR("A multi-sample framebuffer was requested, but multi-sampling isn't available. Defaulting to single sampling.");
			}
			if(usesMultiSampling() && GLEW_EXT_framebuffer_multisample) {
				int color_planes = getColorPlanes();

				// for output texture
				auto tex = Texture::createTextureArray(color_planes, width(), height(), PixelFormat::PF::PIXELFORMAT_RGBA8888, TextureType::TEXTURE_2D);
				tex->setSourceRect(-1, rect(0, 0, width(), height()));
				setTexture(tex);
				tex_width_ = tex->actualWidth();
				tex_height_ = tex->actualHeight();

				renderbuffer_id_ = std::shared_ptr<std::vector<GLuint>>(new std::vector<GLuint>, [color_planes](std::vector<GLuint>* id) {
					glBindRenderbuffer(GL_RENDERBUFFER, 0); 
					glDeleteRenderbuffers(color_planes, &(*id)[0]); 
					delete id;
				});
				renderbuffer_id_->resize(color_planes);
				glGenRenderbuffers(color_planes, &(*renderbuffer_id_)[0]);
				for(int n = 0; n != color_planes; ++n) {
					glBindRenderbuffer(GL_RENDERBUFFER, (*renderbuffer_id_)[n]);
					glRenderbufferStorageMultisample(GL_RENDERBUFFER, getMultiSamples(), GL_RGBA, tex_width_, tex_height_);
				}
				glBindRenderbuffer(GL_RENDERBUFFER, 0);
				if(getDepthPlane() || getStencilPlane()) {
					depth_stencil_buffer_id_ = std::shared_ptr<GLuint>(new GLuint, [](GLuint* id){ 
						glBindRenderbuffer(GL_RENDERBUFFER, 0); 
						glDeleteRenderbuffers(1, id); 
						delete id; 
					});
					glGenRenderbuffers(1, depth_stencil_buffer_id_.get());
					glBindRenderbuffer(GL_RENDERBUFFER, *depth_stencil_buffer_id_);
					glRenderbufferStorageMultisample(GL_RENDERBUFFER, getMultiSamples(), depth_stencil_internal_format, tex_width_, tex_height_);
					glBindRenderbuffer(GL_RENDERBUFFER, 0);				
				}

				sample_framebuffer_id_ = std::shared_ptr<GLuint>(new GLuint, [](GLuint* id) {
					glDeleteFramebuffers(1, id); 
					delete id;
				});
				glGenFramebuffers(1, sample_framebuffer_id_.get());
				glBindFramebuffer(GL_FRAMEBUFFER, *sample_framebuffer_id_);
				if(getDepthPlane() || getStencilPlane()) {
					glFramebufferRenderbuffer(GL_FRAMEBUFFER, ds_attachment, GL_RENDERBUFFER, *depth_stencil_buffer_id_);
				}
				for(int n = 0; n != color_planes; ++n) {
					glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + n, GL_RENDERBUFFER, (*renderbuffer_id_)[n]);
				}
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				ASSERT_LOG(status != GL_FRAMEBUFFER_UNSUPPORTED, "Framebuffer not supported error.");
				ASSERT_LOG(status == GL_FRAMEBUFFER_COMPLETE, "Framebuffer completion status not indicated: " << status);

				// output framebuffer.
				if(getDepthPlane() || getStencilPlane()) {
					depth_stencil_buffer_id_ = std::shared_ptr<GLuint>(new GLuint, [](GLuint* id){ 
						glBindRenderbuffer(GL_RENDERBUFFER, 0); 
						glDeleteRenderbuffers(1, id); 
						delete id; 
					});
					glGenRenderbuffers(1, depth_stencil_buffer_id_.get());
					glBindRenderbuffer(GL_RENDERBUFFER, *depth_stencil_buffer_id_);
					glRenderbufferStorage(GL_RENDERBUFFER, depth_stencil_internal_format, tex_width_, tex_height_);
					glBindRenderbuffer(GL_RENDERBUFFER, 0);
				}

				framebuffer_id_ = std::shared_ptr<GLuint>(new GLuint, [](GLuint* id) {
					glDeleteFramebuffers(1, id); 
					delete id;
				});
				glGenFramebuffers(1, framebuffer_id_.get());
				glBindFramebuffer(GL_FRAMEBUFFER,  *framebuffer_id_);
				// attach the texture to FBO color attachment point
				for(int n = 0; n != color_planes; ++n) {
					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+n, GL_TEXTURE_2D, tex->id(n), 0);
				}
				if(depth_stencil_buffer_id_) {
					glFramebufferRenderbuffer(GL_FRAMEBUFFER, ds_attachment, GL_RENDERBUFFER, *depth_stencil_buffer_id_);
				}
				status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				ASSERT_LOG(status != GL_FRAMEBUFFER_UNSUPPORTED, "Framebuffer not supported error.");
				ASSERT_LOG(status == GL_FRAMEBUFFER_COMPLETE, "Framebuffer completion status not indicated: " << status);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);

			} else {
				int color_planes = getColorPlanes();
				auto tex = Texture::createTextureArray(color_planes, width(), height(), PixelFormat::PF::PIXELFORMAT_RGBA8888, TextureType::TEXTURE_2D);
				tex->setSourceRect(-1, rect(0, 0, width(), height()));
				setTexture(tex);

				tex_width_ = tex->actualWidth();
				tex_height_ = tex->actualHeight();

				if(getDepthPlane() || getStencilPlane()) {
					depth_stencil_buffer_id_ = std::shared_ptr<GLuint>(new GLuint, [](GLuint* id){ 
						glBindRenderbuffer(GL_RENDERBUFFER, 0); 
						glDeleteRenderbuffers(1, id); 
						delete id; 
					});
					glGenRenderbuffers(1, depth_stencil_buffer_id_.get());
					glBindRenderbuffer(GL_RENDERBUFFER, *depth_stencil_buffer_id_);
					glRenderbufferStorage(GL_RENDERBUFFER, depth_stencil_internal_format, tex_width_, tex_height_);
					glBindRenderbuffer(GL_RENDERBUFFER, 0);
				}

				framebuffer_id_ = std::shared_ptr<GLuint>(new GLuint, [](GLuint* id) {
					glDeleteFramebuffers(1, id); 
					delete id;
				});
				glGenFramebuffers(1, framebuffer_id_.get());
				glBindFramebuffer(GL_FRAMEBUFFER, *framebuffer_id_);
				// attach the texture to FBO color attachment point
				for(int n = 0; n != color_planes; ++n) {
					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+n, GL_TEXTURE_2D, tex->id(n), 0);
				}
				if(depth_stencil_buffer_id_) {
					glFramebufferRenderbuffer(GL_FRAMEBUFFER, ds_attachment, GL_RENDERBUFFER, *depth_stencil_buffer_id_);
				}
				if(color_planes > 1) {
					std::vector<GLenum> bufs;
					for(int n = 0; n != color_planes; ++n) {
						bufs.emplace_back(GL_COLOR_ATTACHMENT0 + n);
					}
					glDrawBuffers(color_planes, bufs.data());
				}
				GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				ASSERT_LOG(status != GL_FRAMEBUFFER_UNSUPPORTED, "Framebuffer not supported error.");
				ASSERT_LOG(status == GL_FRAMEBUFFER_COMPLETE, "Framebuffer completion status not indicated: " << status);
			}
		} else if(GLEW_EXT_framebuffer_object) {
			ASSERT_LOG(!(usesMultiSampling() && !GLEW_EXT_framebuffer_multisample), "Multi-sample texture requested but hardware doesn't support multi-sampling.");
			ASSERT_LOG(!((getDepthPlane() || getStencilPlane()) && !GLEW_EXT_packed_depth_stencil), "Depth or Stencil plane required but hardware doesn't support it.");
			uses_ext_ = true;
			// XXX wip
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		setOrder(999999);
		setMirrorHoriz(true);
	}

	FboOpenGL::~FboOpenGL()
	{
	}

	void FboOpenGL::preRender(const WindowPtr& wnd)
	{
		ASSERT_LOG(framebuffer_id_ != nullptr, "Framebuffer object hasn't been created.");
		if(sample_framebuffer_id_) {
			// using multi-sampling
			// blit from multisample FBO to final FBO
			glBindFramebuffer(GL_READ_FRAMEBUFFER, *sample_framebuffer_id_);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, *framebuffer_id_);
			glDrawBuffer(GL_BACK);
			glBlitFramebuffer(0, 0, width(), height(), 
				0, 0, width(), height(), 
				GL_COLOR_BUFFER_BIT | (getDepthPlane() ? GL_DEPTH_BUFFER_BIT : 0) | (getStencilPlane() ? GL_STENCIL_BUFFER_BIT : 0), 
				GL_NEAREST);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		}

		Blittable::preRender(wnd);
	}

	void FboOpenGL::handleApply(const rect& r) const
	{
		ASSERT_LOG(framebuffer_id_ != nullptr, "Framebuffer object hasn't been created.");
		if(sample_framebuffer_id_) {
			glBindFramebuffer(GL_FRAMEBUFFER, *sample_framebuffer_id_);
			get_fbo_stack().emplace(*sample_framebuffer_id_, r);
		} else {
			glBindFramebuffer(GL_FRAMEBUFFER, *framebuffer_id_);
			get_fbo_stack().emplace(*framebuffer_id_, r);
		}

		applied_ = true;
		DisplayDevice::getCurrent()->setViewPort(r);
	}

	void FboOpenGL::handleUnapply() const
	{
		ASSERT_LOG(!get_fbo_stack().empty(), "FBO id stack was empty. This should never happen if calls to apply/unapply are balanced.");
		// This should be our id at top.
		auto chk = get_fbo_stack().top(); get_fbo_stack().pop();
		if(sample_framebuffer_id_) {
			ASSERT_LOG(chk.id == *sample_framebuffer_id_, "Our FBO id was not the one at the top of the stack. This should never happen if calls to apply/unapply are balanced.");
		} else {
			ASSERT_LOG(chk.id == *framebuffer_id_, "Our FBO id was not the one at the top of the stack. This should never happen if calls to apply/unapply are balanced.");
		}
		//ASSERT_LOG(!get_fbo_stack().empty(), "FBO id stack was empty. This should never happen if calls to apply/unapply are balanced.");

		if(get_fbo_stack().empty()) {
			WindowPtr wnd = WindowManager::getMainWindow();
			glBindFramebuffer(GL_FRAMEBUFFER, default_framebuffer_id);
			DisplayDevice::getCurrent()->setViewPort(0, 0, wnd->width(), wnd->height());
		} else {
			auto& last = get_fbo_stack().top();
			glBindFramebuffer(GL_FRAMEBUFFER, last.id);
			DisplayDevice::getCurrent()->setViewPort(last.viewport);
		}

		applied_ = false;
		//setChanged();
	}

	void FboOpenGL::handleSizeChange(int w, int h)
	{
		depth_stencil_buffer_id_.reset();
		framebuffer_id_.reset();
		sample_framebuffer_id_.reset();
		renderbuffer_id_.reset();
		handleCreate();
	}

	void FboOpenGL::handleClear() const
	{
		bool appl = applied_;
		if(!appl) {
			handleApply(rect());
		}
		auto& color = getClearColor();
		glClearColor(color.red(), color.green(), color.blue(), color.alpha());
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		if(!appl) {
			handleUnapply();
		}
	}

	RenderTargetPtr FboOpenGL::handleClone()
	{
		return std::make_shared<FboOpenGL>(*this);
	}

	void FboOpenGL::getDSInfo(GLenum& ds_attachment, GLenum& depth_stencil_internal_format)
	{
		if(getDepthPlane() || getStencilPlane()) {
			if(getDepthPlane()) {
				if(getStencilPlane()) {
					depth_stencil_internal_format = GL_DEPTH24_STENCIL8;
					ds_attachment = GL_DEPTH_STENCIL_ATTACHMENT;
				} else {
					depth_stencil_internal_format = GL_DEPTH_COMPONENT16;
					ds_attachment = GL_DEPTH_ATTACHMENT;
				}
			} else {
				depth_stencil_internal_format = GL_STENCIL_INDEX8;
				ds_attachment = GL_STENCIL_ATTACHMENT;
			}
		}
	}

	std::vector<uint8_t> FboOpenGL::handleReadPixels() const
	{
		const int stride = tex_width_ * 4; // 4 bytes per pixel, which is the format we're requesting.

		std::vector<uint8_t> pixels;
		pixels.resize(stride * tex_height_);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, *framebuffer_id_);
		glReadPixels(0, 0, tex_width_, tex_height_, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glBindFramebuffer(GL_READ_FRAMEBUFFER, get_fbo_stack().top().id);

		// copies rows bottom to top.
		std::vector<uint8_t> res;
		res.resize(stride * tex_height_);
		std::vector<uint8_t>::iterator cp_data = res.begin();
		for(auto it = pixels.begin() + (tex_height_-1)*stride; it != pixels.begin(); it -= stride) {
			std::copy(it, it + stride, cp_data);
			cp_data += stride;
		}
		return res;
	}

	SurfacePtr FboOpenGL::handleReadToSurface(SurfacePtr s) const
	{
		//if(s == nullptr) {
			s = Surface::create(tex_width_, tex_height_, PixelFormat::PF::PIXELFORMAT_ABGR8888);
		//}

		auto pixels = handleReadPixels();
		s->writePixels(pixels.data(), pixels.size());

		return s;
	}
}
