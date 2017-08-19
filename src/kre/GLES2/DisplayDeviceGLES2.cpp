/*
	Copyright (C) 2013-2016 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#pragma comment(lib, "libGLESv2")
#pragma comment(lib, "libEGL")

#include <numeric>

#include "SDL_opengles2.h"

#include "asserts.hpp"
#include "AttributeSetGLES2.hpp"
#include "BlendGLES2.hpp"
#include "CameraObject.hpp"
#include "ColorScope.hpp"
#include "CanvasGLES2.hpp"
#include "ClipScopeGLES2.hpp"
#include "DisplayDeviceGLES2.hpp"
#include "EffectsGLES2.hpp"
#include "FboGLES2.hpp"
#include "LightObject.hpp"
#include "ModelMatrixScope.hpp"
#include "ScissorGLES2.hpp"
#include "ShadersGLES2.hpp"
#include "StencilScopeGLES2.hpp"
#include "TextureGLES2.hpp"
#include "WindowManager.hpp"

namespace KRE
{
	namespace
	{
		static DisplayDeviceRegistrar<DisplayDeviceGLESv2> glesv2_register("GLESv2");

		CameraPtr& get_default_camera()
		{
			static CameraPtr res = nullptr;
			return res;
		}

		rect& get_current_viewport()
		{
			static rect res;
			return res;
		}

		bool& get_current_depth_enable()
		{
			static bool depth_enable = false;
			return depth_enable;
		}

		bool& get_current_depth_write()
		{
			static bool depth_write = false;
			return depth_write;
		}

		GLenum convert_drawing_mode(DrawMode dm)
		{
			switch(dm) {
				case DrawMode::POINTS:			return GL_POINTS;
				case DrawMode::LINE_STRIP:		return GL_LINE_STRIP;
				case DrawMode::LINE_LOOP:		return GL_LINE_LOOP;
				case DrawMode::LINES:			return GL_LINES;
				case DrawMode::TRIANGLE_STRIP:	return GL_TRIANGLE_STRIP;
				case DrawMode::TRIANGLE_FAN:	return GL_TRIANGLE_FAN;
				case DrawMode::TRIANGLES:		return GL_TRIANGLES;
				case DrawMode::POLYGON:			return GL_TRIANGLE_FAN;
			}
			ASSERT_LOG(false, "Unrecognised value for drawing mode.");
			return GL_NONE;
		}

		GLenum convert_index_type(IndexType it) 
		{
			switch(it) {
				case IndexType::INDEX_NONE:		break;
				case IndexType::INDEX_UCHAR:	return GL_UNSIGNED_BYTE;
				case IndexType::INDEX_USHORT:	return GL_UNSIGNED_SHORT;
				case IndexType::INDEX_ULONG:	return GL_UNSIGNED_INT;
			}
			ASSERT_LOG(false, "Unrecognised value for index type.");
			return GL_NONE;
		}

		static const StencilSettings keep_stencil_settings(true,
			StencilFace::FRONT_AND_BACK, 
			StencilFunc::EQUAL, 
			0xff,
			0x01,
			0x00,
			StencilOperation::KEEP,
			StencilOperation::KEEP,
			StencilOperation::KEEP);
	}

	DisplayDeviceGLESv2::DisplayDeviceGLESv2(WindowPtr wnd)
		: DisplayDevice(wnd),
		  seperate_blend_equations_(false),
		  have_render_to_texture_(false),
		  npot_textures_(false),
		  hardware_uniform_buffers_(false),
		  major_version_(0),
		  minor_version_(0),
		  max_texture_units_(-1)
	{
	}

	DisplayDeviceGLESv2::~DisplayDeviceGLESv2()
	{
	}

	void DisplayDeviceGLESv2::init(int width, int height)
	{
		glViewport(0, 0, width, height);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
		if(extensions != NULL && glGetError() == GL_NONE) {
			std::string exts(extensions);
			for(auto& ext : Util::split(exts, " ")) {
				extensions_.emplace(ext);
			}
		} else {
			LOG_ERROR("Couldn't get the GL extension list.");
		}

		seperate_blend_equations_ = extensions_.find("GL_EXT_blend_equation_separate") != extensions_.end();
		have_render_to_texture_ = extensions_.find("GL_EXT_framebuffer_object") != extensions_.end();
		npot_textures_ = extensions_.find("GL_ARB_texture_non_power_of_two") != extensions_.end();
		hardware_uniform_buffers_ = extensions_.find("GL_ARB_uniform_buffer_object") != extensions_.end();

		GLenum err = GL_NONE;
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units_);
		if((err = glGetError()) != GL_NONE) {
			LOG_ERROR("Failed query for GL_MAX_TEXTURE_IMAGE_UNITS: 0x" << std::hex << err);
		}
		const char* version_str = reinterpret_cast<const char*>(glGetString(GL_VERSION));
		if(version_str != NULL) {
			std::stringstream ss(version_str);
			float vers;
			ss >> vers;
			float integral;
			minor_version_ = static_cast<int>(std::modf(vers, &integral) * 100.0f);
			major_version_ = static_cast<int>(integral);
		} else {
			LOG_ERROR("Unable to query the version string.");
		}
	}

	void DisplayDeviceGLESv2::printDeviceInfo()
	{
		if(minor_version_ == 0 && major_version_ == 0) {
			// fall-back to old glGetStrings method.
			const char* version_str = reinterpret_cast<const char*>(glGetString(GL_VERSION));
			if(version_str != NULL) {
				LOG_INFO("GLESv2 version: " << version_str);
			}
		} else {
			LOG_INFO("GLESv2 version: " << major_version_ << "." << minor_version_);
		}
		
		if(max_texture_units_ > 0) {
			LOG_INFO("Maximum texture units: " << max_texture_units_);
		} else {
			LOG_INFO("Maximum texture units: <<unknown>>" );
		}
		
		const int max_line_width = 101;
		std::vector<std::string> lines;
		for(auto& ext : extensions_) {
			if(lines.empty()) {
				lines.emplace_back(std::string());
			}
			if(ext.size() + lines.back().size() + 1 > max_line_width) {
				lines.emplace_back("\n" + ext);
			} else {
				lines.back() += (lines.back().empty() ? "" : " ") + ext;
			}
		}
		LOG_INFO("GLESv2 Extensions: \n" << std::accumulate(lines.begin(), lines.end(), std::string()));
	}

	int DisplayDeviceGLESv2::queryParameteri(DisplayDeviceParameters param)
	{
		switch (param)
		{
		case DisplayDeviceParameters::MAX_TEXTURE_UNITS:	return max_texture_units_;
		default: break;
		}
		ASSERT_LOG(false, "Invalid Parameter requested: " << static_cast<int>(param));
		return -1;
	}

	void DisplayDeviceGLESv2::clearTextures()
	{
		TextureGLESv2::handleClearTextures();
	}

	void DisplayDeviceGLESv2::clear(ClearFlags clr)
	{
		glClear((clr & ClearFlags::COLOR ? GL_COLOR_BUFFER_BIT : 0) 
			| (clr & ClearFlags::DEPTH ? GL_DEPTH_BUFFER_BIT : 0) 
			| (clr & ClearFlags::STENCIL ? GL_STENCIL_BUFFER_BIT : 0));
	}

	void DisplayDeviceGLESv2::setClearColor(float r, float g, float b, float a) const
	{
		glClearColor(r, g, b, a);
	}

	void DisplayDeviceGLESv2::setClearColor(const Color& color) const
	{
		glClearColor(float(color.r()), float(color.g()), float(color.b()), float(color.a()));
	}

	void DisplayDeviceGLESv2::swap()
	{
		// This is a no-action.
	}

	ShaderProgramPtr DisplayDeviceGLESv2::getDefaultShader()
	{
		return GLESv2::ShaderProgram::defaultSystemShader();
	}

	CameraPtr DisplayDeviceGLESv2::setDefaultCamera(const CameraPtr& cam)
	{
		auto old_cam = get_default_camera();
		get_default_camera() = cam;
		return old_cam;
	}

	CameraPtr DisplayDeviceGLESv2::getDefaultCamera() const
	{
		return get_default_camera();
	}

	void DisplayDeviceGLESv2::render(const Renderable* r) const
	{
		if(!r->isEnabled()) {
			// Renderable item not enabled then early return.
			return;
		}

		StencilScopePtr stencil_scope;
		if(r->hasClipSettings()) {
			ModelManager2D mm(static_cast<int>(r->getPosition().x), static_cast<int>(r->getPosition().y));
			auto clip_shape = r->getStencilMask();
			bool cam_set = false;
			if(clip_shape->getCamera() == nullptr && r->getCamera() != nullptr) {
				cam_set = true;
				clip_shape->setCamera(r->getCamera());
			}
			stencil_scope.reset(new StencilScopeGLESv2(r->getStencilSettings()));
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glDepthMask(GL_FALSE);
			glClear(GL_STENCIL_BUFFER_BIT);
			render(clip_shape.get());
			stencil_scope->applyNewSettings(keep_stencil_settings);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDepthMask(GL_TRUE);

			if(cam_set) {
				clip_shape->setCamera(nullptr);
			}
		}

		auto shader = r->getShader();
		shader->makeActive();

		BlendEquationScopeGLESv2 be_scope(*r);
		BlendModeScopeGLESv2 bm_scope(*r);

		// apply lighting/depth check/depth write here.
		bool use_lighting = r->isLightingStateSet() ? r->useLighting() : false;

		// Set the depth enable.
		if(r->isDepthEnableStateSet()) {
			if(get_current_depth_enable() != r->isDepthEnabled()) {
				if(r->isDepthEnabled()) {
					glEnable(GL_DEPTH_TEST);
				} else {
					glDisable(GL_DEPTH_TEST);
				}
				get_current_depth_enable() = r->isDepthEnabled();
			}
		} else {
			// We assume that depth is disabled if not specified.
			if(get_current_depth_enable() == true) {
				glDisable(GL_DEPTH_TEST);
				get_current_depth_enable() = false;
			}
		}

		glm::mat4 pmat(1.0f);
		glm::mat4 vmat(1.0f);
		if(r->getCamera()) {
			// set camera here.
			pmat = r->getCamera()->getProjectionMat();
			vmat = r->getCamera()->getViewMat();
		} else if(get_default_camera() != nullptr) {
			pmat = get_default_camera()->getProjectionMat();
			vmat = get_default_camera()->getViewMat();
		}

		if(use_lighting) {
			for(auto lp : r->getLights()) {
				/// xxx need to set lights here.
			}
		}
		
		if(r->getRenderTarget()) {
			r->getRenderTarget()->apply();
		}

		if(shader->getPUniform() != ShaderProgram::INVALID_UNIFORM) {
			shader->setUniformValue(shader->getPUniform(), glm::value_ptr(pmat));
		}

		if(shader->getMvUniform() != ShaderProgram::INVALID_UNIFORM) {
			glm::mat4 mvmat = vmat;
			if(is_global_model_matrix_valid() && !r->ignoreGlobalModelMatrix()) {
				mvmat *= get_global_model_matrix() * r->getModelMatrix();
			} else {
				mvmat *= r->getModelMatrix();
			}
			shader->setUniformValue(shader->getMvUniform(), glm::value_ptr(mvmat));
		}

		if(shader->getMvpUniform() != ShaderProgram::INVALID_UNIFORM) {
			glm::mat4 pvmat(1.0f);
			if(is_global_model_matrix_valid() && !r->ignoreGlobalModelMatrix()) {
				pvmat = pmat * vmat * get_global_model_matrix() * r->getModelMatrix();
			} else {
				pvmat = pmat * vmat * r->getModelMatrix();
			}
			shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(pvmat));
		}

		if(shader->getColorUniform() != ShaderProgram::INVALID_UNIFORM) {
			if(r->isColorSet()) {
				shader->setUniformValue(shader->getColorUniform(), r->getColor().asFloatVector());
			} else {
				shader->setUniformValue(shader->getColorUniform(), ColorScope::getCurrentColor().asFloatVector());
			}
		}

		shader->setUniformsForTexture(r->getTexture());

		// XXX we should make this either or with setting the mvp/color uniforms above.
		auto uniform_draw_fn = shader->getUniformDrawFunction();
		if(uniform_draw_fn) {
			uniform_draw_fn(shader);
		}

		// Loop through uniform render variables and set them.
		/*for(auto& urv : r->UniformRenderVariables()) {
			for(auto& rvd : urv->VariableDescritionList()) {
				auto rvdd = std::dynamic_pointer_cast<RenderVariableDeviceData>(rvd->GetDisplayData());
				ASSERT_LOG(rvdd != nullptr, "Unable to cast DeviceData to RenderVariableDeviceData.");
				shader->SetUniformValue(rvdd->GetActiveMapIterator(), urv->Value());
			}
		}*/

		// Need to figure the interaction with shaders.
		/// XXX Need to create a mapping between attributes and the index value below.
		for(auto as : r->getAttributeSet()) {
			if(!as->isEnabled()) {
				continue;
			}
			//ASSERT_LOG(as->getCount() > 0, "No (or negative) number of vertices in attribute set. " << as->getCount());
			if(as->getCount() <= 0) {
				//LOG_WARN("No (or negative) number of vertices in attribute set. " << as->getCount());
				continue;
			}
			GLenum draw_mode = convert_drawing_mode(as->getDrawMode());

			// apply blend, if any, from attribute set.
			BlendEquationScopeGLESv2 be_scope(*as);
			BlendModeScopeGLESv2 bm_scope(*as);

			if(shader->getColorUniform() != ShaderProgram::INVALID_UNIFORM && as->isColorSet()) {
				shader->setUniformValue(shader->getColorUniform(), as->getColor().asFloatVector());
			}

			for(auto& attr : as->getAttributes()) {
				if(attr->isEnabled()) {
					shader->applyAttribute(attr);
				}
			}

			if(as->isInstanced()) {
				if(as->isIndexed()) {
					as->bindIndex();
					// XXX as->GetIndexArray() should be as->GetIndexArray()+as->GetOffset()
					//glDrawElementsInstanced(draw_mode, static_cast<GLsizei>(as->getCount()), convert_index_type(as->getIndexType()), as->getIndexArray(), as->getInstanceCount());
					LOG_ERROR("TODO: emulate glDrawElementsInstanced for GLESv2");
					as->unbindIndex();
				} else {
					//glDrawArraysInstanced(draw_mode, static_cast<GLint>(as->getOffset()), static_cast<GLsizei>(as->getCount()), as->getInstanceCount());
					LOG_ERROR("TODO: emulate glDrawElementsInstanced for GLESv2");
				}
			} else {
				if(as->isIndexed()) {
					as->bindIndex();
					// XXX as->GetIndexArray() should be as->GetIndexArray()+as->GetOffset()
					glDrawElements(draw_mode, static_cast<GLsizei>(as->getCount()), convert_index_type(as->getIndexType()), as->getIndexArray());
					as->unbindIndex();
				} else {
					glDrawArrays(draw_mode, static_cast<GLint>(as->getOffset()), static_cast<GLsizei>(as->getCount()));
				}
			}

			shader->cleanUpAfterDraw();
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		if(r->getRenderTarget()) {
			r->getRenderTarget()->unapply();
		}
	}

	ScissorPtr DisplayDeviceGLESv2::getScissor(const rect& r)
	{
		auto scissor = new ScissorGLESv2(r);
		return ScissorPtr(scissor);
	}

	TexturePtr DisplayDeviceGLESv2::handleCreateTexture(const SurfacePtr& surface, const variant& node)
	{
		std::vector<SurfacePtr> surfaces;
		if(surface != nullptr) {
			surfaces.emplace_back(surface);
		}
		return std::make_shared<TextureGLESv2>(node, surfaces);
	}

	TexturePtr DisplayDeviceGLESv2::handleCreateTexture(const SurfacePtr& surface, TextureType type, int mipmap_levels)
	{
		std::vector<SurfacePtr> surfaces;
		return std::make_shared<TextureGLESv2>(surfaces, type, mipmap_levels);
	}

	TexturePtr DisplayDeviceGLESv2::handleCreateTexture1D(int width, PixelFormat::PF fmt)
	{
		return std::make_shared<TextureGLESv2>(1, width, 0, 0, fmt, TextureType::TEXTURE_1D);
	}

	TexturePtr DisplayDeviceGLESv2::handleCreateTexture2D(int width, int height, PixelFormat::PF fmt)
	{
		// XXX make a static function PixelFormat::isPlanar or such.
		const int count = fmt == PixelFormat::PF::PIXELFORMAT_YV12 ? 3 : 1;
		return std::make_shared<TextureGLESv2>(count, width, height, 0, fmt, TextureType::TEXTURE_2D);
	}
	
	TexturePtr DisplayDeviceGLESv2::handleCreateTexture3D(int width, int height, int depth, PixelFormat::PF fmt)
	{
		return std::make_shared<TextureGLESv2>(1, width, height, depth, fmt, TextureType::TEXTURE_3D);
	}

	TexturePtr DisplayDeviceGLESv2::handleCreateTextureArray(int count, int width, int height, PixelFormat::PF fmt, TextureType type)
	{
		return std::make_shared<TextureGLESv2>(count, width, height, 0, fmt, type);
	}

	TexturePtr DisplayDeviceGLESv2::handleCreateTextureArray(const std::vector<SurfacePtr>& surfaces, const variant& node)
	{
		return std::make_shared<TextureGLESv2>(node, surfaces);
	}

	RenderTargetPtr DisplayDeviceGLESv2::handleCreateRenderTarget(int width, int height, 
			int color_plane_count, 
			bool depth, 
			bool stencil, 
			bool use_multi_sampling, 
			int multi_samples)
	{
		return std::make_shared<FboGLESv2>(width, height, color_plane_count, depth, stencil, use_multi_sampling, multi_samples);
	}

	RenderTargetPtr DisplayDeviceGLESv2::handleCreateRenderTarget(const variant& node)
	{
		return std::make_shared<FboGLESv2>(node);
	}

	AttributeSetPtr DisplayDeviceGLESv2::handleCreateAttributeSet(bool indexed, bool instanced)
	{
		return std::make_shared<AttributeSetGLESv2>(indexed, instanced);
	}

	HardwareAttributePtr DisplayDeviceGLESv2::handleCreateAttribute(AttributeBase* parent)
	{
		return std::make_shared<HardwareAttributeGLESv2>(parent);
	}

	CanvasPtr DisplayDeviceGLESv2::getCanvas()
	{
		return CanvasGLESv2::getInstance();
	}

	ClipScopePtr DisplayDeviceGLESv2::createClipScope(const rect& r)
	{
		return ClipScopePtr(new ClipScopeGLESv2(r));
	}

	StencilScopePtr DisplayDeviceGLESv2::createStencilScope(const StencilSettings& settings)
	{
		auto ss = new StencilScopeGLESv2(settings);
		return StencilScopePtr(ss);
	}

	BlendEquationImplBasePtr DisplayDeviceGLESv2::getBlendEquationImpl()
	{
		return BlendEquationImplBasePtr(new BlendEquationImplGLESv2());
	}

	void DisplayDeviceGLESv2::setViewPort(int x, int y, int width, int height)
	{
		rect new_vp(x, y, width, height);
		if(get_current_viewport() != new_vp && width != 0 && height != 0) {
			get_current_viewport() = new_vp;
			// N.B. glViewPort has the origin in the bottom-left corner. 
			glViewport(x, y, width, height);
		}
	}

	void DisplayDeviceGLESv2::setViewPort(const rect& vp)
	{
		if(get_current_viewport() != vp && vp.w() != 0 && vp.h() != 0) {
			get_current_viewport() = vp;
			// N.B. glViewPort has the origin in the bottom-left corner. 
			glViewport(vp.x(), vp.y(), vp.w(), vp.h());
		}
	}

	const rect& DisplayDeviceGLESv2::getViewPort() const 
	{
		return get_current_viewport();
	}
	
	bool DisplayDeviceGLESv2::doCheckForFeature(DisplayDeviceCapabilties cap)
	{
		bool ret_val = false;
		switch(cap) {
		case DisplayDeviceCapabilties::NPOT_TEXTURES:
			return npot_textures_;
		case DisplayDeviceCapabilties::BLEND_EQUATION_SEPERATE:
			return seperate_blend_equations_;
		case DisplayDeviceCapabilties::RENDER_TO_TEXTURE:
			return have_render_to_texture_;
		case DisplayDeviceCapabilties::SHADERS:
			return true;
		case DisplayDeviceCapabilties::UNIFORM_BUFFERS:
			return hardware_uniform_buffers_;
		default:
			ASSERT_LOG(false, "Unknown value for DisplayDeviceCapabilties given.");
		}
		return ret_val;
	}

	void DisplayDeviceGLESv2::loadShadersFromVariant(const variant& node) 
	{
		GLESv2::ShaderProgram::loadShadersFromVariant(node);
	}

	ShaderProgramPtr DisplayDeviceGLESv2::getShaderProgram(const std::string& name)
	{
		return GLESv2::ShaderProgram::factory(name);
	}

	ShaderProgramPtr DisplayDeviceGLESv2::getShaderProgram(const variant& node)
	{
		return GLESv2::ShaderProgram::factory(node);
	}

	ShaderProgramPtr DisplayDeviceGLESv2::createShader(const std::string& name, 
		const std::vector<ShaderData>& shader_data, 
		const std::vector<ActiveMapping>& uniform_map,
		const std::vector<ActiveMapping>& attribute_map)
	{
		return GLESv2::ShaderProgram::createShader(name, shader_data, uniform_map, attribute_map);
	}

	// XXX Need a way to deal with blits with Camera/Lighting.
	void DisplayDeviceGLESv2::doBlitTexture(const TexturePtr& tex, int dstx, int dsty, int dstw, int dsth, float rotation, int srcx, int srcy, int srcw, int srch)
	{
		ASSERT_LOG(false, "DisplayDevice::doBlitTexture deprecated");
		ASSERT_LOG(!tex, "Texture passed in was not of expected type.");

		const float tx1 = float(srcx) / tex->width();
		const float ty1 = float(srcy) / tex->height();
		const float tx2 = srcw == 0 ? 1.0f : float(srcx + srcw) / tex->width();
		const float ty2 = srch == 0 ? 1.0f : float(srcy + srch) / tex->height();
		const float uv_coords[] = {
			tx1, ty1,
			tx2, ty1,
			tx1, ty2,
			tx2, ty2,
		};

		const float vx1 = float(dstx);
		const float vy1 = float(dsty);
		const float vx2 = float(dstx + dstw);
		const float vy2 = float(dsty + dsth);
		const float vtx_coords[] = {
			vx1, vy1,
			vx2, vy1,
			vx1, vy2,
			vx2, vy2,
		};

		// Apply blend mode from texture if there is any.
		BlendEquationScopeGLESv2 be_scope(*tex);
		BlendModeScopeGLESv2 bm_scope(*tex);

		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3((vx1+vx2)/2.0f,(vy1+vy2)/2.0f,0.0f)) * glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0.0f,0.0f,1.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-(vx1+vy1)/2.0f,-(vy1+vy1)/2.0f,0.0f));
		glm::mat4 mvp = glm::ortho(0.0f, 800.0f, 600.0f, 0.0f) * model;
		auto shader = GLESv2::ShaderProgram::defaultSystemShader();
		shader->makeActive();
		getDefaultShader()->setUniformsForTexture(tex);

		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));
		shader->setUniformValue(shader->getColorUniform(), glm::value_ptr(glm::vec4(1.0f,1.0f,1.0f,1.0f)));
		// XXX the following line are only temporary, obviously.
		//shader->setUniformValue(shader->getUniform("discard"), 0);
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, vtx_coords);
		glEnableVertexAttribArray(shader->getTexcoordAttribute());
		glVertexAttribPointer(shader->getTexcoordAttribute(), 2, GL_FLOAT, GL_FALSE, 0, uv_coords);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glDisableVertexAttribArray(shader->getTexcoordAttribute());
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	namespace
	{
		GLenum convert_read_format(ReadFormat fmt)
		{
			switch(fmt) {
			case ReadFormat::ALPHA:			return GL_ALPHA;
			case ReadFormat::RGB:			return GL_RGB;
			case ReadFormat::RGBA:			return GL_RGBA;
			default: break;
			}

			ASSERT_LOG(false, "Unrecognised ReadFormat: " << static_cast<int>(fmt));
			return GL_NONE;
		}

		GLenum convert_attr_format(AttrFormat type)
		{
			switch(type) {
			case AttrFormat::BOOL:				return GL_BOOL;
			case AttrFormat::FLOAT:				return GL_FLOAT;
			case AttrFormat::FIXED:				return GL_FIXED;
			case AttrFormat::SHORT:				return GL_SHORT;
			case AttrFormat::UNSIGNED_SHORT:	return GL_UNSIGNED_SHORT;
			case AttrFormat::BYTE:				return GL_BYTE;
			case AttrFormat::UNSIGNED_BYTE:		return GL_UNSIGNED_BYTE;
			case AttrFormat::INT:				return GL_INT;
			case AttrFormat::UNSIGNED_INT:		return GL_UNSIGNED_INT;
			default: break;
			}
			ASSERT_LOG(false, "Unrecognised AttrFormat: " << static_cast<int>(type));
			return GL_NONE;
		}
	}

	bool DisplayDeviceGLESv2::handleReadPixels(int x, int y, unsigned width, unsigned height, ReadFormat fmt, AttrFormat type, void* data, int stride)
	{
		ASSERT_LOG(width > 0 && height > 0, "Width or height was negative: " << width << " x " << height);
		LOG_DEBUG("row_pitch: " << stride);
		std::vector<uint8_t> new_data;
		new_data.resize(height * stride);
		//if(pixel_size != 4) {
		//	glPixelStorei(GL_PACK_ALIGNMENT, 1);
		//}
		//glPixelStorei(GL_PACK_ALIGNMENT, 4);
		LOG_DEBUG("before read pixels");
		glReadPixels(x, y, static_cast<int>(width), static_cast<int>(height), convert_read_format(fmt), convert_attr_format(type), &new_data[0]);
		LOG_DEBUG("after read pixels");
		GLenum ok = glGetError();
		if(ok != GL_NONE) {
			LOG_ERROR("Unable to read pixels error was: " << ok);
			return false;
		}
		LOG_DEBUG("before copy");
		uint8_t* cp_data = reinterpret_cast<uint8_t*>(data);
		
		for(auto it = new_data.begin() + (height-1)*stride; it != new_data.begin(); it -= stride) {
			std::copy(it, it + stride, cp_data);
			cp_data += stride;
		}
		LOG_DEBUG("after copy");
		return true;
	}

	EffectPtr DisplayDeviceGLESv2::createEffect(const variant& node)
	{
		ASSERT_LOG(node.has_key("type") && node["type"].is_string(), "Effects must have 'type' attribute as string: " << node.to_debug_string());
		const std::string& type = node["type"].as_string();
		if(type == "stipple") {
			return std::make_shared<GLESv2::StippleEffect>(node);
		}
		// XXX Add more effects here as and if needed.
		return EffectPtr();
	}

	ShaderProgramPtr DisplayDeviceGLESv2::createGaussianShader(int radius) 
	{
		return GLESv2::ShaderProgram::createGaussianShader(radius);
	}
}

