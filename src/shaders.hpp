/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once
#ifndef SHADERS_HPP_INCLUDED
#define SHADERS_HPP_INCLUDED

#include <map>
#include <vector>
#include <boost/intrusive_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include "asserts.hpp"
#include "entity_fwd.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "formula_variable_storage.hpp"

namespace gles2
{

class shader 
{
public:
	static void set_runtime_error(const std::string& msg);
	static std::string get_and_clear_runtime_error();

	shader() : type_(0), shader_(0)
	{}
	explicit shader(GLenum type, const std::string& name, const std::string& code);
	GLuint get() const { return shader_; }
	std::string name() const { return name_; }
	std::string code() const { return code_; }
protected:
	bool compile(const std::string& code);
private:
	GLenum type_;
	GLuint shader_;
	std::string name_;
	std::string code_;
};

struct actives
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
	// Last value
	variant last_value;
};

class program;
typedef boost::intrusive_ptr<program> program_ptr;
typedef boost::intrusive_ptr<const program> const_program_ptr;

typedef std::map<std::string,actives>::iterator actives_map_iterator;
typedef std::map<std::string,actives>::const_iterator const_actives_map_iterator;

class program : public game_logic::formula_callable
{
public:
	program();
	explicit program(const std::string& name, const shader& vs, const shader& fs);
	virtual ~program()
	{}
	void init(const std::string& name, const shader& vs, const shader& fs);
	GLuint get() const { return object_; }
	GLuint get_attribute(const std::string& attr) const;
	GLint get_uniform(const std::string& attr) const;
	std::string name() const { return name_; }
	game_logic::formula_ptr create_formula(const variant& v);
	bool execute_command(const variant& var);
	actives_map_iterator get_uniform_reference(const std::string& name);
	actives_map_iterator get_attribute_reference(const std::string& name);
	void set_uniform(const actives_map_iterator& it, const variant& value);
	void set_uniform(const actives_map_iterator& it, const GLsizei count, const GLfloat* fv);
	void set_uniform_or_defer(actives_map_iterator& it, const variant& value);
	void set_uniform_or_defer(const std::string& key, const variant& value);
	variant get_uniform_value(const std::string& key) const;
	void set_attributes(const std::string& key, const variant& value);
	void set_attributes(const actives_map_iterator& it, const variant& value);
	variant get_attributes_value(const std::string& key) const;
	game_logic::formula_callable* get_environment() { return environ_; }
	void set_deferred_uniforms();
	GLint mvp_matrix_uniform() const { return u_mvp_matrix_; }
	GLint vertex_attribute() const { return vertex_location_; }
	GLint texcoord_attribute() const { return texcoord_location_; }

	virtual variant write();

	void vertex_attrib_array(GLint ndx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr);

	virtual void vertex_array(GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr);
	virtual void texture_array(GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr);
	virtual void color_array(GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr);
	virtual void set_fixed_attributes(const variant& node);
	virtual void set_fixed_uniforms(const variant& node);
	virtual void set_fixed_attributes();
	virtual void set_fixed_uniforms();

	void disable_vertex_attrib(GLint);

	static void load_shaders(const std::string& shader_data);
	static program_ptr find_program(const std::string& prog_name);
	static void add_shader(const std::string& program_name, 
		const shader& v_shader, 
		const shader& f_shader,
		const variant& prog,
		const variant& uniforms);
	static std::map<std::string, gles2::program_ptr>& get_shaders();
	static void clear_shaders();
	void set_known_uniforms();
	void set_sprite_area(const GLfloat* fl);
	void set_draw_area(const GLfloat* fl);
	void set_cycle(int cycle);

	const shader& vertex_shader() const { return vs_; }
	const shader& fragment_shader() const { return fs_; }
private:
	bool link();
	bool queryUniforms();
	bool queryAttributes();

	std::vector<GLint> active_attributes_;
	variant stored_attributes_;
	variant stored_uniforms_;

	DECLARE_CALLABLE(program)

	game_logic::formula_callable* environ_;
	std::string name_;
	shader vs_;
	shader fs_;
	GLuint object_;
	std::map<std::string, actives> attribs_;
	std::map<std::string, actives> uniforms_;

	//"vertex" and "texcoord" values within stored_attributes_
	std::string vertex_attribute_;
	std::string texcoord_attribute_;

	GLint vertex_location_, texcoord_location_;

	std::vector<std::map<std::string, actives>::iterator> uniforms_to_update_;

	GLint u_tex_map_;
	GLint u_mvp_matrix_;
	GLint u_sprite_area_;
	GLint u_draw_area_;
	GLint u_cycle_;
	GLint u_color_;
	GLint u_point_size_;
	GLint u_discard_;

	friend class shader_program;
};

class shader_program : public game_logic::formula_callable
{
public:
	static boost::intrusive_ptr<shader_program> get_global(const std::string& key);
	shader_program();
	shader_program(const shader_program& o);
	explicit shader_program(const variant& node, entity* obj = NULL);
	explicit shader_program(const std::string& program_name);
	virtual ~shader_program()
	{}

	bool enabled() const { return enabled_; }
	void configure(const variant& node, entity* obj = NULL);
	void init(entity* obj);
	game_logic::formula_ptr create_formula(const variant& v);
	bool execute_command(const variant& var);
	int zorder() const { return zorder_; }

	void prepare_draw();
	void refresh_for_draw();
	program_ptr shader() const;
	const std::string& name() const { return name_; }
	entity* parent() const { return parent_; }
	void set_parent(entity* obj) { parent_ = obj; }

	game_logic::formula_callable* vars() { return vars_.get(); }
	const game_logic::formula_callable* vars() const { return vars_.get(); }

	void clear();
	variant write();
protected:

	struct DrawCommand {
		DrawCommand();
		std::map<std::string, actives>::iterator target;
		variant value;
		bool increment;
	};

	class attribute_commands_callable : public game_logic::formula_callable
	{
	public:
		void set_program(program_ptr program) { program_ = program; }
		void execute_on_draw();
	private:
		virtual variant get_value(const std::string& key) const;
		virtual void set_value(const std::string& key, const variant& value);

		program_ptr program_;
		std::vector<DrawCommand> attribute_commands_;
	};

	class uniform_commands_callable : public game_logic::formula_callable
	{
	public:
		void set_program(program_ptr program) { program_ = program; }
		void execute_on_draw();
	private:
		virtual variant get_value(const std::string& key) const;
		virtual void set_value(const std::string& key, const variant& value);

		program_ptr program_;
		std::vector<DrawCommand> uniform_commands_;
	};

private:
	DECLARE_CALLABLE(shader_program)

	void operator=(const shader_program&);

	std::string name_;
	program_ptr program_object_;

	game_logic::formula_variable_storage_ptr vars_;

	std::vector<std::string> create_commands_;
	std::vector<std::string> draw_commands_;
	std::vector<game_logic::formula_ptr> create_formulas_;
	std::vector<game_logic::formula_ptr> draw_formulas_;

	boost::intrusive_ptr<uniform_commands_callable> uniform_commands_;
	boost::intrusive_ptr<attribute_commands_callable> attribute_commands_;

	// fake zorder value
	int zorder_;

	entity* parent_;

	bool enabled_;
};

typedef boost::intrusive_ptr<shader_program> shader_program_ptr;
typedef boost::intrusive_ptr<const shader_program> const_shader_program_ptr;

}

GLenum get_blend_mode(variant v);

#endif
