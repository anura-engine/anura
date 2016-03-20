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

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "SDL_opengles2.h"

#include "Shaders.hpp"

namespace KRE
{
	namespace GLESv2
	{
		// Abstraction of vertex/geometry/fragment shader
		class Shader
		{
		public:
			explicit Shader(GLenum type, const std::string& name, const std::string& code);
			GLuint get() const { return shader_; }
			std::string name() const { return name_; }
		protected:
			bool compile(const std::string& code);
		private:
			GLenum type_;
			GLuint shader_;
			std::string name_;
		};
		//typedef std::unique_ptr<Shader> ShaderPtr;

		struct Actives
		{
			// Name of variable.
			std::string name;
			// type of the uniform/attribute variable
			GLenum type;
			// If an array type, this is the maximum number of array elements used 
			// in the program. Value is 1 if type is not an array type.
			GLsizei num_elements;
			// Location of the active uniform/attribute
			GLint location;
		};

		typedef std::pair<std::string,std::string> ShaderDef;

		typedef std::map<std::string, Actives> ActivesMap;

		class ShaderProgram;
		typedef std::shared_ptr<ShaderProgram> ShaderProgramPtr;

		class ShaderProgram : public KRE::ShaderProgram
		{
		public:
			ShaderProgram(const std::string& name, const ShaderDef& va, const ShaderDef& fs, const variant& node);
			ShaderProgram(const std::string& name, const std::vector<ShaderData>& shader_data, const std::vector<ActiveMapping>& uniform_map, const std::vector<ActiveMapping>& attribute_map);
			virtual ~ShaderProgram();
			void init(const std::string& name, const ShaderDef& vs, const ShaderDef& fs);
			std::string name() const { return name_; }

			int getAttributeOrDie(const std::string& attr) const override;
			int getUniformOrDie(const std::string& attr) const override;

			int getAttribute(const std::string& attr) const override;
			int getUniform(const std::string& attr) const override;

			void setActives();

			void setUniformValue(int uid, const GLint) const override;
			void setUniformValue(int uid, const GLfloat) const override;
			void setUniformValue(int uid, const GLfloat*) const override;
			void setUniformValue(int uid, const GLint*) const override;
			void setUniformValue(int uid, const void*) const override;

			void setUniformFromVariant(int uid, const variant& value) const override;

			void makeActive() override;

			void configureActives(AttributeSetPtr attrset) override;
			void configureAttribute(AttributeBasePtr attr) override;
			void configureUniforms(UniformBufferBase& uniforms) override;

			void setAlternateUniformName(const std::string& name, const std::string& alt_name);
			void setAlternateAttributeName(const std::string& name, const std::string& alt_name);

			void setUniformMapping(const std::vector<std::pair<std::string, std::string>>& mapping) override;
			void setAttributeMapping(const std::vector<std::pair<std::string, std::string>>& mapping) override;

			static ShaderProgramPtr factory(const std::string& name);
			static ShaderProgramPtr factory(const variant& node);
			static ShaderProgramPtr defaultSystemShader();
			static void loadShadersFromVariant(const variant& node);
			static ShaderProgramPtr getProgramFromVariant(const variant& node);
			static ShaderProgramPtr createShader(const std::string& name, 
				const std::vector<ShaderData>& shader_data, 
				const std::vector<ActiveMapping>& uniform_map,
				const std::vector<ActiveMapping>& attribute_map);
			static ShaderProgramPtr createGaussianShader(int radius);

			int getColorUniform() const override { return u_color_; }
			int getLineWidthUniform() const override { return u_line_width_; }
			int getMvUniform() const override { return u_mv_; }
			int getPUniform() const override { return u_p_; }
			int getMvpUniform() const override { return u_mvp_; }
			int getTexMapUniform() const override { return u_tex_; }
			
			int getColorAttribute() const override { return a_color_; }
			int getVertexAttribute() const override { return a_vertex_; }
			int getTexcoordAttribute() const override { return a_texcoord_; }
			int getNormalAttribute() const override { return a_normal_; }
		
			void applyAttribute(AttributeBasePtr attr) override;
			void cleanUpAfterDraw() override;

			void setUniformsForTexture(const TexturePtr& tex) const override;

			KRE::ShaderProgramPtr clone() override;
		protected:
			bool link(const std::vector<Shader>& shader_programs);
			bool queryUniforms();
			bool queryAttributes();

			std::vector<GLint> active_attributes_;
		private:
			void operator=(const ShaderProgram&);

			std::string name_;
			GLuint object_;
			ActivesMap attribs_;
			ActivesMap uniforms_;
			std::unordered_map<int, Actives> v_uniforms_;
			std::unordered_map<int, Actives> v_attribs_;
			std::map<std::string, std::string> uniform_alternate_name_map_;
			std::map<std::string, std::string> attribute_alternate_name_map_;

			// Store for common attributes and uniforms
			int u_mvp_;
			int u_mv_;
			int u_p_;
			int u_color_;
			int u_line_width_;
			int u_tex_;
			int a_vertex_;
			int a_texcoord_;
			int a_color_;
			int a_normal_;

			int u_enable_palette_lookup_;
			int u_palette_;
			int u_palette_width_;
			int u_palette_map_;
			int u_mix_palettes_;
			int u_mix_;

			std::vector<GLuint> enabled_attribs_;
		};
	}
}
