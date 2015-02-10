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
#include "AttributeSetOpenGL.hpp"
#include "BlendOGL.hpp"
#include "CameraObject.hpp"
#include "CanvasOGL.hpp"
#include "ClipScopeOGL.hpp"
#include "DisplayDeviceOpenGL.hpp"
#include "EffectsOpenGL.hpp"
#include "FboOpenGL.hpp"
#include "LightObject.hpp"
#include "MaterialOpenGL.hpp"
#include "ScissorOGL.hpp"
#include "ShadersOpenGL.hpp"
#include "StencilScopeOGL.hpp"
#include "TextureOpenGL.hpp"

namespace KRE
{
	namespace
	{
		static DisplayDeviceRegistrar<DisplayDeviceOpenGL> ogl_register("opengl");
	}

	// These basically get attached to renderable's and we can retreive them during the
	// rendering process. So we store stuff like shader information and shader variables.
	class OpenGLDeviceData : public DisplayDeviceData
	{
	public:
		OpenGLDeviceData() {
		}
		~OpenGLDeviceData() { 
		}
		void setShader(OpenGL::ShaderProgramPtr shader) {
			shader_ = shader;
		}
		OpenGL::ShaderProgramPtr getShader() const { return shader_; }
	private:
		OpenGL::ShaderProgramPtr shader_;
		OpenGLDeviceData(const OpenGLDeviceData&);
	};

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

		// Register with the render variable factory so we can create 
		// VBO backed render variables.
	}

	void DisplayDeviceOpenGL::printDeviceInfo()
	{
		GLint minor_version;
		GLint major_version;
		glGetIntegerv(GL_MINOR_VERSION, &minor_version);
		glGetIntegerv(GL_MAJOR_VERSION, &major_version);
		if(glGetError() != GL_NONE) {
			// fall-back to old glGetStrings method.
			const char* version_str = reinterpret_cast<const char*>(glGetString(GL_VERSION));
			std::cerr << "OpenGL version: " << version_str << std::endl;
		} else {
			std::cerr << "OpenGL version: " << major_version << "." << minor_version << std::endl;
		}
	}

	void DisplayDeviceOpenGL::clear(ClearFlags clr)
	{
		glClear(clr & ClearFlags::COLOR ? GL_COLOR_BUFFER_BIT : 0 
			| clr & ClearFlags::DEPTH ? GL_DEPTH_BUFFER_BIT : 0 
			| clr & ClearFlags::STENCIL ? GL_STENCIL_BUFFER_BIT : 0);
	}

	void DisplayDeviceOpenGL::setClearColor(float r, float g, float b, float a)
	{
		glClearColor(r, g, b, a);
	}

	void DisplayDeviceOpenGL::setClearColor(const Color& color)
	{
		glClearColor(float(color.r()), float(color.g()), float(color.b()), float(color.a()));
	}

	void DisplayDeviceOpenGL::swap()
	{
		// This is a no-action.
	}

	DisplayDeviceDataPtr DisplayDeviceOpenGL::createDisplayDeviceData(const DisplayDeviceDef& def)
	{
		OpenGLDeviceData* dd = new OpenGLDeviceData();
		bool use_default_shader = true;
		for(auto& hints : def.getHints()) {
			if(hints.first == "shader") {
				// Need to have retrieved more shader data here.
				dd->setShader(OpenGL::ShaderProgram::factory(hints.second[0]));
				use_default_shader = false;
			}
			// ...
			// add more hints here if needed.
		}
		// If there is no shader hint, we will assume the default system shader.
		if(use_default_shader) {
			dd->setShader(OpenGL::ShaderProgram::defaultSystemShader());
		}
		
		// XXX Set uniforms from block here.

		for(auto& as : def.getAttributeSet()) {
			for(auto& attr : as->getAttributes()) {
				for(auto& desc : attr->getAttrDesc()) {
					auto ddp = DisplayDeviceDataPtr(new RenderVariableDeviceData(dd->getShader()->getAttributeIterator(desc.getAttrName())));
					desc.setDisplayData(ddp);
				}
			}
		}

		return DisplayDeviceDataPtr(dd);
	}

	void DisplayDeviceOpenGL::render(const Renderable* r) const
	{
		auto dd = std::dynamic_pointer_cast<OpenGLDeviceData>(r->getDisplayData());
		ASSERT_LOG(dd != NULL, "Failed to cast display data to the type required(OpenGLDeviceData).");
		auto shader = dd->getShader();
		shader->makeActive();

		BlendEquation::Manager blend(r->getBlendEquation());
		BlendModeManagerOGL blend_mode(r->getBlendMode());

		// lighting can be switched on or off at a material level.
		// so we grab the return of the Material::Apply() function
		// to find whether to apply it or not.
		bool use_lighting = true;
		if(r->getMaterial()) {
			use_lighting = r->getMaterial()->apply();
		}

		glm::mat4 pmat(1.0f);
		if(r->getCamera()) {
			// set camera here.
			pmat = r->getCamera()->getProjectionMat() * r->getCamera()->getViewMat();
		}

		if(use_lighting) {
			for(auto lp : r->getLights()) {
				/// xxx need to set lights here.
			}
		}

		if(r->getRenderTarget()) {
			r->getRenderTarget()->apply();
		}

		if(shader->getMvpUniform() != shader->uniformsIteratorEnd()) {
			pmat *= r->getModelMatrix();
			shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(pmat));
		}

		if(shader->getColorUniform() != shader->uniformsIteratorEnd() && r->isColorSet()) {
			shader->setUniformValue(shader->getColorUniform(), r->getColor().asFloatVector());
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
				ASSERT_LOG(rvdd != NULL, "Unable to cast DeviceData to RenderVariableDeviceData.");
				shader->SetUniformValue(rvdd->GetActiveMapIterator(), urv->Value());
			}
		}*/

		// Need to figure the interaction with shaders.
		/// XXX Need to create a mapping between attributes and the index value below.
		for(auto as : r->getAttributeSet()) {
			GLenum draw_mode = convert_drawing_mode(as->getDrawMode());
			std::vector<GLuint> enabled_attribs;

			// apply blend, if any, from attribute set.
			BlendEquation::Manager attrset_eq(r->getBlendEquation());
			BlendModeManagerOGL attrset_mode(r->getBlendMode());

			if(shader->getColorUniform() != shader->uniformsIteratorEnd() && as->getColor()) {
				shader->setUniformValue(shader->getColorUniform(), as->getColor()->asFloatVector());
			}

			for(auto& attr : as->getAttributes()) {
				auto attr_hw = attr->getDeviceBufferData();
				//auto attrogl = std::dynamic_pointer_cast<AttributeOGL>(attr);
				attr_hw->bind();
				for(auto& attrdesc : attr->getAttrDesc()) {
					auto ddp = std::dynamic_pointer_cast<RenderVariableDeviceData>(attrdesc.getDisplayData());
					ASSERT_LOG(ddp != NULL, "Converting attribute device data was NULL.");
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
		if(r->getMaterial()) {
			r->getMaterial()->unapply();
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
		return TexturePtr(new OpenGLTexture(node, nullptr));
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture(const SurfacePtr& surface, const variant& node)
	{
		return TexturePtr(new OpenGLTexture(node, surface));
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture(const SurfacePtr& surface, Texture::Type type, int mipmap_levels)
	{
		return TexturePtr(new OpenGLTexture(surface, type, mipmap_levels));
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture(unsigned width, PixelFormat::PF fmt)
	{
		return TexturePtr(new OpenGLTexture(width, 0, fmt, Texture::Type::TEXTURE_1D));
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture(unsigned width, unsigned height, PixelFormat::PF fmt, Texture::Type type)
	{
		return TexturePtr(new OpenGLTexture(width, height, fmt, Texture::Type::TEXTURE_2D));
	}
	
	TexturePtr DisplayDeviceOpenGL::handleCreateTexture(unsigned width, unsigned height, unsigned depth, PixelFormat::PF fmt)
	{
		return TexturePtr(new OpenGLTexture(width, height, fmt, Texture::Type::TEXTURE_3D, depth));
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture(const std::string& filename, Texture::Type type, int mipmap_levels)
	{
		auto surface = Surface::create(filename);
		return TexturePtr(new OpenGLTexture(surface, type, mipmap_levels));
	}

	TexturePtr DisplayDeviceOpenGL::handleCreateTexture(const SurfacePtr& surface, const SurfacePtr& palette)
	{
		return std::make_shared<OpenGLTexture>(surface, palette);
	}

	MaterialPtr DisplayDeviceOpenGL::handleCreateMaterial(const variant& node)
	{
		return MaterialPtr(new OpenGLMaterial(node));
	}

	MaterialPtr DisplayDeviceOpenGL::handleCreateMaterial(const std::string& name, 
		const std::vector<TexturePtr>& textures, 
		const BlendMode& blend, 
		bool fog, 
		bool lighting, 
		bool depth_write, 
		bool depth_check)
	{
		return MaterialPtr(new OpenGLMaterial(name, textures, blend, fog, lighting, depth_write, depth_check));
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
			// XXX We could put a force npot textures check here.
			if(GLEW_ARB_texture_non_power_of_two) {
				ret_val = true;
			}
			break;
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

	// XXX Need a way to deal with blits with Camera/Lighting.
	void DisplayDeviceOpenGL::doBlitTexture(const TexturePtr& tex, int dstx, int dsty, int dstw, int dsth, float rotation, int srcx, int srcy, int srcw, int srch)
	{
		auto texture = std::dynamic_pointer_cast<OpenGLTexture>(tex);
		ASSERT_LOG(texture != NULL, "Texture passed in was not of expected type.");

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
		std::unique_ptr<BlendModeManagerOGL> bmm;
		if(tex->hasBlendMode()) {
			bmm.reset(new BlendModeManagerOGL(tex->getBlendMode()));
		}

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
