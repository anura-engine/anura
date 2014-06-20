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

#include <GL/glew.h>

#include "../asserts.hpp"
#include "AttributeSetOpenGL.hpp"
#include "BlendOGL.hpp"
#include "CameraObject.hpp"
#include "CanvasOGL.hpp"
#include "ClipScopeOGL.hpp"
#include "DisplayDeviceOpenGL.hpp"
#include "FboOpenGL.hpp"
#include "LightObject.hpp"
#include "MaterialOpenGL.hpp"
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
		void SetShader(Shader::ShaderProgramPtr shader) {
			shader_ = shader;
		}
		Shader::ShaderProgramPtr GetShader() const { return shader_; }
	private:
		Shader::ShaderProgramPtr shader_;
		OpenGLDeviceData(const OpenGLDeviceData&);
	};

	class RenderVariableDeviceData : public DisplayDeviceData
	{
	public:
		RenderVariableDeviceData() {
		}
		~RenderVariableDeviceData() {
		}
		RenderVariableDeviceData(const Shader::ConstActivesMapIterator& it)
			: active_iterator_(it) {
		}

		void SetActiveMapIterator(const Shader::ConstActivesMapIterator& it) {
			active_iterator_ = it;
		}
		Shader::ConstActivesMapIterator GetActiveMapIterator() const { return active_iterator_; }
	private:
		Shader::ConstActivesMapIterator active_iterator_;
	};

	namespace 
	{
		GLenum ConvertRenderVariableType(AttributeDesc::VariableType type)
		{
			switch(type) {
				case AttributeDesc::VariableType::BOOL:							return GL_BYTE;
				case AttributeDesc::VariableType::HALF_FLOAT:					return GL_HALF_FLOAT;
				case AttributeDesc::VariableType::FLOAT:						return GL_FLOAT;
				case AttributeDesc::VariableType::DOUBLE:						return GL_DOUBLE;
				case AttributeDesc::VariableType::FIXED:						return GL_FIXED;
				case AttributeDesc::VariableType::SHORT:						return GL_SHORT;
				case AttributeDesc::VariableType::UNSIGNED_SHORT:				return GL_UNSIGNED_SHORT;
				case AttributeDesc::VariableType::BYTE:							return GL_BYTE;
				case AttributeDesc::VariableType::UNSIGNED_BYTE:				return GL_UNSIGNED_BYTE;
				case AttributeDesc::VariableType::INT:							return GL_INT;
				case AttributeDesc::VariableType::UNSIGNED_INT:					return GL_UNSIGNED_INT;
				case AttributeDesc::VariableType::INT_2_10_10_10_REV:			return GL_INT_2_10_10_10_REV;
				case AttributeDesc::VariableType::UNSIGNED_INT_2_10_10_10_REV:	return GL_UNSIGNED_INT_2_10_10_10_REV;
				case AttributeDesc::VariableType::UNSIGNED_INT_10F_11F_11F_REV:	return GL_UNSIGNED_INT_10F_11F_11F_REV;
			}
			ASSERT_LOG(false, "Unrecognised value for variable type.");
			return GL_NONE;
		}

		GLenum ConvertDrawingMode(AttributeSet::DrawMode dm)
		{
			switch(dm) {
				case AttributeSet::DrawMode::POINTS:			return GL_POINTS;
				case AttributeSet::DrawMode::LINE_STRIP:		return GL_LINE_STRIP;
				case AttributeSet::DrawMode::LINE_LOOP:			return GL_LINE_LOOP;
				case AttributeSet::DrawMode::LINES:				return GL_LINES;
				case AttributeSet::DrawMode::TRIANGLE_STRIP:	return GL_TRIANGLE_STRIP;
				case AttributeSet::DrawMode::TRIANGLE_FAN:		return GL_TRIANGLE_FAN;
				case AttributeSet::DrawMode::TRIANGLES:			return GL_TRIANGLES;
				case AttributeSet::DrawMode::QUAD_STRIP:		return GL_QUAD_STRIP;
				case AttributeSet::DrawMode::QUADS:				return GL_QUADS;
				case AttributeSet::DrawMode::POLYGON:			return GL_POLYGON;
			}
			ASSERT_LOG(false, "Unrecognised value for drawing mode.");
			return GL_NONE;
		}

		GLenum ConvertIndexType(AttributeSet::IndexType it) 
		{
			switch(it) {
				case AttributeSet::IndexType::INDEX_NONE:		break;
				case AttributeSet::IndexType::INDEX_UCHAR:		return GL_UNSIGNED_BYTE;
				case AttributeSet::IndexType::INDEX_USHORT:		return GL_UNSIGNED_SHORT;
				case AttributeSet::IndexType::INDEX_ULONG:		return GL_UNSIGNED_INT;
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

	void DisplayDeviceOpenGL::Init(size_t width, size_t height)
	{
		GLenum err = glewInit();
		ASSERT_LOG(err == GLEW_OK, "Could not initialise GLEW: " << glewGetErrorString(err));

		glViewport(0, 0, width, height);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Register with the render variable factory so we can create 
		// VBO backed render variables.
	}

	void DisplayDeviceOpenGL::PrintDeviceInfo()
	{
		GLint minor_version;
		GLint major_version;
		glGetIntegerv(GL_MINOR_VERSION, &minor_version);
		glGetIntegerv(GL_MINOR_VERSION, &major_version);
		if(glGetError() != GL_NONE) {
			// fall-back to old glGetStrings method.
			const char* version_str = reinterpret_cast<const char*>(glGetString(GL_VERSION));
			std::cerr << "OpenGL version: " << version_str << std::endl;
		} else {
			std::cerr << "OpenGL version: " << major_version << "." << minor_version << std::endl;
		}
	}

	void DisplayDeviceOpenGL::Clear(uint32_t clr)
	{
		glClear(clr & DISPLAY_CLEAR_COLOR ? GL_COLOR_BUFFER_BIT : 0 
			| clr & DISPLAY_CLEAR_DEPTH ? GL_DEPTH_BUFFER_BIT : 0 
			| clr & DISPLAY_CLEAR_STENCIL ? GL_STENCIL_BUFFER_BIT : 0);
	}

	void DisplayDeviceOpenGL::SetClearColor(float r, float g, float b, float a)
	{
		glClearColor(r, g, b, a);
	}

	void DisplayDeviceOpenGL::SetClearColor(const Color& color)
	{
		glClearColor(float(color.r()), float(color.g()), float(color.b()), float(color.a()));
	}

	void DisplayDeviceOpenGL::Swap()
	{
		// This is a no-action.
	}

	DisplayDeviceDataPtr DisplayDeviceOpenGL::CreateDisplayDeviceData(const DisplayDeviceDef& def)
	{
		OpenGLDeviceData* dd = new OpenGLDeviceData();
		bool use_default_shader = true;
		for(auto& hints : def.GetHints()) {
			if(hints.first == "shader") {
				// Need to have retrieved more shader data here.
				dd->SetShader(Shader::ShaderProgram::Factory(hints.second[0]));
				use_default_shader = false;
			}
			// ...
			// add more hints here if needed.
		}
		// If there is no shader hint, we will assume the default system shader.
		if(use_default_shader) {
			dd->SetShader(Shader::ShaderProgram::DefaultSystemShader());
		}
		
		// XXX Set uniforms from block here.

		for(auto& as : def.GetAttributeSet()) {
			for(auto& attr : as->GetAttributes()) {
				for(auto& desc : attr->GetAttrDesc()) {
					auto ddp = DisplayDeviceDataPtr(new RenderVariableDeviceData(dd->GetShader()->GetAttributeIterator(desc.AttrName())));
					desc.SetDisplayData(ddp);
				}
			}
		}

		return DisplayDeviceDataPtr(dd);
	}

	void DisplayDeviceOpenGL::render(const Renderable* r) const
	{
		auto dd = std::dynamic_pointer_cast<OpenGLDeviceData>(r->GetDisplayData());
		ASSERT_LOG(dd != NULL, "Failed to cast display data to the type required(OpenGLDeviceData).");
		auto shader = dd->GetShader();
		shader->MakeActive();

		BlendEquation::Manager blend(r->getBlendEquation());
		BlendModeManagerOGL blend_mode(r->getBlendMode());

		// lighting can be switched on or off at a material level.
		// so we grab the return of the Material::Apply() function
		// to find whether to apply it or not.
		bool use_lighting = true;
		if(r->Material()) {
			use_lighting = r->Material()->Apply();
		}

		glm::mat4 pmat(1.0f);
		if(r->Camera()) {
			// set camera here.
			pmat = r->Camera()->ProjectionMat() * r->Camera()->ViewMat();
		}

		if(use_lighting) {
			for(auto lp : r->Lights()) {
				/// xxx need to set lights here.
			}
		}

		if(r->GetRenderTarget()) {
			r->GetRenderTarget()->Apply();
		}

		if(shader->GetMvpUniform() != shader->UniformsIteratorEnd()) {
			pmat *= r->ModelMatrix();
			shader->SetUniformValue(shader->GetMvpUniform(), glm::value_ptr(pmat));
		}

		if(shader->GetColorUniform() != shader->UniformsIteratorEnd() && r->IsColorSet()) {
			shader->SetUniformValue(shader->GetColorUniform(), r->GetColor().asFloatVector());
		}

		if(shader->GetTexMapUniform() != shader->UniformsIteratorEnd()) {
			shader->SetUniformValue(shader->GetTexMapUniform(), 0);
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
		for(auto as : r->GetAttributeSet()) {
			GLenum draw_mode = ConvertDrawingMode(as->GetDrawMode());
			std::vector<GLuint> enabled_attribs;

			// apply blend, if any, from attribute set.
			BlendEquation::Manager attrset_eq(r->getBlendEquation());
			BlendModeManagerOGL attrset_mode(r->getBlendMode());

			if(shader->GetColorUniform() != shader->UniformsIteratorEnd() && as->getColor()) {
				shader->SetUniformValue(shader->GetColorUniform(), as->getColor()->asFloatVector());
			}

			for(auto& attr : as->GetAttributes()) {
				auto attr_hw = attr->GetDeviceBufferData();
				//auto attrogl = std::dynamic_pointer_cast<AttributeOGL>(attr);
				attr_hw->Bind();
				for(auto& attrdesc : attr->GetAttrDesc()) {
					auto ddp = std::dynamic_pointer_cast<RenderVariableDeviceData>(attrdesc.GetDisplayData());
					ASSERT_LOG(ddp != NULL, "Converting attribute device data was NULL.");
					glEnableVertexAttribArray(ddp->GetActiveMapIterator()->second.location);
					
					glVertexAttribPointer(ddp->GetActiveMapIterator()->second.location, 
						attrdesc.NumElements(), 
						ConvertRenderVariableType(attrdesc.VarType()), 
						attrdesc.Normalise(), 
						attrdesc.Stride(), 
						reinterpret_cast<const GLvoid*>(attr_hw->Value() + attr->GetOffset() + attrdesc.Offset()));
					enabled_attribs.emplace_back(ddp->GetActiveMapIterator()->second.location);
				}
			}

			if(as->IsInstanced()) {
				if(as->IsIndexed()) {
					as->BindIndex();
					// XXX as->GetIndexArray() should be as->GetIndexArray()+as->GetOffset()
					glDrawElementsInstanced(draw_mode, as->GetCount(), ConvertIndexType(as->GetIndexType()), as->GetIndexArray(), as->GetInstanceCount());
					as->UnbindIndex();
				} else {
					glDrawArraysInstanced(draw_mode, as->GetOffset(), as->GetCount(), as->GetInstanceCount());
				}
			} else {
				if(as->IsIndexed()) {
					as->BindIndex();
					// XXX as->GetIndexArray() should be as->GetIndexArray()+as->GetOffset()
					glDrawElements(draw_mode, as->GetCount(), ConvertIndexType(as->GetIndexType()), as->GetIndexArray());
					as->UnbindIndex();
				} else {
					glDrawArrays(draw_mode, as->GetOffset(), as->GetCount());
				}
			}

			for(auto attrib : enabled_attribs) {
				glDisableVertexAttribArray(attrib);
			}
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
		if(r->Material()) {
			r->Material()->Unapply();
		}
		if(r->GetRenderTarget()) {
			r->GetRenderTarget()->Unapply();
		}
	}

	TexturePtr DisplayDeviceOpenGL::HandleCreateTexture(const SurfacePtr& surface, const variant& node)
	{
		return TexturePtr(new OpenGLTexture(surface, node));
	}

	TexturePtr DisplayDeviceOpenGL::HandleCreateTexture(const SurfacePtr& surface, Texture::Type type, int mipmap_levels)
	{
		return TexturePtr(new OpenGLTexture(surface, type, mipmap_levels));
	}

	TexturePtr DisplayDeviceOpenGL::HandleCreateTexture(unsigned width, PixelFormat::PF fmt)
	{
		return TexturePtr(new OpenGLTexture(width, 0, fmt, Texture::Type::TEXTURE_1D));
	}

	TexturePtr DisplayDeviceOpenGL::HandleCreateTexture(unsigned width, unsigned height, PixelFormat::PF fmt, Texture::Type type)
	{
		return TexturePtr(new OpenGLTexture(width, height, fmt, Texture::Type::TEXTURE_2D));
	}
	
	TexturePtr DisplayDeviceOpenGL::HandleCreateTexture(unsigned width, unsigned height, unsigned depth, PixelFormat::PF fmt)
	{
		return TexturePtr(new OpenGLTexture(width, height, fmt, Texture::Type::TEXTURE_3D, depth));
	}

	TexturePtr DisplayDeviceOpenGL::HandleCreateTexture(const std::string& filename, Texture::Type type, int mipmap_levels)
	{
		auto surface = Surface::create(filename);
		return TexturePtr(new OpenGLTexture(surface, type, mipmap_levels));
	}

	MaterialPtr DisplayDeviceOpenGL::HandleCreateMaterial(const variant& node)
	{
		return MaterialPtr(new OpenGLMaterial(node));
	}

	MaterialPtr DisplayDeviceOpenGL::HandleCreateMaterial(const std::string& name, 
		const std::vector<TexturePtr>& textures, 
		const BlendMode& blend, 
		bool fog, 
		bool lighting, 
		bool depth_write, 
		bool depth_check)
	{
		return MaterialPtr(new OpenGLMaterial(name, textures, blend, fog, lighting, depth_write, depth_check));
	}

	RenderTargetPtr DisplayDeviceOpenGL::HandleCreateRenderTarget(size_t width, size_t height, 
			size_t color_plane_count, 
			bool depth, 
			bool stencil, 
			bool use_multi_sampling, 
			size_t multi_samples)
	{
		return RenderTargetPtr(new FboOpenGL(width, height, color_plane_count, depth, stencil, use_multi_sampling, multi_samples));
	}

	RenderTargetPtr DisplayDeviceOpenGL::HandleCreateRenderTarget(const variant& node)
	{
		return RenderTargetPtr(new FboOpenGL(node));
	}

	AttributeSetPtr DisplayDeviceOpenGL::HandleCreateAttributeSet(bool indexed, bool instanced)
	{
		return AttributeSetPtr(new AttributeSetOGL(indexed, instanced));
	}

	HardwareAttributePtr DisplayDeviceOpenGL::HandleCreateAttribute(AttributeBase* parent)
	{
		return HardwareAttributePtr(new HardwareAttributeOGL(parent));
	}

	CanvasPtr DisplayDeviceOpenGL::GetCanvas()
	{
		return CanvasOGL::getInstance();
	}

	ClipScopePtr DisplayDeviceOpenGL::createClipScope(const rect& r)
	{
		return ClipScopePtr(new ClipScopeOGL(r));
	}

	BlendEquationImplBasePtr DisplayDeviceOpenGL::getBlendEquationImpl()
	{
		return BlendEquationImplBasePtr(new BlendEquationImplOGL());
	}
	
	bool DisplayDeviceOpenGL::DoCheckForFeature(DisplayDeviceCapabilties cap)
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

	// XXX Need a way to deal with blits with Camera/Lighting.
	void DisplayDeviceOpenGL::DoBlitTexture(const TexturePtr& tex, int dstx, int dsty, int dstw, int dsth, float rotation, int srcx, int srcy, int srcw, int srch)
	{
		auto texture = std::dynamic_pointer_cast<OpenGLTexture>(tex);
		ASSERT_LOG(texture != NULL, "Texture passed in was not of expected type.");

		const float tx1 = float(srcx) / texture->Width();
		const float ty1 = float(srcy) / texture->Height();
		const float tx2 = srcw == 0 ? 1.0f : float(srcx + srcw) / texture->Width();
		const float ty2 = srch == 0 ? 1.0f : float(srcy + srch) / texture->Height();
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

		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3((vx1+vx2)/2.0f,(vy1+vy2)/2.0f,0.0f)) * glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0.0f,0.0f,1.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-(vx1+vy1)/2.0f,-(vy1+vy1)/2.0f,0.0f));
		glm::mat4 mvp = glm::ortho(0.0f, 800.0f, 600.0f, 0.0f) * model;
		auto shader = Shader::ShaderProgram::DefaultSystemShader();
		shader->MakeActive();
		texture->Bind();
		shader->SetUniformValue(shader->GetMvpUniform(), glm::value_ptr(mvp));
		shader->SetUniformValue(shader->GetColorUniform(), glm::value_ptr(glm::vec4(1.0f,1.0f,1.0f,1.0f)));
		shader->SetUniformValue(shader->GetTexMapUniform(), 0);
		// XXX the following line are only temporary, obviously.
		shader->SetUniformValue(shader->GetUniformIterator("discard"), 0);
		glEnableVertexAttribArray(shader->GetVertexAttribute()->second.location);
		glVertexAttribPointer(shader->GetVertexAttribute()->second.location, 2, GL_FLOAT, GL_FALSE, 0, vtx_coords);
		glEnableVertexAttribArray(shader->GetTexcoordAttribute()->second.location);
		glVertexAttribPointer(shader->GetTexcoordAttribute()->second.location, 2, GL_FLOAT, GL_FALSE, 0, uv_coords);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glDisableVertexAttribArray(shader->GetTexcoordAttribute()->second.location);
		glDisableVertexAttribArray(shader->GetVertexAttribute()->second.location);
	}
}
