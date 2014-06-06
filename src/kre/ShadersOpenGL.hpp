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

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <GL/glew.h>

namespace Shader
{
	// Abstraction of vertex/geometry/fragment shader
	class Shader
	{
	public:
		explicit Shader(GLenum type, const std::string& name, const std::string& code);
		GLuint Get() const { return shader_; }
		std::string Name() const { return name_; }
	protected:
		bool Compile(const std::string& code);
	private:
		GLenum type_;
		GLuint shader_;
		std::string name_;
		Shader();
		Shader(const Shader&);
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

	typedef std::pair<std::string,std::string> ShaderDef;

	class ShaderProgram;
	typedef std::shared_ptr<ShaderProgram> ShaderProgramPtr;

	class ShaderProgram
	{
	public:
		ShaderProgram(const std::string& name, const ShaderDef& va, const ShaderDef& fs);
		virtual ~ShaderProgram();
		void Init(const std::string& name, const ShaderDef& vs, const ShaderDef& fs);
		std::string Name() const { return name_; }
		GLint GetAttributeOrDie(const std::string& attr) const;
		GLint GetUniformOrDie(const std::string& attr) const;
		GLint GetAttribute(const std::string& attr) const;
		GLint GetUniform(const std::string& attr) const;
		ConstActivesMapIterator GetAttributeIterator(const std::string& attr) const;
		ConstActivesMapIterator GetUniformIterator(const std::string& attr) const;

		void SetActives();

		void SetUniformValue(ConstActivesMapIterator it, const GLint);
		void SetUniformValue(ConstActivesMapIterator it, const GLfloat);
		void SetUniformValue(ConstActivesMapIterator it, const GLfloat*);
		void SetUniformValue(ConstActivesMapIterator it, const GLint*);
		void SetUniformValue(ConstActivesMapIterator it, const void*);

		void MakeActive();

		void SetAlternateUniformName(const std::string& name, const std::string& alt_name);
		void SetAlternateAttributeName(const std::string& name, const std::string& alt_name);

		static ShaderProgramPtr Factory(const std::string& name);
		static ShaderProgramPtr DefaultSystemShader();

		ConstActivesMapIterator GetColorUniform() const { return u_color_; }
		ConstActivesMapIterator GetMvpUniform() const { return u_mvp_; }
		ConstActivesMapIterator GetTexMapUniform() const { return u_tex_; }
		ConstActivesMapIterator GetColorAttribute() const { return a_color_; }
		ConstActivesMapIterator GetVertexAttribute() const { return a_vertex_; }
		ConstActivesMapIterator GetTexcoordAttribute() const { return a_texcoord_; }
		
		ConstActivesMapIterator UniformsIteratorEnd() const { return uniforms_.end(); }
		ConstActivesMapIterator AttributesIteratorEnd() const { return attribs_.end(); }

	protected:
		bool Link();
		bool QueryUniforms();
		bool QueryAttributes();

		std::vector<GLint> active_attributes_;
	private:
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
		ConstActivesMapIterator u_color_;
		ConstActivesMapIterator u_tex_;
		ConstActivesMapIterator a_vertex_;
		ConstActivesMapIterator a_texcoord_;
		ConstActivesMapIterator a_color_;

		ShaderProgram();
		ShaderProgram(const ShaderProgram&);
	};
}
