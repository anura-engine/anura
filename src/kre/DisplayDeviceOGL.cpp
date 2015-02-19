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
	}

	class RenderVariableDeviceData : public DisplayDeviceData
	{
	public:
		RenderVariableDeviceData() {
		}
		~RenderVariableDeviceData() {
		}
		RenderVariableDeviceData(const OpenGL::ConstActivesMapIterator& it)
			: active_iterator_(it) {
		}

		void setActiveMapIterator(const OpenGL::ConstActivesMapIterator& it) {
			active_iterator_ = it;
		}
		OpenGL::ConstActivesMapIterator getActiveMapIterator() const { return active_iterator_; }
	private:
		OpenGL::ConstActivesMapIterator active_iterator_;
	};

	namespace 
	{
		GLenum convert_render_variable_type(AttrFormat type)
		{
			switch(type) {
				case AttrFormat::BOOL:							return GL_BYTE;
				case AttrFormat::HALF_FLOAT:					return GL_HALF_FLOAT;
				case AttrFormat::FLOAT:							return GL_FLOAT;
				case AttrFormat::DOUBLE:						return GL_DOUBLE;
				case AttrFormat::FIXED:							return GL_FIXED;
				case AttrFormat::SHORT:							return GL_SHORT;
				case AttrFormat::UNSIGNED_SHORT:				return GL_UNSIGNED_SHORT;
				case AttrFormat::BYTE:							return GL_BYTE;
				case AttrFormat::UNSIGNED_BYTE:					return GL_UNSIGNED_BYTE;
				case AttrFormat::INT:							return GL_INT;
				case AttrFormat::UNSIGNED_INT:					return GL_UNSIGNED_INT;
				case AttrFormat::INT_2_10_10_10_REV:			return GL_INT_2_10_10_10_REV;
				case AttrFormat::UNSIGNED_INT_2_10_10_10_REV:	return GL_UNSIGNED_INT_2_10_10_10_REV;
				case AttrFormat::UNSIGNED_INT_10F_11F_11F_REV:	return GL_UNSIGNED_INT_10F_11F_11F_REV;
			}
			ASSERT_LOG(false, "Unrecognised value for variable type.");
			return GL_NONE;
		}

		GLenum convert_drawing_mode(DrawMode dm)
		{
			switch(dm) {
				case DrawMode::POINTS:			return GL_POINTS;
				case DrawMode::LINE_STRIP:		return GL_LINE_STRIP;
				case DrawMode::LINE_LOOP:			return GL_LINE_LOOP;
				case DrawMode::LINES:				return GL_LINES;
				case DrawMode::TRIANGLE_STRIP:	return GL_TRIANGLE_STRIP;
				case DrawMode::TRIANGLE_FAN:		return GL_TRIANGLE_FAN;
				case DrawMode::TRIANGLES:			return GL_TRIANGLES;
				case DrawMode::QUAD_STRIP:		return GL_QUAD_STRIP;
				case DrawMode::QUADS:				return GL_QUADS;
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
		  major_version_(0),
		  minor_version_(0)
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
				LOG_INFO("Extensions: " << ext);
			}
		} else {
			std::string exts(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));
			if(glGetError() == GL_NONE) {
				for(auto& ext : Util::split(exts, " ")) {
					extensions_.emplace(ext);
					LOG_INFO("Extensions: " << ext);
				}
			} else {
				LOG_ERROR("Couldn't get the GL extension list. Extension count=" << extension_count);
			}
		}

		seperate_blend_equations_ = extensions_.find("EXT_blend_equation_separate") != extensions_.end();
		have_render_to_texture_ = extensions_.find("EXT_framebuffer_object") != extensions_.end();
		npot_textures_ = extensions_.find("ARB_texture_non_power_of_two") != extensions_.end();
	}

	void DisplayDeviceOpenGL::printDeviceInfo()
	{
		glGetIntegerv(GL_MINOR_VERSION, &minor_version_);
		glGetIntegerv(GL_MAJOR_VERSION, &major_version_);
		if(glGetError() != GL_NONE) {
			// fall-back to old glGetStrings method.
			const char* version_str = reinterpret_cast<const char*>(glGetString(GL_VERSION));
			std::cerr << "OpenGL version: " << version_str << std::endl;
		} else {
			std::cerr << "OpenGL version: " << major_version_ << "." << minor_version_ << std::endl;
		}
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

	DisplayDeviceDataPtr DisplayDeviceOpenGL::createDisplayDeviceData(const DisplayDeviceDef& def)
	{
		DisplayDeviceDataPtr dd = std::make_shared<DisplayDeviceData>();
		OpenGL::ShaderProgramPtr shader;
		bool use_default_shader = true;
		for(auto& hints : def.getHints()) {
			if(hints.first == "shader") {
				// Need to have retrieved more shader data here.
				shader = OpenGL::ShaderProgram::factory(hints.second[0]);
				if(shader != nullptr) {
					dd->setShader(shader);
				}
			}
			// ...
			// add more hints here if needed.
		}
		// If there is no shader hint, we will assume the default system shader.
		if(shader == nullptr) {
			shader = OpenGL::ShaderProgram::defaultSystemShader();
			dd->setShader(shader);
		}
		
		// XXX Set uniforms from block here.

		for(auto& as : def.getAttributeSet()) {
			for(auto& attr : as->getAttributes()) {
				for(auto& desc : attr->getAttrDesc()) {
					auto ddp = DisplayDeviceDataPtr(new RenderVariableDeviceData(shader->getAttributeIterator(desc.getAttrName())));
					desc.setDisplayData(ddp);
				}
			}
		}

		return dd;
	}

	void DisplayDeviceOpenGL::render(const Renderable* r) const
	{
		auto dd = r->getDisplayData();
		// XXX work out removing this dynamic_pointer_cast.
		auto shader = std::dynamic_pointer_cast<OpenGL::ShaderProgram>(dd->getShader());
		ASSERT_LOG(shader != nullptr, "Failed to cast shader to the type required(OpenGL::ShaderProgram).");
		shader->makeActive();

		BlendEquationScopeOGL be_scope(*r);
		BlendModeScopeOGL bm_scope(*r);

		// apply lighting/depth check/depth write here.
		bool use_lighting = false;

		glm::mat4 pvmat(1.0f);
		if(r->getCamera()) {
			// set camera here.
			pvmat = r->getCamera()->getProjectionMat() * r->getCamera()->getViewMat();
		}

		if(use_lighting) {
			for(auto lp : r->getLights()) {
				/// xxx need to set lights here.
			}
		}
		
		if(r->getTexture()) {
			r->getTexture()->bind();
		}

		if(r->getRenderTarget()) {
			r->getRenderTarget()->apply();
		}

		if(shader->getMvpUniform() != shader->uniformsIteratorEnd()) {
			pvmat *= r->getModelMatrix();
			shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(pvmat));
		}

		if(shader->getColorUniform() != shader->uniformsIteratorEnd()) {
			if(r->isColorSet()) {
				shader->setUniformValue(shader->getColorUniform(), r->getColor().asFloatVector());
			} else {
				shader->setUniformValue(shader->getColorUniform(), ColorScope::getCurrentColor().asFloatVector());
			}
		}

		// XXX The material may need to set more texture uniforms for multi-texture -- need to do that here.
		// Or maybe it should be done through the uniform block and override this somehow.
		if(shader->getTexMapUniform() != shader->uniformsIteratorEnd()) {
			shader->setUniformValue(shader->getTexMapUniform(), 0);
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
			GLenum draw_mode = convert_drawing_mode(as->getDrawMode());
			std::vector<GLuint> enabled_attribs;

			// apply blend, if any, from attribute set.
			BlendEquationScopeOGL be_scope(*as);
			BlendModeScopeOGL bm_scope(*as);

			if(shader->getColorUniform() != shader->uniformsIteratorEnd() && as->isColorSet()) {
				shader->setUniformValue(shader->getColorUniform(), as->getColor().asFloatVector());
			}

			for(auto& attr : as->getAttributes()) {
				auto attr_hw = attr->getDeviceBufferData();
				//auto attrogl = std::dynamic_pointer_cast<AttributeOGL>(attr);
				attr_hw->bind();
				for(auto& attrdesc : attr->getAttrDesc()) {
					auto ddp = std::dynamic_pointer_cast<RenderVariableDeviceData>(attrdesc.getDisplayData());
					ASSERT_LOG(ddp != nullptr, "Converting attribute device data was nullptr.");
					glEnableVertexAttribArray(ddp->getActiveMapIterator()->second.location);
					
					glVertexAttribPointer(ddp->getActiveMapIterator()->second.location, 
						attrdesc.getNumElements(), 
						convert_render_variable_type(attrdesc.getVarType()), 
						attrdesc.normalise(), 
						attrdesc.getStride(), 
						reinterpret_cast<const GLvoid*>(attr_hw->value() + attr->getOffset() + attrdesc.getOffset()));
					enabled_attribs.emplace_back(ddp->getActiveMapIterator()->second.location);
				}
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

			for(auto attrib : enabled_attribs) {
				glDisableVertexAttribArray(attrib);
			}
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

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture(const variant& node) 
	{
		return std::make_shared<OpenGLTexture>(node, std::vector<SurfacePtr>());
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

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture1D(unsigned width, PixelFormat::PF fmt)
	{
		return std::make_shared<OpenGLTexture>(1, width, 0, fmt, TextureType::TEXTURE_1D);
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture2D(unsigned width, unsigned height, PixelFormat::PF fmt, TextureType type)
	{
		return std::make_shared<OpenGLTexture>(1, width, height, fmt, TextureType::TEXTURE_2D);
	}
	
	TexturePtr DisplayDeviceOpenGL::handleCreateTexture3D(unsigned width, unsigned height, unsigned depth, PixelFormat::PF fmt)
	{
		return std::make_shared<OpenGLTexture>(1, width, height, fmt, TextureType::TEXTURE_3D, depth);
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture(const std::string& filename, TextureType type, int mipmap_levels)
	{
		auto surface = Surface::create(filename);		
		std::vector<SurfacePtr> surfaces(1, surface);
		return std::make_shared<OpenGLTexture>(surfaces, type, mipmap_levels);
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture(const SurfacePtr& surface, const SurfacePtr& palette)
	{
		return std::make_shared<OpenGLTexture>(surface, palette);
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture2D(int count, int width, int height, PixelFormat::PF fmt)
	{
		return std::make_shared<OpenGLTexture>(count, width, height, fmt, TextureType::TEXTURE_2D);
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture2D(const std::vector<std::string>& filenames, const variant& node)
	{
		std::vector<SurfacePtr> surfaces;
		surfaces.reserve(filenames.size());
		for(auto& fn : filenames) {
			surfaces.emplace_back(Surface::create(fn));
		}
		return std::make_shared<OpenGLTexture>(node, surfaces);
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture2D(const std::vector<SurfacePtr>& surfaces, bool cache)
	{
		return std::make_shared<OpenGLTexture>(surfaces, TextureType::TEXTURE_2D);
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
		default:
			ASSERT_LOG(false, "Unknown value for DisplayDeviceCapabilties given.");
		}
		return ret_val;
	}

	void DisplayDeviceOpenGL::loadShadersFromFile(const variant& node) 
	{
		OpenGL::ShaderProgram::loadFromFile(node);
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
		auto texture = std::dynamic_pointer_cast<OpenGLTexture>(tex);
		ASSERT_LOG(texture != nullptr, "Texture passed in was not of expected type.");

		const float tx1 = float(srcx) / texture->width();
		const float ty1 = float(srcy) / texture->height();
		const float tx2 = srcw == 0 ? 1.0f : float(srcx + srcw) / texture->width();
		const float ty2 = srch == 0 ? 1.0f : float(srcy + srch) / texture->height();
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
		texture->bind();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));
		shader->setUniformValue(shader->getColorUniform(), glm::value_ptr(glm::vec4(1.0f,1.0f,1.0f,1.0f)));
		shader->setUniformValue(shader->getTexMapUniform(), 0);
		// XXX the following line are only temporary, obviously.
		shader->setUniformValue(shader->getUniformIterator("discard"), 0);
		glEnableVertexAttribArray(shader->getVertexAttribute()->second.location);
		glVertexAttribPointer(shader->getVertexAttribute()->second.location, 2, GL_FLOAT, GL_FALSE, 0, vtx_coords);
		glEnableVertexAttribArray(shader->getTexcoordAttribute()->second.location);
		glVertexAttribPointer(shader->getTexcoordAttribute()->second.location, 2, GL_FLOAT, GL_FALSE, 0, uv_coords);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glDisableVertexAttribArray(shader->getTexcoordAttribute()->second.location);
		glDisableVertexAttribArray(shader->getVertexAttribute()->second.location);
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
