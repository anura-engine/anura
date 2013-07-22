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
#if defined(USE_GLES2)

#include <glm/gtc/matrix_inverse.hpp>

#include "lighting.hpp"
#include "variant_utils.hpp"

namespace graphics
{
	namespace 
	{
		const double default_light_power = 1000.0;
		const double default_shininess = 5.0;
		const double default_gamma = 1.0;

		struct manager
		{
			manager(gles2::program_ptr shader) 
			{
				glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);
				glUseProgram(shader->get());
			}
			~manager()
			{
				glUseProgram(old_program);
			}
			GLint old_program;
		};
	}

	lighting::lighting(gles2::program_ptr shader, const variant& node)
		: shader_(shader),
		light_power_(float(node["light_power"].as_decimal(decimal(default_light_power)).as_float())),
		shininess_(float(node["shininess"].as_decimal(decimal(default_shininess)).as_float())),
		gamma_(float(node["gamma"].as_decimal(decimal(default_gamma)).as_float()))

	{
		light_position_ = variant_to_vec3(node["light_position"]);
		if(node.has_key("ambient_color")) {
			ambient_color_ = variant_to_vec3(node["ambient_color"]);
		} else {
			ambient_color_ = glm::vec3(0.1f, 0.1f, 0.1f);
		}
		if(node.has_key("specular_color")) {
			specular_color_ = variant_to_vec3(node["specular_color"]);
		} else {
			specular_color_ = glm::vec3(0.1f, 0.1f, 0.1f);
		}
		if(node.has_key("light_color")) {
			light_color_ = variant_to_vec3(node["light_color"]);
		} else {
			light_color_ = glm::vec3(1.0f, 1.0f, 1.0f);
		}

		configure_uniforms();
		set_all_uniforms();
	}

	lighting::lighting(gles2::program_ptr shader)
		: shader_(shader), light_power_(1000.0f), shininess_(5.0f), gamma_(1.0f),
		light_position_(0.0f, 20.0f, 0.0f), ambient_color_(0.2f, 0.2f, 0.2f),
		specular_color_(0.1f, 0.1f, 0.1f), light_color_(1.0f, 1.0f, 1.0f)
	{
		configure_uniforms();
		set_all_uniforms();
	}

	lighting::~lighting()
	{
	}

	variant lighting::write()
	{
		variant_builder res;
		if(light_power_ != default_light_power) {
			res.add("light_power", light_power_);
		}
		if(shininess_ != default_shininess) {
			res.add("shininess", shininess_);
		}
		if(gamma_ != default_gamma) {
			res.add("gamma", gamma_);
		}
		res.add("light_position", vec3_to_variant(light_position_));
		if(ambient_color_.r != 0.1f && ambient_color_.g != 0.1f && ambient_color_.b != 0.1f) {
			res.add("ambient_color", vec3_to_variant(ambient_color_));
		}
		if(specular_color_.r != 0.1f && specular_color_.g != 0.1f && specular_color_.b != 0.1f) {
			res.add("specular_color", vec3_to_variant(specular_color_));
		}
		if(light_color_.r != 1.0f && light_color_.g != 1.0f && light_color_.b != 1.0f) {
			res.add("light_color", vec3_to_variant(light_color_));
		}
		return res.build();
	}

	void lighting::set_all_uniforms() const
	{
		if(enabled_) { 
			manager m(shader_);
			glUniform1f(u_lightpower_, light_power_);
			glUniform3fv(u_lightposition_, 1, glm::value_ptr(light_position_));
			glUniform1f(u_shininess_, shininess_);
			glUniform1f(u_gamma_, gamma_);
			glUniform3fv(u_ambient_color_, 1, glm::value_ptr(ambient_color_));
			glUniform3fv(u_specular_color_, 1, glm::value_ptr(specular_color_));
			glUniform3fv(u_light_color_, 1, glm::value_ptr(light_color_));
		}
	}

	void lighting::configure_uniforms()
	{
		u_lightposition_ = shader_->get_fixed_uniform("light_position");
		u_lightpower_ = shader_->get_fixed_uniform("light_power");
		u_shininess_ = shader_->get_fixed_uniform("shininess");
		u_gamma_ = shader_->get_fixed_uniform("gamma");
		u_ambient_color_ = shader_->get_fixed_uniform("ambient_color");
		u_specular_color_ = shader_->get_fixed_uniform("specular_color");
		u_light_color_ = shader_->get_fixed_uniform("light_color");
		u_m_matrix_ = shader_->get_fixed_uniform("m_matrix");
		u_v_matrix_ = shader_->get_fixed_uniform("v_matrix");
		u_n_matrix_ = shader_->get_fixed_uniform("normal_matrix");
		
		enabled_ = u_lightposition_ != -1
			&& u_lightpower_ != -1
			&& u_shininess_ != -1
			&& u_gamma_ != -1
			&& u_ambient_color_ != -1
			&& u_specular_color_ != -1
			&& u_light_color_ != -1
			&& u_m_matrix_ != -1
			&& u_v_matrix_ != -1
			&& u_n_matrix_ != -1;
		if(!enabled_) {
			std::cerr << "Lighting Disabled" << std::endl;
		}
	}

	void lighting::set_light_power(float lp)
	{
		if(enabled_) {
			manager m(shader_);
			light_power_ = lp;
			glUniform1f(u_lightpower_, light_power_);
		}
	}

	void lighting::set_light_position(const glm::vec3& lp)
	{
		if(enabled_) {
			manager m(shader_);
			light_position_ = lp;
			glUniform3fv(u_lightposition_, 1, glm::value_ptr(light_position_));
		}
	}

	void lighting::set_shininess(float shiny)
	{
		if(enabled_) {
			manager m(shader_);
			shininess_ = shiny;
			glUniform1f(u_shininess_, shininess_);
		}
	}

	void lighting::set_gamma(float g)
	{
		if(enabled_) {
			manager m(shader_);
			gamma_ = g;
			glUniform1f(u_gamma_, gamma_);
		}
	}

	void lighting::set_ambient_color(const glm::vec3& ac)
	{
		if(enabled_) {
			manager m(shader_);
			ambient_color_ = ac;
			glUniform3fv(u_ambient_color_, 1, glm::value_ptr(ambient_color_));
		}
	}

	void lighting::set_specular_color(const glm::vec3& sc)
	{
		if(enabled_) {
			manager m(shader_);
			specular_color_ = sc;
			glUniform3fv(u_specular_color_, 1, glm::value_ptr(specular_color_));
		}
	}

	void lighting::set_light_color(const glm::vec3& lc)
	{
		if(enabled_) {
			manager m(shader_);
			light_color_ = lc;
			glUniform3fv(u_light_color_, 1, glm::value_ptr(light_color_));
		}
	}

	void lighting::set_modelview_matrix(const glm::mat4& mm, const glm::mat4& vm)
	{
		if(enabled_) {
			manager m(shader_);
			glm::mat4 nm = glm::inverseTranspose(vm * mm);
			glUniformMatrix4fv(u_m_matrix_, 1, GL_FALSE, glm::value_ptr(mm));
			glUniformMatrix4fv(u_v_matrix_, 1, GL_FALSE, glm::value_ptr(vm));
			glUniformMatrix4fv(u_n_matrix_, 1, GL_FALSE, glm::value_ptr(nm));
		}
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(lighting)
	DEFINE_FIELD(shininess, "decimal")
		return variant(obj.shininess());
	DEFINE_SET_FIELD
		obj.set_shininess(float(value.as_decimal().as_float()));
	DEFINE_FIELD(gamma, "decimal")
		return variant(obj.gamma());
	DEFINE_SET_FIELD
		obj.set_gamma(float(value.as_decimal().as_float()));
	DEFINE_FIELD(light_power, "decimal")
		return variant(obj.light_power());
	DEFINE_SET_FIELD
		obj.set_light_power(float(value.as_decimal().as_float()));
	DEFINE_FIELD(light_position, "[decimal,decimal,decimal]")
		return vec3_to_variant(obj.light_position());
	DEFINE_SET_FIELD
		obj.set_light_position(variant_to_vec3(value));
	DEFINE_FIELD(ambient_color, "[decimal,decimal,decimal]")
		return vec3_to_variant(obj.ambient_color());
	DEFINE_SET_FIELD
		obj.set_ambient_color(variant_to_vec3(value));
	DEFINE_FIELD(specular_color, "[decimal,decimal,decimal]")
		return vec3_to_variant(obj.specular_color());
	DEFINE_SET_FIELD
		obj.set_specular_color(variant_to_vec3(value));
	DEFINE_FIELD(light_color, "[decimal,decimal,decimal]")
		return vec3_to_variant(obj.light_color());
	DEFINE_SET_FIELD
		obj.set_light_color(variant_to_vec3(value));
	END_DEFINE_CALLABLE(lighting)
}

#endif