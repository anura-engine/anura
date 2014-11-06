/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#if defined(USE_SHADERS)

#include <boost/intrusive_ptr.hpp>

#include "camera.hpp"
#include "color_utils.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "graphics.hpp"
#include "variant.hpp"

namespace graphics
{
	class sunlight : public game_logic::formula_callable
	{
	public:
		explicit sunlight(gles2::program_ptr shader, const variant& node);
		virtual ~sunlight();
		void set_all_uniforms() const;

		float ambient_intensity() const { return abmient_intensity_; }
		void set_ambient_intensity(float f);

		const color& get_color() const { return color_; }
		void set_color(const color& color);

		const glm::vec3& direction() const { return direction_; }
		void set_direction(const glm::vec3& d);

		variant write();
	private:
		DECLARE_CALLABLE(sunlight);

		void configure_uniforms();

		gles2::program_ptr shader_;

		GLuint u_color_;
		GLuint u_ambient_intensity_;
		GLuint u_direction_;

		color color_;
		glm::vec3 direction_;
		float abmient_intensity_;

		bool enabled_;
		
		sunlight();
		sunlight(const sunlight&);
	};
	typedef boost::intrusive_ptr<sunlight> sunlight_ptr;

	class lighting : public game_logic::formula_callable
	{
	public:
		explicit lighting(gles2::program_ptr shader);
		explicit lighting(gles2::program_ptr shader, const variant& node);
		virtual ~lighting();

		void set_all_uniforms() const;

		gles2::program_ptr shader() { return shader_; }

		const std::vector<float>& light_power() const { return light_power_; }
		void set_light_power(int n, float lp);
		void set_light_power(const std::vector<float>& lp);

		const std::vector<glm::vec3>& light_position() const { return light_position_; }
		void set_light_position(int n, const glm::vec3& lp);
		void set_light_position(const std::vector<glm::vec3>& lp);

		const std::vector<glm::vec3>& light_color() const { return light_color_; }
		void set_light_color(int n, const glm::vec3& lc);
		void set_light_color(const std::vector<glm::vec3>& lc);

		const std::vector<float>& gamma() const { return gamma_; }
		void set_gamma(int n, float g);
		void set_gamma(const std::vector<float>& g);

		const std::vector<glm::vec3>& ambient_color() const { return ambient_color_; }
		void set_ambient_color(int n, const glm::vec3& ac);
		void set_ambient_color(const std::vector<glm::vec3>& ac);

		const std::vector<float>& ambient_intensity() const { return ambient_intensity_; }
		void set_ambient_intensity(int n, float shiny);
		void set_ambient_intensity(const std::vector<float>& shiny);

		const std::vector<glm::vec3>& specular_color() const { return specular_color_; }
		void set_specular_color(int n, const glm::vec3& sc);
		void set_specular_color(const std::vector<glm::vec3>& sc);

		const std::vector<float>& shininess() const { return shininess_; }
		void set_shininess(int n, float shiny);
		void set_shininess(const std::vector<float>& shiny);

		void set_modelview_matrix(const glm::mat4& mm, const glm::mat4& vm);
		
		void enable_light_source(int n, bool en);

		variant write();
	protected:
		GLuint light_position_uniform() const { return u_lightposition_; }
		GLuint light_power_uniform() const { return u_lightpower_; }
		GLuint shininess_uniform() const { return u_shininess_; }
		GLuint gamma_uniform() const { return u_gamma_; }
		GLuint ambient_color_uniform() const { return u_ambient_color_; }
		GLuint ambient_intensity_uniform() const { return u_ambient_color_; }
		GLuint specular_color_uniform() const { return u_specular_color_; }
		GLuint light_color_uniform() const { return u_light_color_; }
		GLuint m_matrix_uniform() const { return u_m_matrix_; }
		GLuint v_matrix_uniform() const { return u_v_matrix_; }
		GLuint n_matrix_uniform() const { return u_n_matrix_; }
		GLuint enabled_uniform() const { return u_enabled_; }
	private:
		DECLARE_CALLABLE(lighting);

		void configure_uniforms();

		gles2::program_ptr shader_;
		
		GLuint u_lightposition_;
		GLuint u_lightpower_;
		GLuint u_light_color_;

		GLuint u_gamma_;

		GLuint u_ambient_color_;
		GLuint u_ambient_intensity_;

		GLuint u_specular_color_;
		GLuint u_shininess_;

		GLuint u_m_matrix_;
		GLuint u_v_matrix_;
		GLuint u_n_matrix_;

		GLuint u_enabled_;

		sunlight_ptr sunlight_;

		std::vector<int> lights_enabled_;

		std::vector<float> light_power_;
		std::vector<glm::vec3> light_position_;
		std::vector<glm::vec3> light_color_;

		std::vector<float> gamma_;

		std::vector<glm::vec3> ambient_color_;
		std::vector<float> ambient_intensity_;

		std::vector<glm::vec3> specular_color_;
		std::vector<float> shininess_;

		bool configure_uniforms_on_set_;

		lighting();
		lighting(const lighting&);
	};
	typedef boost::intrusive_ptr<lighting> lighting_ptr;
	typedef boost::intrusive_ptr<const lighting> const_lighting_ptr;
}

#endif
