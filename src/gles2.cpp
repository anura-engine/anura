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
#if defined(USE_SHADERS)

#include <glm/gtc/matrix_transform.hpp>

#include "asserts.hpp"
#include "filesystem.hpp"
#include "graphics.hpp"
#include "gles2.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "module.hpp"

namespace {
	typedef std::stack<glm::mat4> projection_mat_stack;
	typedef std::stack<glm::mat4> modelview_mat_stack;

	GLenum matrix_mode = GL_PROJECTION;
	projection_mat_stack p_mat_stack;
	modelview_mat_stack mv_mat_stack;

	// Current project/modelview matricies
	glm::mat4 proj_matrix = glm::mat4(1.0f);
	glm::mat4 modelview_matrix = glm::mat4(1.0f);

	float colors[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	GLenum shade_model = GL_FLAT;
	GLfloat point_size = 1.0f;

	std::string gl_error_to_string(GLenum error) {
#define DEFINE_ERROR(err) case err: return #err
		switch(error) {
			DEFINE_ERROR(GL_NO_ERROR);
			DEFINE_ERROR(GL_INVALID_ENUM);
			DEFINE_ERROR(GL_INVALID_VALUE);
			DEFINE_ERROR(GL_INVALID_OPERATION);
			DEFINE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
			DEFINE_ERROR(GL_OUT_OF_MEMORY);
			DEFINE_ERROR(GL_STACK_UNDERFLOW);
			DEFINE_ERROR(GL_STACK_OVERFLOW);
			default:
				return "Unknown error";
		}
#undef DEFINE_ERROR
	}

	void check_gl_errors() {
		GLenum err = glGetError();
		ASSERT_LOG(err == GL_NONE, "Error in shader code: " << " : 0x" << std::hex << err << ": " << gl_error_to_string(err));
	}
}

#if defined(GL_ES_VERSION_2_0)

extern "C" {

void glMatrixMode(GLenum mode)
{
	ASSERT_LOG(mode == GL_MODELVIEW || mode == GL_PROJECTION, "Unrecognised matrix mode: " << mode)
	matrix_mode = mode;
}

void glPushMatrix()
{
	if(matrix_mode == GL_MODELVIEW) {
		mv_mat_stack.push(modelview_matrix);
	} else if(matrix_mode == GL_PROJECTION) {
		p_mat_stack.push(proj_matrix);
	}
}

void glPopMatrix()
{
	if(matrix_mode == GL_MODELVIEW) {
		modelview_matrix = mv_mat_stack.top();
		mv_mat_stack.pop();
	} else if(matrix_mode == GL_PROJECTION) {
		proj_matrix = p_mat_stack.top();
		p_mat_stack.pop();
	}
}

void glLoadIdentity()
{
	if(matrix_mode == GL_MODELVIEW) {
		modelview_matrix = glm::mat4(1.0f);
	} else if(matrix_mode == GL_PROJECTION) {
		proj_matrix = glm::mat4(1.0f);
	}
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	if(matrix_mode == GL_MODELVIEW) {
		modelview_matrix = glm::translate(modelview_matrix, glm::vec3(x,y,z));
	} else if(matrix_mode == GL_PROJECTION) {
		proj_matrix = glm::translate(proj_matrix, glm::vec3(x,y,z));
	}
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	if(matrix_mode == GL_MODELVIEW) {
		modelview_matrix = glm::rotate(modelview_matrix, angle, glm::vec3(x,y,z));
	} else if(matrix_mode == GL_PROJECTION) {
		proj_matrix = glm::rotate(proj_matrix, angle, glm::vec3(x,y,z));
	}
}

void glScalef (GLfloat x, GLfloat y, GLfloat z)
{
	if(matrix_mode == GL_MODELVIEW) {
		modelview_matrix = glm::scale(modelview_matrix, glm::vec3(x,y,z));
	} else if(matrix_mode == GL_PROJECTION) {
		proj_matrix = glm::scale(proj_matrix, glm::vec3(x,y,z));
	}
}

void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	colors[0] = red;
	colors[1] = green;
	colors[2] = blue;
	colors[3] = alpha;
}

void glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
	colors[0] = float(red)/255.0f;
	colors[1] = float(green)/255.0f;
	colors[2] = float(blue)/255.0f;
	colors[3] = float(alpha)/255.0f;
}

void glGetFloatv_1(GLenum pname, GLfloat* params)
{
	ASSERT_LOG(params != NULL, "params must not be null");
	if(pname == GL_CURRENT_COLOR) {
		memcpy(params, colors, sizeof(colors));
	} else if(pname == GL_MODELVIEW_MATRIX) {
		memcpy(params, glm::value_ptr(modelview_matrix), sizeof(modelview_matrix));
	} else if(pname == GL_PROJECTION_MATRIX) {
		memcpy(params, glm::value_ptr(proj_matrix), sizeof(proj_matrix));
	} else {
		ASSERT_LOG(false, "Unsupported mode in the call: " << pname);
	}
}

void glShadeModel(GLenum mode)
{
	ASSERT_LOG(mode == GL_FLAT || mode == GL_SMOOTH, "Unrecognised shade mode: " << mode)
	shade_model = mode;
}

void glOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar)
{
	glm::mat4 ortho = glm::frustum(left, right, bottom, top, zNear, zFar);

	if(matrix_mode == GL_MODELVIEW) {
		modelview_matrix = ortho;
	} else if(matrix_mode == GL_PROJECTION) {
		proj_matrix = ortho;
	}
}

void glPointSize(GLfloat size)
{
	point_size = size;
}

}
#else

void glColor4f_1(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	colors[0] = red;
	colors[1] = green;
	colors[2] = blue;
	colors[3] = alpha;
}

void glColor4ub_1(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
	colors[0] = float(red)/255.0f;
	colors[1] = float(green)/255.0f;
	colors[2] = float(blue)/255.0f;
	colors[3] = float(alpha)/255.0f;
}

#endif

/////////////////////////////////////////////////////

namespace {
	const std::string fs1 = 
		"uniform vec4 u_color;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = u_color;\n"
		"}\n";
	const std::string vs1 = 
		"uniform mat4 mvp_matrix;\n"
		"uniform float u_point_size;\n"
		"attribute vec2 a_position;\n"
		"void main()\n"
		"{\n"
		"	gl_PointSize = u_point_size;\n"
		"	gl_Position = mvp_matrix * vec4(a_position,0.0,1.0);\n"
		"}\n";
    const std::string simple_shader_info = 
		"{\"shader\": {\n"
		"    \"program\": \"simple_shader\",\n"
		"}}\n";
	const std::string simple_attribute_info = 
		"{\n"
		"    \"attributes\": {\n"
		"        \"vertex\": \"a_position\",\n"
		"    },\n"
		"	\"uniforms\": {\n"
		"		\"mvp_matrix\": \"mvp_matrix\",\n"
		"		\"color\": \"u_color\",\n"
		"		\"point_size\": \"u_point_size\",\n"
        "    },\n"
		"}\n";

	const std::string fs_col = 
		"varying vec4 v_color;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = v_color;\n"
		"}\n";
	const std::string vs_col = 
		"uniform mat4 mvp_matrix;\n"
		"uniform float u_point_size;\n"
		"attribute vec2 a_position;\n"
		"attribute vec4 a_color;\n"
		"varying vec4 v_color;\n"
		"void main()\n"
		"{\n"
		"	v_color = a_color;\n"
		"	gl_PointSize = u_point_size;\n"
		"	gl_Position = mvp_matrix * vec4(a_position,0.0,1.0);\n"
		"}\n";
    const std::string simple_col_shader_info = 
		"{\"shader\": {\n"
		"    \"program\": \"simple_col_shader\",\n"
		"}}\n";
	const std::string simple_col_attribute_info = 
		"{\n"
		"    \"attributes\": {\n"
		"        \"vertex\": \"a_position\",\n"
		"        \"color\": \"a_color\",\n"
		"    },\n"
		"	\"uniforms\": {\n"
		"		\"mvp_matrix\": \"mvp_matrix\",\n"
		"		\"point_size\": \"u_point_size\",\n"
        "    },\n"
		"}\n";

	const std::string fs_tex = 
		"uniform sampler2D u_tex_map;\n"
		"uniform vec4 u_color;\n"
		"uniform bool u_anura_discard;\n"
		"varying vec2 v_texcoord;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = texture2D(u_tex_map, v_texcoord) * u_color;\n"
		"	if(u_anura_discard && gl_FragColor[3] == 0.0) { discard; }\n"
		"}\n";
	const std::string vs_tex = 
		"uniform mat4 mvp_matrix;\n"
		"attribute vec2 a_position;\n"
		"attribute vec2 a_texcoord;\n"
		"varying vec2 v_texcoord;\n"
		"void main()\n"
		"{\n"
		"	v_texcoord = a_texcoord;\n"
		"	gl_Position = mvp_matrix * vec4(a_position,0.0,1.0);\n"
		"}\n";
	const std::string tex_shader_info = 
		"{\"shader\": {\n"
        "    \"program\": \"tex_shader\",\n"
		"    \"create\": \"[set(uniforms.u_tex_map, 0)]\",\n"
		"}}\n";
	const std::string tex_attribute_info = 
		"{\n"
        "    \"attributes\": {\n"
        "        \"vertex\": \"a_position\",\n"
        "        \"texcoord\": \"a_texcoord\",\n"
        "    },\n"
		"	\"uniforms\": {\n"
		"		\"mvp_matrix\": \"mvp_matrix\",\n"
		"		\"color\": \"u_color\",\n"
        "    },\n"
		"}\n";

	const std::string fs_texcol = 
		"uniform sampler2D u_tex_map;\n"
		"varying vec4 v_color;\n"
		"varying vec2 v_texcoord;\n"
		"uniform bool u_anura_discard;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = texture2D(u_tex_map, v_texcoord) * v_color;\n"
		"	if(u_anura_discard && gl_FragColor[3] == 0.0) { discard; }\n"
		"}\n";
	const std::string vs_texcol = 
		"uniform mat4 mvp_matrix;\n"
		"attribute vec2 a_position;\n"
		"attribute vec4 a_color;\n"
		"attribute vec2 a_texcoord;\n"
		"varying vec2 v_texcoord;\n"
		"varying vec4 v_color;\n"
		"void main()\n"
		"{\n"
		"	v_color = a_color;\n"
		"	v_texcoord = a_texcoord;\n"
		"	gl_Position = mvp_matrix * vec4(a_position,0.0,1.0);\n"
		"}\n";
	const std::string texcol_shader_info = 
		"{\"shader\": {\n"
        "    \"program\": \"texcol_shader\",\n"
		"    \"create\": \"[set(uniforms.u_tex_map, 0)]\",\n"
		"    \"draw\": \"[set(attributes.a_color,color)]\",\n"
		"}}\n";
	const std::string texcol_attribute_info = 
		"{\n"
        "    \"attributes\": {\n"
        "        \"vertex\": \"a_position\",\n"
        "        \"texcoord\": \"a_texcoord\",\n"
		"        \"color\": \"a_color\",\n"
        "    },\n"
		"	\"uniforms\": {\n"
		"		\"mvp_matrix\": \"mvp_matrix\",\n"
        "    },\n"
		"}\n";

	static gles2::shader_program_ptr tex_shader_program;
	static gles2::shader_program_ptr texcol_shader_program;
	static gles2::shader_program_ptr simple_shader_program;
	static gles2::shader_program_ptr simple_col_shader_program;

	std::stack<gles2::shader_program_ptr> shader_stack;
	gles2::shader_program_ptr active_shader_program;

	struct blend_mode 
	{
		GLenum blend_src_mode;
		GLenum blend_dst_mode;
		bool blend_enabled;
		blend_mode(GLenum src, GLenum dst, bool en) 
			: blend_src_mode(src), blend_dst_mode(dst), blend_enabled(en)
		{}
	};
	std::stack<blend_mode> blend_stack;

	std::stack<GLint> active_texture_unit;
}

namespace gles2 {
	fixed_program::fixed_program() : program(), vtx_coord_(-1), col_coord_(-1)
	{
		tex_coord_[0] = tex_coord_[1] = -1;
	}

	fixed_program::fixed_program(const std::string& name, const shader& vs, const shader& fs) 
		: program(name, vs, fs), vtx_coord_(-1), col_coord_(-1)
	{
		tex_coord_[0] = tex_coord_[1] = -1;
	}

	void fixed_program::vertex_array(GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr)
	{
		vertex_attrib_array(vtx_coord_, size, type, normalized, stride, ptr);
	}

	void fixed_program::texture_array(GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr)
	{
		vertex_attrib_array(tex_coord_[0], size, type, normalized, stride, ptr);
	}

	void fixed_program::color_array(GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr)
	{
		vertex_attrib_array(col_coord_, size, type, normalized, stride, ptr);
	}

	void fixed_program::add_shader(const std::string& program_name, 
			const shader& v_shader, 
			const shader& f_shader,
			const variant& prog,
			const variant& uniforms)
	{
		std::map<std::string, gles2::program_ptr>::iterator it = program::get_shaders().find(program_name);
		if(it == program::get_shaders().end()) {
			program::get_shaders()[program_name] = fixed_program_ptr(new fixed_program(program_name, v_shader, f_shader));
		} else {
			it->second->init(program_name, v_shader, f_shader);
		}
		program::get_shaders()[program_name]->set_fixed_attributes(prog);
		program::get_shaders()[program_name]->set_fixed_uniforms(uniforms);
	}

	void fixed_program::set_fixed_attributes(const variant& node)
	{
		program::set_fixed_attributes(node);
		std::cerr << "shader program: " << name() << "(" << get() << ")";
		if(node.has_key("vertex")) {
			vtx_coord_ = get_attribute(node["vertex"].as_string());
			std::cerr << ", vtx_coord: " << vtx_coord_;
		} 
		if(node.has_key("color")) {
			col_coord_ = get_attribute(node["color"].as_string());
			std::cerr << ", col_coord: " << col_coord_;
		} 
		if(node.has_key("colour")) {
			col_coord_ = get_attribute(node["colour"].as_string());
			std::cerr << ", col_coord: " << col_coord_;
		} 
		if(node.has_key("texcoord")) {
			tex_coord_[0] = get_attribute(node["texcoord"].as_string());
			std::cerr << ", tex_coord0: " << tex_coord_[0];
		} 
		if(node.has_key("texcoord0")) {
			tex_coord_[0] = get_attribute(node["texcoord0"].as_string());
			std::cerr << ", tex_coord0: " << tex_coord_[0];
		} 
		if(node.has_key("texcoord1")) {
			tex_coord_[1] = get_attribute(node["texcoord1"].as_string());
			std::cerr << ", tex_coord1: " << tex_coord_[1];
		}
		std::cerr << std::endl;
	}

	shader_program_ptr get_tex_shader()
	{
		return tex_shader_program;
	}

	shader_program_ptr get_texcol_shader()
	{
		return texcol_shader_program;
	}

	shader_program_ptr get_simple_shader()
	{
		return simple_shader_program;
	}

	shader_program_ptr get_simple_col_shader()
	{
		return simple_col_shader_program;
	}

	shader_program_ptr active_shader()
	{
		return active_shader_program;
	}

	const glm::mat4& get_mvp_matrix()
	{
		static glm::mat4 mvp = glm::mat4();
	#if !defined(GL_ES_VERSION_2_0)
		glGetFloatv(GL_MODELVIEW_MATRIX, glm::value_ptr(modelview_matrix));
		glGetFloatv(GL_PROJECTION_MATRIX, glm::value_ptr(proj_matrix));
	#endif
		mvp = proj_matrix * modelview_matrix;
		return mvp;
	}

	namespace {
	bool g_alpha_test = false;
	}

	void set_alpha_test(bool value) {
		g_alpha_test = value;
	}

	bool get_alpha_test() {
		return g_alpha_test;
	}

	GLfloat get_alpha()
	{
		return colors[3];
	}

	GLfloat* get_color()
	{
		return colors;
	}

	GLfloat get_point_size()
	{
#if defined(GL_ES_VERSION_2_0)
		return point_size;
#else
		GLfloat pt_size;
		glGetFloatv(GL_POINT_SIZE, &pt_size);
		return pt_size;
#endif
	}

	void init_default_shader()
	{
		gles2::shader v1(GL_VERTEX_SHADER, "simple_vertex_shader", variant(vs1));
		gles2::shader f1(GL_FRAGMENT_SHADER, "simple_fragment_shader", variant(fs1));
		variant sai = json::parse(simple_attribute_info);
		fixed_program::add_shader("simple_shader", v1, f1, sai["attributes"], sai["uniforms"]);
		simple_shader_program.reset(new shader_program());
		simple_shader_program->configure(json::parse(simple_shader_info)["shader"]);
		simple_shader_program->init(0);

		gles2::shader v1_col(GL_VERTEX_SHADER, "simple_col_vertex_shader", variant(vs_col));
		gles2::shader f1_col(GL_FRAGMENT_SHADER, "simple_col_fragment_shader", variant(fs_col));
		variant scs = json::parse(simple_col_attribute_info);
		fixed_program::add_shader("simple_col_shader", v1_col, f1_col, scs["attributes"], scs["uniforms"]);
		simple_col_shader_program.reset(new shader_program());
		simple_col_shader_program->configure(json::parse(simple_col_shader_info)["shader"]);
		simple_col_shader_program->init(0);

		gles2::shader v_tex(GL_VERTEX_SHADER, "tex_vertex_shader", variant(vs_tex));
		gles2::shader f_tex(GL_FRAGMENT_SHADER, "tex_fragment_shader", variant(fs_tex));
		variant ts = json::parse(tex_attribute_info);
		fixed_program::add_shader("tex_shader", v_tex, f_tex, ts["attributes"], ts["uniforms"]);
		tex_shader_program.reset(new shader_program());
		tex_shader_program->configure(json::parse(tex_shader_info)["shader"]);
		tex_shader_program->init(0);

		gles2::shader v_texcol(GL_VERTEX_SHADER, "texcol_vertex_shader", variant(vs_texcol));
		gles2::shader f_texcol(GL_FRAGMENT_SHADER, "texcol_fragment_shader", variant(fs_texcol));
		variant tcs = json::parse(texcol_attribute_info);
		fixed_program::add_shader("texcol_shader", v_texcol, f_texcol, tcs["attributes"], tcs["uniforms"]);
		texcol_shader_program.reset(new shader_program());
		texcol_shader_program->configure(json::parse(texcol_shader_info)["shader"]);
		texcol_shader_program->init(0);

		matrix_mode = GL_PROJECTION;
		p_mat_stack.empty();
		mv_mat_stack.empty();
		proj_matrix = glm::mat4(1.0f);
		modelview_matrix = glm::mat4(1.0f);
		colors[0] = 1.0f;
		colors[1] = 1.0f;
		colors[2] = 1.0f;
		colors[3] = 1.0f;
		glActiveTexture(GL_TEXTURE0);

		std::string shader_file = "data/shaders.cfg";
		if(sys::file_exists(shader_file)) {
			program::load_shaders(sys::read_file(shader_file));
		}

		shader_file = module::map_file("data/shaders.cfg");
		if(sys::file_exists(shader_file)) {
			program::load_shaders(sys::read_file(shader_file));
		}
		active_shader_program = tex_shader_program;
	}

	manager::manager(shader_program_ptr shader)
	{
		// Reset errors, so we can track errors that happened here.
		glGetError();

		GLint blend_src_mode;
		GLint blend_dst_mode;
		// Save current blend mode
		glGetIntegerv(GL_BLEND_SRC, &blend_src_mode);
		glGetIntegerv(GL_BLEND_DST, &blend_dst_mode);
		blend_stack.push(blend_mode(blend_src_mode, blend_dst_mode, glIsEnabled(GL_BLEND) != 0));

		GLint atu;
		glGetIntegerv(GL_ACTIVE_TEXTURE, &atu);
		active_texture_unit.push(atu);

		if(shader == NULL || active_shader_program == shader) {
			return;
		}	

		if(active_shader_program != shader) {
			shader_stack.push(active_shader_program);
			active_shader_program = shader;
		}
		ASSERT_LOG(active_shader_program != NULL, "Active shader was NULL");
		//check_gl_errors();
		active_shader_program->prepare_draw();

		//GLenum err = glGetError();
		//ASSERT_LOG(err == GL_NONE, "Error in shader code: " << shader->name() << " : 0x" << std::hex << err << ": " << gl_error_to_string(err));
	}

	manager::~manager()
	{
		blend_mode bm(blend_stack.top());
		blend_stack.pop();
		if(bm.blend_enabled) {
			glEnable(GL_BLEND);
		} else {
			glDisable(GL_BLEND);
		}
		glBlendFunc(bm.blend_src_mode, bm.blend_dst_mode);
		glActiveTexture(active_texture_unit.top());
		active_texture_unit.pop();

		active_shader_program->shader()->disable_vertex_attrib(-1);
		if(shader_stack.empty() == false) {
			active_shader_program = shader_stack.top();
			shader_stack.pop();
		} else {
			active_shader_program = tex_shader_program;
		}
		glUseProgram(active_shader_program->shader()->get());
	}
}

#endif
