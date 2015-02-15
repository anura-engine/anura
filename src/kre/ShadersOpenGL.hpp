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

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <GL/glew.h>

#include "Shaders.hpp"

namespace KRE
{
	namespace OpenGL
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
		typedef std::unique_ptr<Shader> ShaderPtr;

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

		typedef std::map<std::string, Actives> ActivesMap;
		typedef ActivesMap::iterator ActivesMapIterator;
		typedef ActivesMap::const_iterator ConstActivesMapIterator;

		class ActivesHandle : public ActivesHandleBase
		{
		public:
			ActivesHandle(ConstActivesMapIterator a) : active_(a) {}
			~ActivesHandle() {}
			ConstActivesMapIterator getIterator() { return active_; }
		private:
			ConstActivesMapIterator active_;
		};

		typedef std::pair<std::string,std::string> ShaderDef;

		class ShaderProgram;
		typedef std::shared_ptr<ShaderProgram> ShaderProgramPtr;

		class ShaderProgram : public KRE::ShaderProgram
		{
		public:
			ShaderProgram(const std::string& name, const ShaderDef& va, const ShaderDef& fs, const variant& node);
			virtual ~ShaderProgram();
			void init(const std::string& name, const ShaderDef& vs, const ShaderDef& fs);
			std::string name() const { return name_; }
			GLint getAttributeOrDie(const std::string& attr) const;
			GLint getUniformOrDie(const std::string& attr) const;
			GLint getAttribute(const std::string& attr) const;
			GLint getUniform(const std::string& attr) const;
			ConstActivesMapIterator getAttributeIterator(const std::string& attr) const;
			ConstActivesMapIterator getUniformIterator(const std::string& attr) const;

			void setActives();

			void setUniformValue(ConstActivesMapIterator it, const GLint);
			void setUniformValue(ConstActivesMapIterator it, const GLfloat);
			void setUniformValue(ConstActivesMapIterator it, const GLfloat*);
			void setUniformValue(ConstActivesMapIterator it, const GLint*);
			void setUniformValue(ConstActivesMapIterator it, const void*);

			void makeActive() override;

			void setAlternateUniformName(const std::string& name, const std::string& alt_name);
			void setAlternateAttributeName(const std::string& name, const std::string& alt_name);

			static ShaderProgramPtr factory(const std::string& name);
			static ShaderProgramPtr factory(const variant& node);
			static ShaderProgramPtr defaultSystemShader();
			static void loadFromFile(const variant& node);
			static ShaderProgramPtr getProgramFromVariant(const variant& node);

			ConstActivesMapIterator getColorUniform() const { return u_color_; }
			ConstActivesMapIterator getLineWidthUniform() const { return u_line_width_; }
			ConstActivesMapIterator getMvUniform() const { return u_mv_; }
			ConstActivesMapIterator getPUniform() const { return u_p_; }
			ConstActivesMapIterator getMvpUniform() const { return u_mvp_; }
			ConstActivesMapIterator getTexMapUniform() const { return u_tex_; }
			ConstActivesMapIterator getColorAttribute() const { return a_color_; }
			ConstActivesMapIterator getVertexAttribute() const { return a_vertex_; }
			ConstActivesMapIterator getTexcoordAttribute() const { return a_texcoord_; }
			ConstActivesMapIterator getNormalAttribute() const { return a_normal_; }
		
			ConstActivesMapIterator uniformsIteratorEnd() const { return uniforms_.end(); }
			ConstActivesMapIterator attributesIteratorEnd() const { return attribs_.end(); }

			ActivesHandleBasePtr getHandle(const std::string& name) override;

			void setUniform(ActivesHandleBasePtr active, const void*) override;

			void applyActives() override;

		protected:
			bool link();
			bool queryUniforms();
			bool queryAttributes();

			std::vector<GLint> active_attributes_;
		private:
			DISALLOW_COPY_AND_ASSIGN(ShaderProgram);

			std::string name_;
			ShaderPtr vs_;
			ShaderPtr fs_;
			GLuint object_;
			ActivesMap attribs_;
			ActivesMap uniforms_;
			std::map<std::string, std::string> uniform_alternate_name_map_;
			std::map<std::string, std::string> attribute_alternate_name_map_;

			// Store for common attributes and uniforms
			ConstActivesMapIterator u_mvp_;
			ConstActivesMapIterator u_mv_;
			ConstActivesMapIterator u_p_;
			ConstActivesMapIterator u_color_;
			ConstActivesMapIterator u_line_width_;
			ConstActivesMapIterator u_tex_;
			ConstActivesMapIterator a_vertex_;
			ConstActivesMapIterator a_texcoord_;
			ConstActivesMapIterator a_color_;
			ConstActivesMapIterator a_normal_;
		};
	}
}
