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

#pragma comment(lib, "opengl32")
#pragma comment(lib, "glu32")
#pragma comment(lib, "glew32")

#include <numeric>

#include <GL/glew.h>

#include "asserts.hpp"
#include "AttributeSetOGL.hpp"
#include "BlendOGL.hpp"
#include "CameraObject.hpp"
#include "ColorScope.hpp"
#include "CanvasOGL.hpp"
#include "ClipScopeOGL.hpp"
#include "DisplayDeviceOGL.hpp"
#include "EffectsOGL.hpp"
#include "FboOGL.hpp"
#include "LightObject.hpp"
#include "ScissorOGL.hpp"
#include "ShadersOGL.hpp"
#include "StencilScopeOGL.hpp"
#include "TextureOGL.hpp"

namespace KRE
{
	namespace
	{
		static DisplayDeviceRegistrar<DisplayDeviceOpenGL> ogl_register("opengl");

		CameraPtr& get_default_camera()
		{
			static CameraPtr res = nullptr;
			return res;
		}
	}

	namespace 
	{
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
				case DrawMode::QUAD_STRIP:		return GL_QUAD_STRIP;
				case DrawMode::QUADS:			return GL_QUADS;
				case DrawMode::POLYGON:			return GL_POLYGON;
			}
			ASSERT_LOG(false, "Unrecognised value for drawing mode.");
			return GL_NONE;
		}

		GLenum convert_index_type(IndexType it) 
		{
			switch(it) {
				case IndexType::INDEX_NONE:		break;
				case IndexType::INDEX_UCHAR:		return GL_UNSIGNED_BYTE;
				case IndexType::INDEX_USHORT:		return GL_UNSIGNED_SHORT;
				case IndexType::INDEX_ULONG:		return GL_UNSIGNED_INT;
			}
			ASSERT_LOG(false, "Unrecognised value for index type.");
			return GL_NONE;
		}
	}

	DisplayDeviceOpenGL::DisplayDeviceOpenGL()
		: seperate_blend_equations_(false),
		  have_render_to_texture_(false),
		  npot_textures_(false),
		  hardware_uniform_buffers_(false),
		  major_version_(0),
		  minor_version_(0),
		  max_texture_units_(-1)
	{
	}

	DisplayDeviceOpenGL::~DisplayDeviceOpenGL()
	{
	}

	void DisplayDeviceOpenGL::init(size_t width, size_t height)
	{
		GLenum err = glewInit();
		ASSERT_LOG(err == GLEW_OK, "Could not initialise GLEW: " << glewGetErrorString(err));

		glViewport(0, 0, width, height);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		int extension_count = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &extension_count);

		if(major_version_ >= 3) {
			// Get extensions
			for(int n = 0; n != extension_count; ++n) {
				std::string ext(reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, n)));
				extensions_.emplace(ext);
			}
		} else {
			std::string exts(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));
			if(glGetError() == GL_NONE) {
				for(auto& ext : Util::split(exts, " ")) {
					extensions_.emplace(ext);
				}
			} else {
				LOG_ERROR("Couldn't get the GL extension list. Extension count=" << extension_count);
			}
		}

		seperate_blend_equations_ = extensions_.find("GL_EXT_blend_equation_separate") != extensions_.end();
		have_render_to_texture_ = extensions_.find("GL_EXT_framebuffer_object") != extensions_.end();
		npot_textures_ = extensions_.find("GL_ARB_texture_non_power_of_two") != extensions_.end();
		hardware_uniform_buffers_ = extensions_.find("GL_ARB_uniform_buffer_object") != extensions_.end();

		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units_);
		if((err = glGetError()) != GL_NONE) {
			LOG_ERROR("Failed query for GL_MAX_TEXTURE_IMAGE_UNITS: 0x" << std::hex << err);
		}
		glGetIntegerv(GL_MINOR_VERSION, &minor_version_);
		glGetIntegerv(GL_MAJOR_VERSION, &major_version_);
		if((err = glGetError()) != GL_NONE) {
			const char* version_str = reinterpret_cast<const char*>(glGetString(GL_VERSION));
			std::stringstream ss(version_str);
			float vers;
			ss >> vers;
			float integral;
			minor_version_ = static_cast<int>(std::modf(vers, &integral) * 100.0f);
			major_version_ = static_cast<int>(integral);
		}
	}

	void DisplayDeviceOpenGL::printDeviceInfo()
	{
		if(minor_version_ == 0 && major_version_ == 0) {
			// fall-back to old glGetStrings method.
			const char* version_str = reinterpret_cast<const char*>(glGetString(GL_VERSION));
			LOG_INFO("OpenGL version: " << version_str);
		} else {
			LOG_INFO("OpenGL version: " << major_version_ << "." << minor_version_);
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
		LOG_INFO("OpenGL Extensions: \n" << std::accumulate(lines.begin(), lines.end(), std::string()));
	}

	int DisplayDeviceOpenGL::queryParameteri(DisplayDeviceParameters param)
	{
		switch (param)
		{
		case DisplayDeviceParameters::MAX_TEXTURE_UNITS:	return max_texture_units_;
		default: break;
		}
		ASSERT_LOG(false, "Invalid Parameter requested: " << static_cast<int>(param));
		return -1;
	}

	void DisplayDeviceOpenGL::clearTextures()
	{
		OpenGLTexture::handleClearTextures();
	}

	void DisplayDeviceOpenGL::clear(ClearFlags clr)
	{
		glClear(clr & ClearFlags::COLOR ? GL_COLOR_BUFFER_BIT : 0 
			| clr & ClearFlags::DEPTH ? GL_DEPTH_BUFFER_BIT : 0 
			| clr & ClearFlags::STENCIL ? GL_STENCIL_BUFFER_BIT : 0);
	}

	void DisplayDeviceOpenGL::setClearColor(float r, float g, float b, float a) const
	{
		glClearColor(r, g, b, a);
	}

	void DisplayDeviceOpenGL::setClearColor(const Color& color) const
	{
		glClearColor(float(color.r()), float(color.g()), float(color.b()), float(color.a()));
	}

	void DisplayDeviceOpenGL::swap()
	{
		// This is a no-action.
	}

	ShaderProgramPtr DisplayDeviceOpenGL::getDefaultShader()
	{
		return OpenGL::ShaderProgram::defaultSystemShader();
	}

	void DisplayDeviceOpenGL::setDefaultCamera(const CameraPtr& cam)
	{
		get_default_camera() = cam;
	}

	void DisplayDeviceOpenGL::render(const Renderable* r) const
	{
		auto shader = r->getShader();
		shader->makeActive();

		BlendEquationScopeOGL be_scope(*r);
		BlendModeScopeOGL bm_scope(*r);

		// apply lighting/depth check/depth write here.
		bool use_lighting = false;

		glm::mat4 pvmat(1.0f);
		if(r->getCamera()) {
			// set camera here.
			pvmat = r->getCamera()->getProjectionMat() * r->getCamera()->getViewMat();
		} else if(get_default_camera() != nullptr) {
			pvmat = get_default_camera()->getProjectionMat() * get_default_camera()->getViewMat();
		}

		if(use_lighting) {
			for(auto lp : r->getLights()) {
				/// xxx need to set lights here.
			}
		}
		
		if(r->getRenderTarget()) {
			r->getRenderTarget()->apply();
		}

		if(shader->getMvpUniform() != ShaderProgram::INALID_UNIFORM) {
			pvmat *= r->getModelMatrix();
			shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(pvmat));
		}

		if(shader->getColorUniform() != ShaderProgram::INALID_UNIFORM) {
			if(r->isColorSet()) {
				shader->setUniformValue(shader->getColorUniform(), r->getColor().asFloatVector());
			} else {
				shader->setUniformValue(shader->getColorUniform(), ColorScope::getCurrentColor().asFloatVector());
			}
		}

		shader->setUniformsForTexture(r->getTexture());

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
			//ASSERT_LOG(as->getCount() > 0, "No (or negative) number of vertices in attribute set. " << as->getCount());
			if(as->getCount() <= 0) {
				//LOG_WARN("No (or negative) number of vertices in attribute set. " << as->getCount());
				continue;
			}
			GLenum draw_mode = convert_drawing_mode(as->getDrawMode());

			// apply blend, if any, from attribute set.
			BlendEquationScopeOGL be_scope(*as);
			BlendModeScopeOGL bm_scope(*as);

			if(shader->getColorUniform() != ShaderProgram::INALID_UNIFORM && as->isColorSet()) {
				shader->setUniformValue(shader->getColorUniform(), as->getColor().asFloatVector());
			}

			for(auto& attr : as->getAttributes()) {
				shader->applyAttribute(attr);
			}

			if(as->isInstanced()) {
				if(as->isIndexed()) {
					as->bindIndex();
					// XXX as->GetIndexArray() should be as->GetIndexArray()+as->GetOffset()
					glDrawElementsInstanced(draw_mode, as->getCount(), convert_index_type(as->getIndexType()), as->getIndexArray(), as->getInstanceCount());
					as->unbindIndex();
				} else {
					glDrawArraysInstanced(draw_mode, as->getOffset(), as->getCount(), as->getInstanceCount());
				}
			} else {
				if(as->isIndexed()) {
					as->bindIndex();
					// XXX as->GetIndexArray() should be as->GetIndexArray()+as->GetOffset()
					glDrawElements(draw_mode, as->getCount(), convert_index_type(as->getIndexType()), as->getIndexArray());
					as->unbindIndex();
				} else {
					glDrawArrays(draw_mode, as->getOffset(), as->getCount());
				}
			}

			shader->cleanUpAfterDraw();
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		if(r->getRenderTarget()) {
			r->getRenderTarget()->unapply();
		}
	}

	ScissorPtr DisplayDeviceOpenGL::getScissor(const rect& r)
	{
		auto scissor = new ScissorOGL(r);
		return ScissorPtr(scissor);
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture(const SurfacePtr& surface, const variant& node)
	{
		std::vector<SurfacePtr> surfaces;
		if(surface != nullptr) {
			surfaces.emplace_back(surface);
		}
		return std::make_shared<OpenGLTexture>(node, surfaces);
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture(const SurfacePtr& surface, TextureType type, int mipmap_levels)
	{
		std::vector<SurfacePtr> surfaces(1, surface);
		return std::make_shared<OpenGLTexture>(surfaces, type, mipmap_levels);
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture1D(int width, PixelFormat::PF fmt)
	{
		return std::make_shared<OpenGLTexture>(1, width, 0, 0, fmt, TextureType::TEXTURE_1D);
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture2D(int width, int height, PixelFormat::PF fmt)
	{
		return std::make_shared<OpenGLTexture>(1, width, height, 0, fmt, TextureType::TEXTURE_2D);
	}
	
	TexturePtr DisplayDeviceOpenGL::handleCreateTexture3D(int width, int height, int depth, PixelFormat::PF fmt)
	{
		return std::make_shared<OpenGLTexture>(1, width, height, depth, fmt, TextureType::TEXTURE_3D);
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTextureArray(int count, int width, int height, PixelFormat::PF fmt, TextureType type)
	{
		return std::make_shared<OpenGLTexture>(count, width, height, 0, fmt, type);
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTextureArray(const std::vector<SurfacePtr>& surfaces, const variant& node)
	{
		return std::make_shared<OpenGLTexture>(node, surfaces);
	}

	RenderTargetPtr DisplayDeviceOpenGL::handleCreateRenderTarget(size_t width, size_t height, 
			size_t color_plane_count, 
			bool depth, 
			bool stencil, 
			bool use_multi_sampling, 
			size_t multi_samples)
	{
		return RenderTargetPtr(new FboOpenGL(width, height, color_plane_count, depth, stencil, use_multi_sampling, multi_samples));
	}

	RenderTargetPtr DisplayDeviceOpenGL::handleCreateRenderTarget(const variant& node)
	{
		return RenderTargetPtr(new FboOpenGL(node));
	}

	AttributeSetPtr DisplayDeviceOpenGL::handleCreateAttributeSet(bool indexed, bool instanced)
	{
		return AttributeSetPtr(new AttributeSetOGL(indexed, instanced));
	}

	HardwareAttributePtr DisplayDeviceOpenGL::handleCreateAttribute(AttributeBase* parent)
	{
		return HardwareAttributePtr(new HardwareAttributeOGL(parent));
	}

	CanvasPtr DisplayDeviceOpenGL::getCanvas()
	{
		return CanvasOGL::getInstance();
	}

	ClipScopePtr DisplayDeviceOpenGL::createClipScope(const rect& r)
	{
		return ClipScopePtr(new ClipScopeOGL(r));
	}

	StencilScopePtr DisplayDeviceOpenGL::createStencilScope(const StencilSettings& settings)
	{
		auto ss = new StencilScopeOGL(settings);
		return StencilScopePtr(ss);
	}

	BlendEquationImplBasePtr DisplayDeviceOpenGL::getBlendEquationImpl()
	{
		return BlendEquationImplBasePtr(new BlendEquationImplOGL());
	}

	void DisplayDeviceOpenGL::setViewPort(int x, int y, unsigned width, unsigned height)
	{
		glViewport(x, y, width, height);
	}
	
	bool DisplayDeviceOpenGL::doCheckForFeature(DisplayDeviceCapabilties cap)
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

	void DisplayDeviceOpenGL::loadShadersFromVariant(const variant& node) 
	{
		OpenGL::ShaderProgram::loadShadersFromVariant(node);
	}

	ShaderProgramPtr DisplayDeviceOpenGL::getShaderProgram(const std::string& name)
	{
		return OpenGL::ShaderProgram::factory(name);
	}

	ShaderProgramPtr DisplayDeviceOpenGL::getShaderProgram(const variant& node)
	{
		return OpenGL::ShaderProgram::factory(node);
	}

	// XXX Need a way to deal with blits with Camera/Lighting.
	void DisplayDeviceOpenGL::doBlitTexture(const TexturePtr& tex, int dstx, int dsty, int dstw, int dsth, float rotation, int srcx, int srcy, int srcw, int srch)
	{
		ASSERT_LOG(tex != nullptr, "Texture passed in was not of expected type.");

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
		BlendEquationScopeOGL be_scope(*tex);
		BlendModeScopeOGL bm_scope(*tex);

		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3((vx1+vx2)/2.0f,(vy1+vy2)/2.0f,0.0f)) * glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0.0f,0.0f,1.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-(vx1+vy1)/2.0f,-(vy1+vy1)/2.0f,0.0f));
		glm::mat4 mvp = glm::ortho(0.0f, 800.0f, 600.0f, 0.0f) * model;
		auto shader = OpenGL::ShaderProgram::defaultSystemShader();
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
			switch(fmt)
			{
			case ReadFormat::DEPTH:			return GL_DEPTH_COMPONENT;
			case ReadFormat::STENCIL:		return GL_STENCIL_INDEX;
			case ReadFormat::DEPTH_STENCIL:	return GL_DEPTH_STENCIL;
			case ReadFormat::RED:			return GL_RED;
			case ReadFormat::GREEN:			return GL_GREEN;
			case ReadFormat::BLUE:			return GL_BLUE;
			case ReadFormat::RG:			return GL_RG;
			case ReadFormat::RGB:			return GL_RGB;
			case ReadFormat::BGR:			return GL_BGR;
			case ReadFormat::RGBA:			return GL_RGBA;
			case ReadFormat::BGRA:			return GL_BGRA;
			case ReadFormat::RED_INT:		return GL_RED_INTEGER;
			case ReadFormat::GREEN_INT:		return GL_GREEN_INTEGER;
			case ReadFormat::BLUE_INT:		return GL_BLUE_INTEGER;
			case ReadFormat::RG_INT:		return GL_RG_INTEGER;
			case ReadFormat::RGB_INT:		return GL_RGB_INTEGER;
			case ReadFormat::BGR_INT:		return GL_BGR_INTEGER;
			case ReadFormat::RGBA_INT:		return GL_RGBA_INTEGER;
			case ReadFormat::BGRA_INT:		return GL_BGRA_INTEGER;
			default: break;
			}

			ASSERT_LOG(false, "Unrecognised ReadFormat: " << static_cast<int>(fmt));
			return GL_NONE;
		}

		GLenum convert_attr_format(AttrFormat type)
		{
			switch (type)
			{
			case AttrFormat::BOOL:				return GL_BOOL;
			case AttrFormat::HALF_FLOAT:		return GL_HALF_FLOAT;
			case AttrFormat::FLOAT:				return GL_FLOAT;
			case AttrFormat::DOUBLE:			return GL_DOUBLE;
			case AttrFormat::FIXED:				return GL_FIXED;
			case AttrFormat::SHORT:				return GL_SHORT;
			case AttrFormat::UNSIGNED_SHORT:	return GL_UNSIGNED_SHORT;
			case AttrFormat::BYTE:				return GL_BYTE;
			case AttrFormat::UNSIGNED_BYTE:		return GL_UNSIGNED_BYTE;
			case AttrFormat::INT:				return GL_INT;
			case AttrFormat::UNSIGNED_INT:		return GL_UNSIGNED_INT;
			case AttrFormat::INT_2_10_10_10_REV:			return GL_INT_2_10_10_10_REV;
			case AttrFormat::UNSIGNED_INT_2_10_10_10_REV:	return GL_UNSIGNED_INT_2_10_10_10_REV;
			case AttrFormat::UNSIGNED_INT_10F_11F_11F_REV:	return GL_UNSIGNED_INT_10F_11F_11F_REV;
			default: break;
			}
			ASSERT_LOG(false, "Unrecognised AttrFormat: " << static_cast<int>(type));
			return GL_NONE;
		}
	}

	bool DisplayDeviceOpenGL::handleReadPixels(int x, int y, unsigned width, unsigned height, ReadFormat fmt, AttrFormat type, void* data)
	{
		glReadPixels(x, y, width, height, convert_read_format(fmt), convert_attr_format(type), data);
		GLenum ok = glGetError();
		if(ok != GL_NONE) {
			LOG_ERROR("Unable to read pixels error was: " << ok);
			return false;
		}
		return true;
	}

	EffectPtr DisplayDeviceOpenGL::createEffect(const variant& node)
	{
		ASSERT_LOG(node.has_key("type") && node["type"].is_string(), "Effects must have 'type' attribute as string: " << node.to_debug_string());
		const std::string& type = node["type"].as_string();
		if(type == "stipple") {
			return std::make_shared<OpenGL::StippleEffect>(node);
		}
		// XXX Add more effects here as and if needed.
		return EffectPtr();
	}
}

