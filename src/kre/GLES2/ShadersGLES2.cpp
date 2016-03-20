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

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "asserts.hpp"
#include "AttributeSet.hpp"
#include "DisplayDevice.hpp"
#include "ShadersGLES2.hpp"
#include "TextureGLES2.hpp"
#include "UniformBufferGLES2.hpp"

namespace KRE
{
	namespace GLESv2
	{
		namespace
		{
			struct uniform_mapping { const char* alt_name; const char* name; };
			struct attribute_mapping { const char* alt_name; const char* name; };

			const char* const default_vs = 
				"uniform mat4 u_mvp_matrix;\n"
				"attribute vec2 a_position;\n"
				"attribute vec2 a_texcoord;\n"
				"varying vec2 v_texcoord;\n"
				"void main()\n"
				"{\n"
				"    v_texcoord = a_texcoord;\n"
				"    gl_Position = u_mvp_matrix * vec4(a_position,0.0,1.0);\n"
				"}\n";
			const char* const default_fs =
				"precision mediump float;\n"
				"uniform sampler2D u_tex_map;\n"
				"uniform sampler2D u_palette_map;\n"
				"uniform bool u_enable_palette_lookup;\n"
				"uniform float u_palette[2];\n"
				"uniform float u_palette_width;\n"
				"uniform bool u_discard;\n"
				"uniform bool u_mix_palettes;\n"
				"uniform float u_mix;\n"
				"uniform vec4 u_color;\n"
				"varying vec2 v_texcoord;\n"
				"void main()\n"
				"{\n"
				"    vec4 color1 = texture2D(u_tex_map, v_texcoord);\n"
				"    if(u_enable_palette_lookup) {\n"
				"        color1 = texture2D(u_palette_map, vec2(255.0 * color1.r / (u_palette_width-0.5), u_palette[0]));\n"
				"        if(u_mix_palettes) {\n"
				"            vec4 color2 = texture2D(u_palette_map, vec2(255.0 * color1.r / (u_palette_width-0.5), u_palette[1]));\n"
				"            color1 = mix(color1, color2, u_mix);\n"
				"        }\n"
				"    }\n"
				"    if(u_discard && color1[3] == 0.0) {\n"
				"        discard;\n"
				"    } else {\n"
				"        gl_FragColor = color1 * u_color;\n"
				"    }\n"
				"}\n";

			const uniform_mapping default_uniform_mapping[] =
			{
				{"mvp_matrix", "u_mvp_matrix"},
				{"color", "u_color"},
				{"discard", "u_discard"},
				{"tex_map", "u_tex_map"},
				{"palette", "u_palette"},
				{"palette_width", "u_palette_width"},
				{"palette_map", "u_palette_map"},
				{"enable_palette_lookup", "u_enable_palette_lookup"},
				{"tex_map0", "u_tex_map"},
				{"", ""},
			};
			const attribute_mapping default_attribue_mapping[] =
			{
				{"position", "a_position"},
				{"texcoord", "a_texcoord"},
				{"", ""},
			};

			const char* const simple_vs = 
				"uniform mat4 u_mvp_matrix;\n"
				"uniform float u_point_size;\n"
				"attribute vec2 a_position;\n"
				"void main()\n"
				"{\n"
				"    gl_PointSize = u_point_size;\n"
				"    gl_Position = u_mvp_matrix * vec4(a_position, 0.0, 1.0);\n"
				"}\n";
			const char* const simple_fs =
				"precision mediump float;\n"
				"uniform bool u_discard;\n"
				"uniform vec4 u_color;\n"
				"void main()\n"
				"{\n"
				"    gl_FragColor = u_color;\n"
				"    if(u_discard && gl_FragColor[3] == 0.0) {\n"
				"        discard;\n"
				"    }\n"
				"}\n";

			const uniform_mapping simple_uniform_mapping[] =
			{
				{"mvp_matrix", "u_mvp_matrix"},
				{"color", "u_color"},
				{"discard", "u_discard"},
				{"point_size", "u_point_size"},
				{"", ""},
			};
			const attribute_mapping simple_attribue_mapping[] =
			{
				{"position", "a_position"},
				{"", ""},
			};

			// circle shader definition starts
			const char* const circle_vs = 
				"uniform mat4 u_mvp_matrix;\n"
				"attribute vec2 a_position;\n"
				"varying vec2 v_position;\n"
				"void main()\n"
				"{\n"
				"	gl_Position = u_mvp_matrix * vec4(a_position, 0.0, 1.0);\n"
				"	v_position = a_position;\n"
				"}\n";
			const char* const circle_fs =
				"precision mediump float;\n"
				"uniform bool u_discard;\n"
				"uniform vec4 u_color;\n"
				"uniform float u_outer_radius;\n"
				"uniform float u_inner_radius;\n"
				"uniform vec2 u_centre;\n"
				"uniform vec2 u_screen_dimensions;\n"
				"varying vec2 v_position;\n"
				"void main()\n"
				"{\n"
				// This adjusts for gl_FragCoord being origin at the bottom-left of the screen.
				"	vec2 pos = vec2(gl_FragCoord.x, u_screen_dimensions.y - gl_FragCoord.y) - u_centre;\n"
				"	float dist_squared = dot(pos, pos);\n"
				"	float r_squared = u_outer_radius*u_outer_radius;\n"
				"	if(u_inner_radius > 0.0 && dist_squared < u_inner_radius*u_inner_radius) {\n"
				"		gl_FragColor = mix(vec4(u_color.rgb, 0.0), u_color, smoothstep(u_inner_radius*u_inner_radius-u_inner_radius-0.25, u_inner_radius*u_inner_radius+u_inner_radius-0.25, dist_squared));\n"
				"	} else if(dist_squared < r_squared) {\n"
				"		gl_FragColor = mix(u_color, vec4(u_color.rgb, 0.0), smoothstep(r_squared-u_outer_radius+0.25, r_squared+u_outer_radius+0.25, dist_squared));\n"
				"	} else {\n"
				"		discard;\n"
				"	}\n"
				"}\n";

			const uniform_mapping circle_uniform_mapping[] =
			{
				{"mvp_matrix", "u_mvp_matrix"},
				{"color", "u_color"},
				{"discard", "u_discard"},
				{"outer_radius", "u_outer_radius"},
				{"inner_radius", "u_inner_radius"},
				{"screen_dimensions", "u_screen_dimensions"},
				{"centre", "u_centre"},
				{"", ""},
			};
			const attribute_mapping circle_attribue_mapping[] =
			{
				{"position", "a_position"},
				{"", ""},
			};
			// circle shader definition ends

			const char* const complex_vs = 
				"precision mediump float;\n"
				"uniform mat4 u_mv_matrix;\n"
				"uniform mat4 u_p_matrix;\n"
				"uniform float u_point_size;\n"
				"uniform float u_line_width;\n"
				"attribute vec2 a_position;\n"
				"attribute vec2 a_normal;\n"
				"varying vec2 v_normal;\n"
				"void main()\n"
				"{\n"
				"    gl_PointSize = u_point_size;\n"
				"    vec4 delta = vec4(a_normal * u_line_width, 0.0, 0.0);\n"
				"    vec4 pos = u_mv_matrix * vec4(a_position, 0.0, 1.0);\n"
				"    gl_Position = u_p_matrix * (pos + delta);\n"
				"    v_normal = a_normal;\n"
				"}\n";
			const char* const complex_fs =
				"precision mediump float;\n"
				"uniform bool u_discard;\n"
				"uniform vec4 u_color;\n"
				"uniform float u_line_width;\n"
				"uniform float u_blur;\n"
				"varying vec2 v_normal;\n"
				"void main()\n"
				"{\n"
				"    float blur = 2.0;\n"
				"    float dist = length(v_normal) * u_line_width;\n"
				"    float alpha = clamp((u_line_width - dist) / u_blur, 0.0, 1.0);\n"
				"    gl_FragColor = vec4(u_color.rgb, alpha);\n"
				"    if(u_discard && gl_FragColor[3] == 0.0) {\n"
				"        discard;\n"
				"    }\n"
				"}\n";

			const uniform_mapping complex_uniform_mapping[] =
			{
				{"mv_matrix", "u_mv_matrix"},
				{"p_matrix", "u_p_matrix"},
				{"color", "u_color"},
				{"discard", "u_discard"},
				{"point_size", "u_point_size"},
				{"line_width", "u_line_width"},
				{"", ""},
			};
			const attribute_mapping complex_attribue_mapping[] =
			{
				{"position", "a_position"},
				{"normal", "a_normal"},
				{"", ""},
			};

			const char* const attr_color_vs = 
				"uniform mat4 u_mvp_matrix;\n"
				"uniform float u_point_size;\n"
				"attribute vec2 a_position;\n"
				"attribute vec4 a_color;\n"
				"varying vec4 v_color;\n"
				"void main()\n"
				"{\n"
				"	 v_color = a_color;\n"
				"    gl_PointSize = u_point_size;\n"
				"    gl_Position = u_mvp_matrix * vec4(a_position,0.0,1.0);\n"
				"}\n";
			const char* const attr_color_fs =
				"precision mediump float;\n"
				"uniform bool u_discard;\n"
				"uniform vec4 u_color;\n"
				"varying vec4 v_color;\n"
				"void main()\n"
				"{\n"
				"    gl_FragColor = v_color * u_color;\n"
				"    if(u_discard && gl_FragColor[3] == 0.0) {\n"
				"        discard;\n"
				"    }\n"
				"}\n";

			const uniform_mapping attr_color_uniform_mapping[] =
			{
				{"mvp_matrix", "u_mvp_matrix"},
				{"color", "u_color"},
				{"discard", "u_discard"},
				{"point_size", "u_point_size"},
				{"", ""},
			};
			const attribute_mapping attr_color_attribue_mapping[] =
			{
				{"position", "a_position"},
				{"color", "a_color"},
				{"", ""},
			};

			const char* const vtc_vs = 
				"uniform mat4 u_mvp_matrix;\n"
				"attribute vec2 a_position;\n"
				"attribute vec2 a_texcoord;\n"
				"attribute vec4 a_color;\n"
				"varying vec2 v_texcoord;\n"
				"varying vec4 v_color;\n"
				"void main()\n"
				"{\n"
				"    v_color = a_color;\n"
				"    v_texcoord = a_texcoord;\n"
				"    gl_Position = u_mvp_matrix * vec4(a_position,0.0,1.0);\n"
				"}\n";
			const char* const vtc_fs =
				"precision mediump float;\n"
				"uniform sampler2D u_tex_map;\n"
				"varying vec2 v_texcoord;\n"
				"varying vec4 v_color;\n"
				"uniform vec4 u_color;\n"
				"void main()\n"
				"{\n"
				"    vec4 color = texture2D(u_tex_map, v_texcoord);\n"
				"    gl_FragColor = color * v_color * u_color;\n"
				"}\n";

			const uniform_mapping vtc_uniform_mapping[] =
			{
				{"mvp_matrix", "u_mvp_matrix"},
				{"color", "u_color"},
				{"tex_map", "u_tex_map"},
				{"tex_map0", "u_tex_map"},
				{"", ""},
			};
			const attribute_mapping vtc_attribue_mapping[] =
			{
				{"position", "a_position"},
				{"texcoord", "a_texcoord"},
				{"color", "a_color"},
				{"", ""},
			};

			const char* const point_shader_vs = 
				"uniform mat4 u_mvp_matrix;\n"
				"uniform float u_point_size;\n"
				"attribute vec2 a_position;\n"
				"void main()\n"
				"{\n"
				"    gl_PointSize = u_point_size;\n"
				"    gl_Position = u_mvp_matrix * vec4(a_position,0.0,1.0);\n"
				"}\n";
			const char* const point_shader_fs = 
				"precision mediump float;\n"
				"uniform vec4 u_color;\n"
				"uniform bool u_is_circular;\n"
				"void main()\n"
				"{\n"
				"	 if(u_is_circular && length(gl_PointCoord - vec2(0.5, 0.5)) > 0.5) {\n"
				"        discard;\n"
				"    }\n"
				"    gl_FragColor = u_color;\n"
				"}\n";
			const uniform_mapping point_shader_uniform_mapping[] = 
			{
				{"mvp_matrix", "u_mvp_matrix"},
				{"color", "u_color"},
				{"point_size", "u_point_size"},
				{"", ""},
			};
			const attribute_mapping point_shader_attribute_mapping[] = 
			{
				{"position", "a_position"},
				{"", ""},
			};

			const char* const font_shader_vs = 
				"uniform mat4 u_mvp_matrix;\n"
				"attribute vec2 a_position;\n"
				"attribute vec2 a_texcoord;\n"
				"varying vec2 v_texcoord;\n"
				"void main()\n"
				"{\n"
				"    v_texcoord = a_texcoord;\n"
				"    gl_Position = u_mvp_matrix * vec4(a_position,0.0,1.0);\n"
				"}\n";
			const char* const font_shader_fs = 
				"precision mediump float;\n"
				"uniform sampler2D u_tex_map;\n"
				"uniform vec4 u_color;\n"
				"uniform bool ignore_alpha;\n"
				"varying vec2 v_texcoord;\n"
				"void main()\n"
				"{\n"
				"    vec4 color = vec4(1.0, 1.0, 1.0, texture2D(u_tex_map, v_texcoord).r);\n"
				"    if(ignore_alpha && color.a > 0.0) {\n"
				"	     color.a = 1.0;\n"
				"    }\n"
				"    gl_FragColor = color * u_color;\n"
				"}\n";
			const uniform_mapping font_shader_uniform_mapping[] = 
			{
				{"mvp_matrix", "u_mvp_matrix"},
				{"color", "u_color"},
				{"tex_map", "u_tex_map"},
				{"", ""},
			};
			const attribute_mapping font_shader_attribute_mapping[] = 
			{
				{"position", "a_position"},
				{"texcoord", "a_texcoord"},
				{"", ""},
			};

			const char* const blur_vs =
				"uniform mat4 u_mvp_matrix;\n"
				"attribute vec2 a_position;\n"
				"attribute vec2 a_texcoord;\n"
				"varying vec2 v_texcoords;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    gl_Position = u_mvp_matrix * vec4(a_position, 0.0, 1.0);\n"
				"    v_texcoords = a_texcoord;\n"
				"}\n";
			const char* const blur7_fs =
				"precision mediump float;\n"
				"uniform sampler2D u_tex_map;\n"
				"uniform float texel_width_offset;\n"
				"uniform float texel_height_offset;\n"
				"uniform vec4 u_color;\n"
				"uniform float gaussian[15];\n"
				"varying vec2 v_texcoords;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    vec4 sum = vec4(0.0, 0.0, 0.0, 0.0);\n"
				"    vec2 step_offset = vec2(texel_width_offset, texel_height_offset);\n"
				"    for(int index = 0; index < 15; ++index) {\n"
				"		vec2 offs = step_offset * vec2(index - 7, index - 7);\n"
				"        sum += texture2D(u_tex_map, v_texcoords + offs) * gaussian[index];\n"
				"    }\n"
				"    gl_FragColor = sum * u_color;\n"
				"}\n";
			const uniform_mapping blur_uniform_mapping[] = 
			{
				{"mvp_matrix", "u_mvp_matrix"},
				{"tex_map", "u_tex_map"},
				{"color", "u_color"},
				{"", ""},
			};
			const attribute_mapping blur_attribute_mapping[] = 
			{
				{"position", "a_position"},
				{"texcoord", "a_texcoord"},
				{"", ""},
			};


			const struct {
				const char* shader_name;
				const char* vertex_shader_name;
				const char* const vertex_shader_data;
				const char* fragment_shader_name;
				const char* const fragment_shader_data;
				const uniform_mapping* u_mapping;
				const attribute_mapping* a_mapping;
			} shader_defs[] = 
			{
				{ "default", "default_vs", default_vs, "default_fs", default_fs, default_uniform_mapping, default_attribue_mapping },
				{ "simple", "simple_vs", simple_vs, "simple_fs", simple_fs, simple_uniform_mapping, simple_attribue_mapping },
				{ "complex", "complex_vs", complex_vs, "complex_fs", complex_fs, complex_uniform_mapping, complex_attribue_mapping },
				{ "attr_color_shader", "attr_color_vs", attr_color_vs, "attr_color_fs", attr_color_fs, attr_color_uniform_mapping, attr_color_attribue_mapping },
				{ "vtc_shader", "vtc_vs", vtc_vs, "vtc_fs", vtc_fs, vtc_uniform_mapping, vtc_attribue_mapping },
				{ "circle", "circle_vs", circle_vs, "circle_fs", circle_fs, circle_uniform_mapping, circle_attribue_mapping },
				{ "point_shader", "point_shader_vs", point_shader_vs, "point_shader_fs", point_shader_fs, point_shader_uniform_mapping, point_shader_attribute_mapping },
				{ "font_shader", "font_shader_vs", font_shader_vs, "font_shader_fs", font_shader_fs, font_shader_uniform_mapping, font_shader_attribute_mapping },
				{ "blur7", "blur_vs", blur_vs, "blur7_fs", blur7_fs, blur_uniform_mapping, blur_attribute_mapping },
			};

			typedef std::map<std::string, ShaderProgramPtr> shader_factory_map;
			shader_factory_map& get_shader_factory()
			{
				static shader_factory_map res;
				if(res.empty()) {
					// XXX load some default shaders here.
					for(auto& def : shader_defs) {
						
						auto spp = std::make_shared<GLESv2::ShaderProgram>(def.shader_name, 
							ShaderDef(def.vertex_shader_name, def.vertex_shader_data),
							ShaderDef(def.fragment_shader_name, def.fragment_shader_data),
							variant());
						res[def.shader_name] = spp;
						auto um = def.u_mapping;
						while(strlen(um->alt_name) > 0) {
							spp->setAlternateUniformName(um->name, um->alt_name);
							++um;
						}
						auto am = def.a_mapping;
						while(strlen(am->alt_name) > 0) {
							spp->setAlternateAttributeName(am->name, am->alt_name);
							++am;
						}
						spp->setActives();
					}
				}
				return res;
			}

			GLenum convert_render_variable_type(AttrFormat type)
			{
				switch(type) {
					case AttrFormat::BOOL:							return GL_BYTE;
					case AttrFormat::FLOAT:							return GL_FLOAT;
					case AttrFormat::FIXED:							return GL_FIXED;
					case AttrFormat::SHORT:							return GL_SHORT;
					case AttrFormat::UNSIGNED_SHORT:				return GL_UNSIGNED_SHORT;
					case AttrFormat::BYTE:							return GL_BYTE;
					case AttrFormat::UNSIGNED_BYTE:					return GL_UNSIGNED_BYTE;
					case AttrFormat::INT:							return GL_INT;
					case AttrFormat::UNSIGNED_INT:					return GL_UNSIGNED_INT;
				}
				ASSERT_LOG(false, "Unrecognised value for render variable type: " << static_cast<int>(type));
				return GL_NONE;
			}

			GLuint& get_current_active_shader()
			{
				static GLuint res = -1;
				return res;
			}

			GLenum get_shader_type(ProgramType type)
			{
				switch(type) {
					case ProgramType::VERTEX:						return GL_VERTEX_SHADER;
					case ProgramType::FRAGMENT:						return GL_FRAGMENT_SHADER;
					default: break;
				}
				ASSERT_LOG(false, "Unrecognised value for shader type: " << static_cast<int>(type));
				return GL_NONE;
			}

			std::string get_shader_type_abbrev(ProgramType type)
			{
				switch(type) {
					case ProgramType::VERTEX:						return "vs";
					case ProgramType::FRAGMENT:						return "fs";
					default: break;
				}
				ASSERT_LOG(false, "Unrecognised value for shader type: " << static_cast<int>(type));
				return "none";
			}
		}

		Shader::Shader(GLenum type, const std::string& name, const std::string& code)
			: type_(type), 
			  shader_(0), 
			  name_(name)
		{
			std::string working_version_str;

			bool compiled_ok = compile(code);
			if(compiled_ok == false && code.find("#version") == std::string::npos) {
				for(int n = 120; n <= 150; n += 10) {
					std::stringstream versioned_code;
					versioned_code << "#version " << n << "\n" << code;
					const bool result = compile(versioned_code.str());
					if(result) {
						LOG_WARN("Auto-added '#version " << n << "' to the top of " << name_ << " shader to make it work.");
						compiled_ok = true;
						break;
					}
				}
			}
			ASSERT_LOG(compiled_ok == true, "Error compiling shader for " << name_ << working_version_str);
		}

		bool Shader::compile(const std::string& code)
		{
			GLint compiled;
			if(shader_) {
				glDeleteShader(shader_);
				shader_ = 0;
			}

			shader_ = glCreateShader(type_);
			if(shader_ == 0) {
				LOG_ERROR("Unable to create shader: " << name() << ", glGetError(): 0x" << std::hex << glGetError());
				return false;
			}
			const char* shader_code = code.c_str();
			glShaderSource(shader_, 1, &shader_code, nullptr);
			glCompileShader(shader_);
			glGetShaderiv(shader_, GL_COMPILE_STATUS, &compiled);
			if(!compiled) {
				GLint info_len = 0;
				glGetShaderiv(shader_, GL_INFO_LOG_LENGTH, &info_len);
				if(info_len > 1) {
					std::vector<char> info_log;
					info_log.resize(info_len);
					glGetShaderInfoLog(shader_, static_cast<GLsizei>(info_log.capacity()), nullptr, &info_log[0]);
					std::string s(info_log.begin(), info_log.end());
					LOG_ERROR("Error compiling shader(" << name() << "): " << s);
				}
				glDeleteShader(shader_);
				shader_ = 0;
				return false;
			}
			return true;
		}

		ShaderProgram::ShaderProgram(const std::string& name, const ShaderDef& vs, const ShaderDef& fs, const variant& node)
			: KRE::ShaderProgram(name, node),
              name_(name),
			  object_(0),
              attribs_(),
              uniforms_(),
              v_uniforms_(),
              v_attribs_(),
              uniform_alternate_name_map_(),
              attribute_alternate_name_map_(),
			  u_mvp_(-1),
			  u_mv_(-1),
			  u_p_(-1),
			  u_color_(-1),
			  u_line_width_(-1),
			  u_tex_(-1),
			  a_vertex_(-1),
			  a_texcoord_(-1),
			  a_color_(-1),
			  a_normal_(-1),              
			  u_enable_palette_lookup_(-1),
			  u_palette_(-1),
			  u_palette_width_(-1),
			  u_palette_map_(-1),
			  u_mix_palettes_(-1),
			  u_mix_(-1),
			  enabled_attribs_()
		{
			init(name, vs, fs);
		}

		ShaderProgram::ShaderProgram(const std::string& name, 
			const std::vector<ShaderData>& shader_data, 
			const std::vector<ActiveMapping>& uniform_map, 
			const std::vector<ActiveMapping>& attribute_map)
			: KRE::ShaderProgram(name, variant()),
			  name_(name),
			  object_(0),
              attribs_(),
              uniforms_(),
              v_uniforms_(),
              v_attribs_(),
              uniform_alternate_name_map_(),
              attribute_alternate_name_map_(),
			  u_mvp_(-1),
			  u_mv_(-1),
			  u_p_(-1),
			  u_color_(-1),
			  u_line_width_(-1),
			  u_tex_(-1),
			  a_vertex_(-1),
			  a_texcoord_(-1),
			  a_color_(-1),
			  a_normal_(-1),              
			  u_enable_palette_lookup_(-1),
			  u_palette_(-1),
			  u_palette_width_(-1),
			  u_palette_map_(-1),
			  u_mix_palettes_(-1),
			  u_mix_(-1),
			  enabled_attribs_()
		{
			std::vector<Shader> shader_programs;
			for(auto& sd : shader_data) {
				shader_programs.emplace_back(get_shader_type(sd.type), name + "-" + get_shader_type_abbrev(sd.type), sd.shader_data);
			}
			bool linked_ok = link(shader_programs);
			ASSERT_LOG(linked_ok == true, "Error linking program: " << name_);
			
			for(auto& um : uniform_map) {
				setAlternateUniformName(um.name, um.alt_name);
			}

			for(auto& am : attribute_map) {
				setAlternateAttributeName(am.name, am.alt_name);
			}
		}

		ShaderProgram::~ShaderProgram()
		{
			//if(object_ != 0) {
			//	glDeleteShader(object_);
			//	object_ = 0;
			//}
		}

		void ShaderProgram::init(const std::string& name, const ShaderDef& vs, const ShaderDef& fs)
		{
			//vs_.reset(new Shader(GL_VERTEX_SHADER, vs.first, vs.second));
			//fs_.reset(new Shader(GL_FRAGMENT_SHADER, fs.first, fs.second));
			std::vector<Shader> shader_programs;
			shader_programs.emplace_back(GL_VERTEX_SHADER, vs.first, vs.second);
			shader_programs.emplace_back(GL_FRAGMENT_SHADER, fs.first, fs.second);
			bool linked_ok = link(shader_programs);
			ASSERT_LOG(linked_ok == true, "Error linking program: " << name_);
		}

		int ShaderProgram::getAttributeOrDie(const std::string& attr) const
		{
			int attr_value = getAttribute(attr);
			ASSERT_LOG(attr_value != ShaderProgram::INVALID_ATTRIBUTE, "Could not find attribute '" << attr << "' in shader: " << name());
			return attr_value;
		}

		int ShaderProgram::getUniformOrDie(const std::string& attr) const
		{
			int uniform_value = getUniform(attr);
			ASSERT_LOG(uniform_value != ShaderProgram::INVALID_UNIFORM, "Could not find uniform '" << attr << "' in shader: " << name());
			return uniform_value;
		}

		int ShaderProgram::getAttribute(const std::string& attr) const
		{
			auto it = attribs_.find(attr);
			if(it != attribs_.end()) {
				return it->second.location;
			}
			auto alt_name_it = attribute_alternate_name_map_.find(attr);
			if(alt_name_it == attribute_alternate_name_map_.end()) {
				//LOG_WARN("Attribute '" << attr << "' not found in alternate names list and is not a name defined in the shader: " << name_);
				return ShaderProgram::INVALID_ATTRIBUTE;
			}
			it = attribs_.find(alt_name_it->second);
			if(it == attribs_.end()) {
				//LOG_WARN("Attribute \"" << alt_name_it->second << "\" not found in list, looked up from symbol " << attr << " in shader: " << name_);
				return ShaderProgram::INVALID_ATTRIBUTE;
			}
			return it->second.location;
		}

		int ShaderProgram::getUniform(const std::string& attr) const
		{
			auto it = uniforms_.find(attr);
			if(it != uniforms_.end()) {
				return it->second.location;
			}
			auto alt_name_it = uniform_alternate_name_map_.find(attr);
			if(alt_name_it == uniform_alternate_name_map_.end()) {
				//LOG_WARN("Uniform '" << attr << "' not found in alternate names list and is not a name defined in the shader: " << name_);
				return ShaderProgram::INVALID_UNIFORM;
			}
			it = uniforms_.find(alt_name_it->second);
			if(it == uniforms_.end()) {
				//LOG_WARN("Uniform \"" << alt_name_it->second << "\" not found in list, looked up from symbol " << attr << " in shader: " << name_);
				return ShaderProgram::INVALID_UNIFORM;
			}
			return it->second.location;
		}

		bool ShaderProgram::link(const std::vector<Shader>& shader_programs)
		{
			if(object_) {
				glDeleteProgram(object_);
				object_ = 0;
			}
			object_ = glCreateProgram();
			ASSERT_LOG(object_ != 0, "Unable to create program object.");
			for(auto sp : shader_programs) {
				glAttachShader(object_, sp.get());
			}
			glLinkProgram(object_);
			GLint linked = 0;
			glGetProgramiv(object_, GL_LINK_STATUS, &linked);
			if(!linked) {
				GLint info_len = 0;
				glGetProgramiv(object_, GL_INFO_LOG_LENGTH, &info_len);
				if(info_len > 1) {
					std::vector<char> info_log;
					info_log.resize(info_len);
					glGetProgramInfoLog(object_, static_cast<GLsizei>(info_log.capacity()), nullptr, &info_log[0]);
					std::string s(info_log.begin(), info_log.end());
					LOG_ERROR("Error linking object: " << s);
				}
				glDeleteProgram(object_);
				object_ = 0;
				return false;
			}
			return queryUniforms() && queryAttributes();
		}

		bool ShaderProgram::queryUniforms()
		{
			GLint active_uniforms;
			glGetProgramiv(object_, GL_ACTIVE_UNIFORMS, &active_uniforms);
			GLint uniform_max_len;
			glGetProgramiv(object_, GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniform_max_len);
			std::vector<char> name;
			name.resize(uniform_max_len+1);
			LOG_DEBUG("actives(uniforms) for shader: " << name_);
			for(int i = 0; i < active_uniforms; i++) {
				Actives u;
				GLsizei size;
				glGetActiveUniform(object_, i, static_cast<GLsizei>(name.size()), &size, &u.num_elements, &u.type, &name[0]);
				u.name = std::string(&name[0], &name[size]);

				// Some drivers add a [0] on the end of array uniform names.
				// Strip the [0] off the end here.
				if(u.name.size() > 3 && std::equal(u.name.end()-3,u.name.end(),"[0]")) {
					u.name.resize(u.name.size()-3);
				}
		
				u.location = glGetUniformLocation(object_, u.name.c_str());
				ASSERT_LOG(u.location >= 0, "Unable to determine the location of the uniform: " << u.name);
				uniforms_[u.name] = u;
				v_uniforms_[u.location] = u;
				LOG_DEBUG("    " << u.name << " loc: " << u.location << ", num elements: " << u.num_elements << ", type: " << u.type);
			}
			return true;
		}

		bool ShaderProgram::queryAttributes()
		{
			GLint active_attribs;
			glGetProgramiv(object_, GL_ACTIVE_ATTRIBUTES, &active_attribs);
			GLint attributes_max_len;
			glGetProgramiv(object_, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &attributes_max_len);
			std::vector<char> name;
			name.resize(attributes_max_len+1);
			for(int i = 0; i < active_attribs; i++) {
				Actives a;
				GLsizei size;
				glGetActiveAttrib(object_, i, static_cast<GLsizei>(name.size()), &size, &a.num_elements, &a.type, &name[0]);
				a.name = std::string(&name[0], &name[size]);
				a.location = glGetAttribLocation(object_, a.name.c_str());
				ASSERT_LOG(a.location >= 0, "Unable to determine the location of the attribute: " << a.name);
				ASSERT_LOG(a.num_elements == 1, "More than one element was found for an attribute(" << a.name << ") in shader(" << this->name() << "): " << a.num_elements);
				attribs_[a.name] = a;
				v_attribs_[a.location] = a;
			}
			return true;
		}

		void ShaderProgram::makeActive()
		{
			//if(get_current_active_shader() == object_) {
			//	return;
			//}
			glUseProgram(object_);
			get_current_active_shader() = object_;
		}

		void ShaderProgram::setUniformValue(int uid, const void* value) const
		{
			if(uid == ShaderProgram::INVALID_UNIFORM) {
				LOG_WARN("Tried to set value for invalid uniform iterator.");
				return;
			}
			auto it = v_uniforms_.find(uid);
			ASSERT_LOG(it != v_uniforms_.end(), "Couldn't find location " << uid << " on the uniform list.");
			const Actives& u = it->second;
			ASSERT_LOG(value != nullptr, "setUniformValue(): value is nullptr");
			switch(u.type) {
			case GL_INT:
			case GL_BOOL:
			case GL_SAMPLER_2D:
			case GL_SAMPLER_CUBE:	
				glUniform1i(u.location, *static_cast<const GLint*>(value)); 
				break;
			case GL_INT_VEC2:	
			case GL_BOOL_VEC2:	
				glUniform2i(u.location, (static_cast<const GLint*>(value))[0], (static_cast<const GLint*>(value))[1]); 
				break;
			case GL_INT_VEC3:	
			case GL_BOOL_VEC3:	
				glUniform3iv(u.location, u.num_elements, static_cast<const GLint*>(value)); 
				break;
			case GL_INT_VEC4: 	
			case GL_BOOL_VEC4:
				glUniform4iv(u.location, u.num_elements, static_cast<const GLint*>(value)); 
				break;

			case GL_FLOAT: {
				if(u.num_elements > 1) {
					glUniform1fv(u.location, u.num_elements, static_cast<const GLfloat*>(value));
				} else {
					glUniform1f(u.location, *static_cast<const GLfloat*>(value));
				}	
				break;
			}
			case GL_FLOAT_VEC2: {
				glUniform2fv(u.location, u.num_elements, static_cast<const GLfloat*>(value));
				break;
			}
			case GL_FLOAT_VEC3: {
				glUniform3fv(u.location, u.num_elements, static_cast<const GLfloat*>(value));
				break;
			}
			case GL_FLOAT_VEC4: {
				glUniform4fv(u.location, u.num_elements, static_cast<const GLfloat*>(value));
				break;
			}
			case GL_FLOAT_MAT2:	{
				glUniformMatrix2fv(u.location, u.num_elements, GL_FALSE, static_cast<const GLfloat*>(value));
				break;
			}
			case GL_FLOAT_MAT3: {
				glUniformMatrix3fv(u.location, u.num_elements, GL_FALSE, static_cast<const GLfloat*>(value));
				break;
			}
			case GL_FLOAT_MAT4: {
				glUniformMatrix4fv(u.location, u.num_elements, GL_FALSE, static_cast<const GLfloat*>(value));
				break;
			}
			default:
				ASSERT_LOG(false, "Unhandled uniform type: " << it->second.type);
			}
		}

		void ShaderProgram::setUniformValue(int uid, const GLint value) const
		{
			if(uid == ShaderProgram::INVALID_UNIFORM) {
				LOG_WARN("Tried to set value for invalid uniform iterator.");
				return;
			}
			auto it = v_uniforms_.find(uid);
			ASSERT_LOG(it != v_uniforms_.end(), "Couldn't find location " << uid << " on the uniform list.");
			const Actives& u = it->second;
			switch(u.type) {
			case GL_INT:
			case GL_BOOL:
			case GL_SAMPLER_2D:
			case GL_SAMPLER_CUBE:	
				glUniform1i(u.location, value); 
				break;
			case GL_FLOAT:
				glUniform1f(u.location, static_cast<float>(value));
				break;
			default:
				ASSERT_LOG(false, "Unhandled uniform type: " << it->second.type);
			}
		}

		void ShaderProgram::setUniformValue(int uid, const GLfloat value) const
		{
			if(uid == ShaderProgram::INVALID_UNIFORM) {
				LOG_WARN("Tried to set value for invalid uniform iterator.");
				return;
			}
			auto it = v_uniforms_.find(uid);
			ASSERT_LOG(it != v_uniforms_.end(), "Couldn't find location " << uid << " on the uniform list.");
			const Actives& u = it->second;
			switch(u.type) {
			case GL_FLOAT: {
				glUniform1f(u.location, value);
				break;
			}
			default:
				ASSERT_LOG(false, "Unhandled uniform type: " << it->second.type);
			}	
		}

		void ShaderProgram::setUniformValue(int uid, const GLint* value) const
		{
			if(uid == ShaderProgram::INVALID_UNIFORM) {
				LOG_WARN("Tried to set value for invalid uniform iterator.");
				return;
			}
			auto it = v_uniforms_.find(uid);
			ASSERT_LOG(it != v_uniforms_.end(), "Couldn't find location " << uid << " on the uniform list.");
			const Actives& u = it->second;
			ASSERT_LOG(value != nullptr, "set_uniform(): value is nullptr");
			switch(u.type) {
			case GL_INT:
			case GL_BOOL:
			case GL_SAMPLER_2D:
			case GL_SAMPLER_CUBE:	
				glUniform1i(u.location, *value); 
				break;
			case GL_INT_VEC2:	
			case GL_BOOL_VEC2:	
				glUniform2i(u.location, value[0], value[1]); 
				break;
			case GL_INT_VEC3:	
			case GL_BOOL_VEC3:	
				glUniform3iv(u.location, u.num_elements, value); 
				break;
			case GL_INT_VEC4: 	
			case GL_BOOL_VEC4:
				glUniform4iv(u.location, u.num_elements, value); 
				break;
			case GL_FLOAT:
				glUniform1f(u.location, static_cast<float>(*value));
				break;
			default:
				ASSERT_LOG(false, "Unhandled uniform type: " << it->second.type);
			}
		}

		void ShaderProgram::setUniformValue(int uid, const GLfloat* value) const
		{
			if(uid == ShaderProgram::INVALID_UNIFORM) {
				LOG_WARN("Tried to set value for invalid uniform iterator.");
				return;
			}
			auto it = v_uniforms_.find(uid);
			ASSERT_LOG(it != v_uniforms_.end(), "Couldn't find location " << uid << " on the uniform list.");
			const Actives& u = it->second;
			ASSERT_LOG(value != nullptr, "setUniformValue(): value is nullptr");
			switch(u.type) {
			case GL_FLOAT: {
				if(u.num_elements > 1) {
					glUniform1fv(u.location, u.num_elements, value);
				} else {
					glUniform1f(u.location, *value);
				}
				break;
			}
			case GL_FLOAT_VEC2: {
				glUniform2fv(u.location, u.num_elements, value);
				break;
			}
			case GL_FLOAT_VEC3: {
				glUniform3fv(u.location, u.num_elements, value);
				break;
			}
			case GL_FLOAT_VEC4: {
				glUniform4fv(u.location, u.num_elements, value);
				break;
			}
			case GL_FLOAT_MAT2:	{
				glUniformMatrix2fv(u.location, u.num_elements, GL_FALSE, value);
				break;
			}
			case GL_FLOAT_MAT3: {
				glUniformMatrix3fv(u.location, u.num_elements, GL_FALSE, value);
				break;
			}
			case GL_FLOAT_MAT4: {
				glUniformMatrix4fv(u.location, u.num_elements, GL_FALSE, value);
				break;
			}
			default:
				ASSERT_LOG(false, "Unhandled uniform type: " << it->second.type);
			}	
		}

		void ShaderProgram::setUniformFromVariant(int uid, const variant& value) const
		{
			if(uid == ShaderProgram::INVALID_UNIFORM) {
				LOG_WARN("Tried to set value for invalid uniform iterator.");
				return;
			}
			auto it = v_uniforms_.find(uid);
			ASSERT_LOG(it != v_uniforms_.end(), "Couldn't find location " << uid << " on the uniform list.");
			const Actives& u = it->second;
			if(value.is_null()) {
				ASSERT_LOG(false, "setUniformFromVariant(): value is null. shader='" << getName() << "', uid: " << uid << " : '" << u.name << "'");
			}
			switch(u.type) {
			case GL_FLOAT: {
				if(u.num_elements == 1) {
					glUniform1f(u.location, value.as_float());
				} else {
					ASSERT_LOG(u.num_elements == value.num_elements(), "Incorrect number of elements for uniform array: " << u.num_elements << " vs " << value.num_elements());
					std::vector<float> v(u.num_elements);
					for(int n = 0; n < value.num_elements(); ++n) {
						v[n] = value[n].as_float();
					}

					glUniform1fv(u.location, u.num_elements, &v[0]);
				}
				break;
			}
			case GL_FLOAT_VEC2: {
				if(!(value.num_elements() % 2 == 0 && value.num_elements()/2 <= u.num_elements)) {
					LOG_WARN("Elements in vector must be divisible by 2 and fit in the array");
				}
				std::vector<float> v(value.num_elements());
				for(int n = 0; n < value.num_elements(); ++n) {
					v[n] = value[n].as_float();
				}
				glUniform2fv(u.location, static_cast<GLsizei>(v.size()/2), &v[0]);
				break;
			}
			case GL_FLOAT_VEC3: {
				if(!(value.num_elements() % 3 == 0 && value.num_elements()/3 <= u.num_elements)) {
					LOG_WARN("Elements in vector must be divisible by 3 and fit in the array");
				}
				std::vector<float> v(value.num_elements());
				for(int n = 0; n < value.num_elements(); ++n) {
					v[n] = value[n].as_float();
				}
				glUniform3fv(u.location, static_cast<GLsizei>(v.size()/3), &v[0]);
				break;
			}
			case GL_FLOAT_VEC4: {
				if(!(value.num_elements() % 4 == 0 && value.num_elements()/4 <= u.num_elements)) {
					LOG_WARN("Elements in vector must be divisible by 4 and fit in the array");
				}
				std::vector<float> v(value.num_elements());
				for(int n = 0; n < value.num_elements(); ++n) {
					v[n] = value[n].as_float();
				}
				glUniform4fv(u.location, static_cast<GLsizei>(v.size()/4), &v[0]);
				break;
			}
			
			case GL_BOOL:
			case GL_INT: {
				if(u.num_elements == 1) {
					glUniform1i(u.location, value.as_int32());
				} else {
					ASSERT_LOG(u.num_elements == value.num_elements(), "Incorrect number of elements for uniform array: " << u.num_elements << " vs " << value.num_elements());
					std::vector<int> v(u.num_elements);
					for(int n = 0; n < value.num_elements(); ++n) {
						v[n] = value[n].as_int32();
					}

					glUniform1iv(u.location, u.num_elements, &v[0]);
				}
				break;
			}
			case GL_BOOL_VEC2:	
			case GL_INT_VEC2: {
				if(!(value.num_elements() % 2 == 0 && value.num_elements()/2 <= u.num_elements)) {
					LOG_WARN("Elements in vector must be divisible by 2 and fit in the array");
				}
				std::vector<int> v(value.num_elements());
				for(int n = 0; n < value.num_elements(); ++n) {
					v[n] = value[n].as_int32();
				}
				glUniform2iv(u.location, static_cast<GLsizei>(v.size()/2), &v[0]);
				break;
			}
			case GL_BOOL_VEC3:	
			case GL_INT_VEC3:{
				if(!(value.num_elements() % 3 == 0 && value.num_elements()/3 <= u.num_elements)) {
					LOG_WARN("Elements in vector must be divisible by 3 and fit in the array");
				}
				std::vector<int> v(value.num_elements());
				for(int n = 0; n < value.num_elements(); ++n) {
					v[n] = value[n].as_int32();
				}
				glUniform3iv(u.location, static_cast<GLsizei>(v.size()/3), &v[0]);
				break;
			}
			case GL_BOOL_VEC4:
			case GL_INT_VEC4: {
				if(!(value.num_elements() % 4 == 0 && value.num_elements()/4 <= u.num_elements)) {
					LOG_WARN("Elements in vector must be divisible by 4 and fit in the array");
				}
				std::vector<int> v(value.num_elements());
				for(int n = 0; n < value.num_elements(); ++n) {
					v[n] = value[n].as_int32();
				}
				glUniform2iv(u.location, static_cast<GLsizei>(v.size()/4), &v[0]);
				break;
			}
			
			case GL_FLOAT_MAT2:	{
				if(value.num_elements() != 4) { LOG_WARN("Must be four(4) elements in matrix."); }
				float v[4];
				for(int n = 0; n < value.num_elements(); ++n) {
					v[n] = value[n].as_float();
				}
				glUniformMatrix2fv(u.location, u.num_elements, GL_FALSE, &v[0]);
				break;
			}
			case GL_FLOAT_MAT3: {
				if(value.num_elements() != 9) { LOG_WARN("Must be four(9) elements in matrix."); }
				GLfloat v[9];
				for(int n = 0; n < value.num_elements(); ++n) {
					v[n] = GLfloat(value[n].as_float());
				}
				glUniformMatrix3fv(u.location, u.num_elements, GL_FALSE, &v[0]);
				break;
			}
			case GL_FLOAT_MAT4: {
				if(value.num_elements() != 16) { LOG_WARN("Must be four(16) elements in matrix."); }
				GLfloat v[16];
				for(int n = 0; n < value.num_elements(); ++n) {
					v[n] = GLfloat(value[n].as_float());
				}
				glUniformMatrix4fv(u.location, u.num_elements, GL_FALSE, &v[0]);
				break;
			}

			case GL_SAMPLER_2D:		glUniform1i(u.location, value.as_int32()); break;

			case GL_SAMPLER_CUBE:
			default:
				LOG_DEBUG("Unhandled uniform type: " << it->second.type);
			}
		}

		void ShaderProgram::setAlternateUniformName(const std::string& name, const std::string& alt_name)
		{
			//ASSERT_LOG(uniform_alternate_name_map_.find(alt_name) == uniform_alternate_name_map_.end(),
			//	"Trying to replace alternative uniform name: " << alt_name << " " << name);
			uniform_alternate_name_map_[alt_name] = name;
		}

		void ShaderProgram::setAlternateAttributeName(const std::string& name, const std::string& alt_name)
		{
			//ASSERT_LOG(attribute_alternate_name_map_.find(alt_name) == attribute_alternate_name_map_.end(),
			//	"Trying to replace alternative attribute name: " << alt_name << " " << name);
			attribute_alternate_name_map_[alt_name] = name;
		}

		void ShaderProgram::setActives()
		{
			glUseProgram(object_);
			// Cache some frequently used uniforms.
			u_mvp_ = getUniform("mvp_matrix");
			u_mv_ = getUniform("mv_matrix");
			u_p_ = getUniform("p_matrix");
			u_color_ = getUniform("color");
			u_line_width_ = getUniform("line_width");
			u_tex_ = getUniform("tex_map");
			if(getAttribute("position") != KRE::ShaderProgram::INVALID_UNIFORM) {
				a_vertex_ = getAttribute("position");
			} else {
				a_vertex_ = getAttribute("vertex");
			}
			a_texcoord_ = getAttribute("texcoord");
			a_color_ = getAttribute("a_color");
			a_normal_ = getAttribute("normal");

			// I don't like having to have these as well.
			u_enable_palette_lookup_ = getUniform("u_enable_palette_lookup");
			u_palette_ = getUniform("u_palette");
			u_palette_width_ = getUniform("u_palette_width");
			u_palette_map_ = getUniform("u_palette_map");
			u_mix_palettes_ = getUniform("u_mix_palettes");
			u_mix_ = getUniform("u_mix");
		}

		ShaderProgramPtr ShaderProgram::factory(const std::string& name)
		{
			auto& sf = get_shader_factory();
			auto it = sf.find(name);
			ASSERT_LOG(it != sf.end(), "Shader '" << name << "' not found in the list of shaders.");
			return it->second;
		}

		ShaderProgramPtr ShaderProgram::factory(const variant& node)
		{
			return getProgramFromVariant(node);
		}

		ShaderProgramPtr ShaderProgram::defaultSystemShader()
		{
			auto& sf = get_shader_factory();
			auto it = sf.find("default");
			ASSERT_LOG(it != sf.end(), "No 'default' shader found in the list of shaders.");
			return it->second;
		}

		ShaderProgramPtr ShaderProgram::getProgramFromVariant(const variant& node)
		{
			auto& sf = get_shader_factory();

			if(node.has_key("name") && !node.has_key("vertex") && !node.has_key("fragment")) {
				std::string name = node["name"].as_string();
				auto it = sf.find(name);
				ASSERT_LOG(it != sf.end(), "Unable to find shader '" << name << "'");
				return it->second;
			}

			ASSERT_LOG(node.is_map(), "instance must be a map.");
			ASSERT_LOG(node.has_key("fragment") && node.has_key("vertex") && node.has_key("name"), 
				"instances must have 'fragment', 'vertex' and 'name' attributes. " << node.to_debug_string());
		
			const std::string& name = node["name"].as_string();
			const std::string& vert_data = node["vertex"].as_string();
			const std::string& frag_data = node["fragment"].as_string();

			auto it = sf.find(name);
			if(it != sf.end()) {
				return it->second;
			}

			auto spp = std::make_shared<GLESv2::ShaderProgram>(name, 
				ShaderDef(name + "_vs", vert_data),
				ShaderDef(name + "_fs", frag_data),
				node);
			it = sf.find(name);
			if(it != sf.end()) {
				LOG_WARN("Overwriting shader with name: " << name);
			}
			sf[name] = spp;
			if(node.has_key("uniforms")) {
				ASSERT_LOG(node["uniforms"].is_map(), "'uniforms' attribute in shader(" << name << ") must be a map.");
				for(auto uni : node["uniforms"].as_map()) {
					spp->setAlternateUniformName(uni.second.as_string(), uni.first.as_string());
				}
			}
			if(node.has_key("attributes")) {
				ASSERT_LOG(node["attributes"].is_map(), "'attributes' attribute in shader(" << name << ") must be a map.");
				for(auto attr : node["attributes"].as_map()) {
					spp->setAlternateAttributeName(attr.second.as_string(), attr.first.as_string());
				}
			}
			spp->setActives();
			LOG_INFO("Added shader: " << name);
			return spp;
		}

		void ShaderProgram::loadShadersFromVariant(const variant& node)
		{
			if(!node.has_key("instances")) {
				getProgramFromVariant(node);
				return;
			}
			ASSERT_LOG(node["instances"].is_list(), "'instances' attribute should be a list.");

			if(node.has_key("instances") && node["instances"].is_list()) {
				for(auto instance : node["instances"].as_list()) {
					getProgramFromVariant(instance);
				}
			} else {
				getProgramFromVariant(node);
			}
		}

		void ShaderProgram::configureActives(AttributeSetPtr attrset)
		{
			for(auto& attr : attrset->getAttributes()) {
				for(auto& desc : attr->getAttrDesc()) {
					desc.setLocation(getAttribute(desc.getAttrName()));
				}
			}
		}

		void ShaderProgram::configureAttribute(AttributeBasePtr attr)
		{
			for(auto& desc : attr->getAttrDesc()) {
				desc.setLocation(getAttribute(desc.getAttrName()));
			}
		}
		
		void ShaderProgram::configureUniforms(UniformBufferBase& uniforms)
		{
			/*if(DisplayDevice::checkForFeature(DisplayDeviceCapabilties::UNIFORM_BUFFERS)) {
				auto hw = std::unique_ptr<UniformHardwareOGL>(new UniformHardwareOGL(uniforms.getName()));
				uniforms.setHardware(std::move(hw));
			}*/
		}

		void ShaderProgram::applyAttribute(AttributeBasePtr attr) 
		{
			auto attr_hw = attr->getDeviceBufferData();
			attr_hw->bind();
			for(auto& attrdesc : attr->getAttrDesc()) {
				auto loc = attrdesc.getLocation();
				glEnableVertexAttribArray(loc);					
				glVertexAttribPointer(loc, 
					attrdesc.getNumElements(), 
					convert_render_variable_type(attrdesc.getVarType()), 
					attrdesc.normalise(), 
					static_cast<GLsizei>(attrdesc.getStride()), 
					reinterpret_cast<const GLvoid*>(attr_hw->value() + attr->getOffset() + attrdesc.getOffset()));
				enabled_attribs_.emplace_back(loc);
			}
		}

		void ShaderProgram::setUniformMapping(const std::vector<std::pair<std::string, std::string>>& mapping)
		{
			for(auto& m : mapping) {
				setAlternateUniformName(m.first, m.second);
			}
			setActives();
		}

		void ShaderProgram::setAttributeMapping(const std::vector<std::pair<std::string, std::string>>& mapping)
		{
			for(auto& m : mapping) {
				setAlternateAttributeName(m.first, m.second);
			}
			setActives();
		}

		void ShaderProgram::cleanUpAfterDraw()
		{
			for(auto attrib : enabled_attribs_) {
				glDisableVertexAttribArray(attrib);
			}
			enabled_attribs_.clear();
		}

		void ShaderProgram::setUniformsForTexture(const TexturePtr& tex) const
		{
			if(tex) {
				// XXX The material may need to set more texture uniforms for multi-texture -- need to do that here.
				// Or maybe it should be done through the uniform block and override this somehow.
				if(getTexMapUniform() != ShaderProgram::INVALID_UNIFORM) {
					setUniformValue(getTexMapUniform(), 0);
				}

				tex->bind();

				bool enable_palette = tex->isPaletteized();
				if(enable_palette) {
					if(u_palette_map_ != ShaderProgram::INVALID_UNIFORM) {
						setUniformValue(u_palette_map_, 1);
					} else {
						enable_palette = false;
					}
					if(u_palette_ != ShaderProgram::INVALID_UNIFORM) {
						// XXX replace tex->getSurfaces()[1]->height() with tex->getNormalizedCoordH(1, 1);
						float h = static_cast<float>(tex->getSurfaces()[1]->height()) - 1.0f;
						float palette_sel[2];
						palette_sel[0] = static_cast<float>(tex->getPalette()) / h;
						palette_sel[1] = 0;
						if(u_mix_palettes_ != ShaderProgram::INVALID_UNIFORM && u_mix_ != ShaderProgram::INVALID_UNIFORM) {
							bool do_mix = false;
							if(tex->shouldMixPalettes()) {
								palette_sel[1] = static_cast<float>(tex->getPalette(1)) / h;
								setUniformValue(u_mix_, tex->getMixingRatio()); 
								do_mix = true;
							}
							setUniformValue(u_mix_palettes_, do_mix);
						}
						setUniformValue(u_palette_, palette_sel); 
					} else {
						enable_palette = false;
					}
					if(u_palette_width_ != ShaderProgram::INVALID_UNIFORM) {
						// XXX this needs adjusted for pot texture width.
						setUniformValue(u_palette_width_, static_cast<float>(tex->getSurfaces()[1]->width()));
					} else {
						enable_palette = false;
					}
				}

				if(u_enable_palette_lookup_ != ShaderProgram::INVALID_UNIFORM) {
					setUniformValue(u_enable_palette_lookup_, enable_palette);
				}
			}
		}

		KRE::ShaderProgramPtr ShaderProgram::clone() 
		{
			return KRE::ShaderProgramPtr(new GLESv2::ShaderProgram(*this));
		}

		ShaderProgramPtr ShaderProgram::createShader(const std::string& name, 
			const std::vector<ShaderData>& shader_data, 
			const std::vector<ActiveMapping>& uniform_map,
			const std::vector<ActiveMapping>& attribute_map)
		{
			auto spp = std::make_shared<GLESv2::ShaderProgram>(name, shader_data, uniform_map, attribute_map);
			spp->setActives();
			return spp;
		}

		ShaderProgramPtr ShaderProgram::createGaussianShader(int radius)
		{
			// we use a systematic naming for our gaussian blur shaders, so we can cache.
			std::stringstream ss;
			ss << "blur" << radius;
			const std::string shader_name = ss.str();
			auto& sf = get_shader_factory();
			auto it = sf.find(shader_name);
			if(it != sf.end()) {
				return it->second;
			}
			const std::string fs_name = shader_name + "_fs";

			std::stringstream fs;
			fs	<< "#version 120\n"
				<< "uniform sampler2D u_tex_map;\n"
				<< "uniform float texel_width_offset;\n"
				<< "uniform float texel_height_offset;\n"
				<< "uniform vec4 u_color;\n"
				<< "uniform float gaussian[" << (2 * radius + 1) << "];\n"
				<< "varying vec2 v_texcoords;\n"
				<< "\n"
				<< "void main()\n"
				<< "{\n"
				<< "    vec4 sum = vec4(0.0, 0.0, 0.0, 0.0);\n"
				<< "    vec2 step_offset = vec2(texel_width_offset, texel_height_offset);\n"
				<< "    for(int index = 0; index < " << (2 * radius + 1) << "; ++index) {\n"
				<< "        sum += texture2D(u_tex_map, v_texcoords + step_offset * (index - " << radius << ")) * gaussian[index];\n"
				<< "    }\n"
				<< "    gl_FragColor = sum * u_color;\n"
				<< "}\n";

			auto spp = std::make_shared<GLESv2::ShaderProgram>(shader_name, ShaderDef("blur_fs", blur_vs), ShaderDef(fs_name, fs.str()), variant());
			auto um = blur_uniform_mapping;
			while(strlen(um->alt_name) > 0) {
				spp->setAlternateUniformName(um->name, um->alt_name);
				++um;
			}
			auto am = blur_attribute_mapping;
			while(strlen(am->alt_name) > 0) {
				spp->setAlternateAttributeName(am->name, am->alt_name);
				++am;
			}
			spp->setActives();
			return spp;
		}
	}
}
