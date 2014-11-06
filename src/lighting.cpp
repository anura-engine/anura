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
#if defined(USE_SHADERS)

#include <glm/gtc/matrix_inverse.hpp>

#include "lighting.hpp"
#include "variant_utils.hpp"

namespace graphics
{
	namespace 
	{
		const double default_light_power = 1.0;
		const double default_shininess = 1.0;
		const double default_gamma = 1.0;
		const float default_ambient_intensity = 0.6f;
		const glm::vec3 default_light_position(0.0f, 0.0f, 0.7f);
		const glm::vec3 default_light_color(1.0f, 1.0f, 1.0f);
		const glm::vec3 default_ambient_color(1.0f, 1.0f, 1.0f);
		const glm::vec3 default_specular_color(0.1f, 0.1f, 0.1f);
	}

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

	lighting::lighting(gles2::program_ptr shader, const variant& node)
		: shader_(shader), configure_uniforms_on_set_(node["configure_uniforms_on_set"].as_bool(false))
	{
		configure_uniforms();

		if(node.has_key("lights")) {
			for(int n = 0; n != node["lights"].num_elements(); ++n) {
				ASSERT_LOG(node["lights"][n].is_map(), "Inner elements of lighting must be maps.");
				const variant& element = node["lights"][n];

				//ASSERT_LOG(n < light_position_.size(), "FATAL: LIGHTING: The reported number of elements for 'light_position' uniform is less than requested. " << n << " >= " << light_position_.size());
				if(n < light_position_.size()) {
					light_position_[n] = variant_to_vec3(element["light_position"]);
				}
				if(element.has_key("light_color")) {
					//ASSERT_LOG(n < light_color_.size(), "FATAL: LIGHTING: The reported number of elements for 'light_color' uniform is less than requested. " << n << " >= " << light_color_.size());
					if(n < light_color_.size()) {
						light_color_[n] = variant_to_vec3(element["light_color"]);
					}
				}
				if(element.has_key("light_power")) {
					//ASSERT_LOG(n < light_power_.size(), "FATAL: LIGHTING: The reported number of elements for 'light_power' uniform is less than requested. " << n << " >= " << light_power_.size());
					if(n < light_power_.size()) {
						light_power_[n] = element["light_power"].as_decimal().as_float();
					}
				}

				if(element.has_key("ambient_color")) {
					//ASSERT_LOG(n < ambient_color_.size(), "FATAL: LIGHTING: The reported number of elements for 'ambient_color' uniform is less than requested. " << n << " >= " << ambient_color_.size());
					if(n < ambient_color_.size()) {
						ambient_color_[n] = variant_to_vec3(element["ambient_color"]);
					}
				}
				if(element.has_key("ambient_intensity")) {
					//ASSERT_LOG(n < ambient_intensity_.size(), "FATAL: LIGHTING: The reported number of elements for 'ambient_intensity' uniform is less than requested. " << n << " >= " << ambient_intensity_.size());
					if(n < ambient_intensity_.size()) {
						ambient_intensity_[n] = float(element["ambient_intensity"].as_decimal().as_float());
					}
				}

				if(element.has_key("specular_color")) {
					//ASSERT_LOG(n < specular_color_.size(), "FATAL: LIGHTING: The reported number of elements for 'specular_color' uniform is less than requested. " << n << " >= " << specular_color_.size());
					specular_color_[n] = variant_to_vec3(element["specular_color"]);
				}
				if(element.has_key("shininess")) {
					//ASSERT_LOG(n < shininess_.size(), "FATAL: LIGHTING: The reported number of elements for 'shininess' uniform is less than requested. " << n << " >= " << shininess_.size());
					if(n < shininess_.size()) {
						shininess_[n] = float(element["shininess"].as_decimal().as_float());
					}
				}

				if(element.has_key("enabled")) {
					//ASSERT_LOG(n < lights_enabled_.size(), "FATAL: LIGHTING: The reported number of elements for 'enabled' uniform is less than requested. " << n << " >= " << lights_enabled_.size());
					if(n < lights_enabled_.size()) {
						lights_enabled_[n] = float(element["enabled"].as_bool());
					}
				} else {
					if(n < lights_enabled_.size()) {
						lights_enabled_[n] = true;
					}
				}
			}
		}

		if(node.has_key("sunlight")) {
			sunlight_.reset(new sunlight(shader, node["sunlight"]));
		}

		set_all_uniforms();
	}

	lighting::lighting(gles2::program_ptr shader)
		: shader_(shader), configure_uniforms_on_set_(false)
	{
		configure_uniforms();
		set_all_uniforms();
	}

	lighting::~lighting()
	{
	}

	variant lighting::write()
	{
		/*variant_builder res;
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
		if(sunlight_) {
			res.add("sunlight", sunlight_->write());
		}
		return res.build();*/
		return variant();
	}

	void lighting::set_all_uniforms() const
	{
		manager m(shader_);

		if(u_lightpower_ != -1) {			
			glUniform1fv(u_lightpower_, light_power_.size(), &light_power_[0]);
		}
		if(u_lightposition_ != -1) {
			glUniform3fv(u_lightposition_, light_position_.size(), reinterpret_cast<const GLfloat*>(&light_position_[0]));
		}
		if(u_shininess_ != -1) {
			glUniform1fv(u_shininess_, shininess_.size(), &shininess_[0]);
		}
		if(u_gamma_ != -1) {
			glUniform1fv(u_gamma_, gamma_.size(), &gamma_[0]);
		}
		if(u_ambient_intensity_ != -1) {
			glUniform1fv(u_ambient_intensity_, ambient_intensity_.size(), &ambient_intensity_[0]);
		}
		if(u_ambient_color_ != -1) {
			glUniform3fv(u_ambient_color_, ambient_color_.size(), reinterpret_cast<const GLfloat*>(&ambient_color_[0]));
		}
		if(u_specular_color_ != -1) {
			glUniform3fv(u_specular_color_, specular_color_.size(), reinterpret_cast<const GLfloat*>(&specular_color_[0]));
		}
		if(u_light_color_ != -1) {
			glUniform3fv(u_light_color_, light_color_.size(), reinterpret_cast<const GLfloat*>(&light_color_[0]));
		}
		if(u_enabled_ != -1) {
			glUniform1iv(u_enabled_, lights_enabled_.size(), &lights_enabled_[0]);
		}

		if(sunlight_) {
			sunlight_->set_all_uniforms();
		}
	}

	void lighting::configure_uniforms()
	{
		u_lightposition_ = shader_->get_fixed_uniform("light_position");
		if(u_lightposition_ != -1) {
			auto it = shader_->get_uniform_reference("light_position");
			light_position_.resize(it->second.num_elements, default_light_position);
		}

		u_lightpower_ = shader_->get_fixed_uniform("light_power");
		if(u_lightpower_ != -1) {
			auto it = shader_->get_uniform_reference("light_power");
			light_power_.resize(it->second.num_elements, default_light_power);
		}

		u_shininess_ = shader_->get_fixed_uniform("shininess");
		if(u_shininess_ != -1) {
			auto it = shader_->get_uniform_reference("shininess");
			shininess_.resize(it->second.num_elements, default_shininess);
		}

		u_gamma_ = shader_->get_fixed_uniform("gamma");
		if(u_gamma_ != -1) {
			auto it = shader_->get_uniform_reference("gamma");
			gamma_.resize(it->second.num_elements, default_gamma);
		}

		u_ambient_color_ = shader_->get_fixed_uniform("ambient_color");
		if(u_ambient_color_ != -1) {
			auto it = shader_->get_uniform_reference("ambient_color");
			ambient_color_.resize(it->second.num_elements, default_ambient_color);
		}

		u_ambient_intensity_ = shader_->get_fixed_uniform("ambient_intensity");
		if(u_ambient_intensity_ != -1) {
			auto it = shader_->get_uniform_reference("ambient_intensity");
			ambient_intensity_.resize(it->second.num_elements, default_ambient_intensity);
		}

		u_specular_color_ = shader_->get_fixed_uniform("specular_color");
		if(u_specular_color_ != -1) {
			auto it = shader_->get_uniform_reference("specular_color");
			specular_color_.resize(it->second.num_elements, default_specular_color);
		}

		u_light_color_ = shader_->get_fixed_uniform("light_color");
		if(u_light_color_ != -1) {
			auto it = shader_->get_uniform_reference("light_color");
			light_color_.resize(it->second.num_elements, default_light_color);
		}

		u_enabled_ = shader_->get_fixed_uniform("enabled");
		if(u_enabled_ != -1) {
			auto it = shader_->get_uniform_reference("enabled");
			lights_enabled_.resize(it->second.num_elements, false);
		}

		u_m_matrix_ = shader_->get_fixed_uniform("m_matrix");
		u_v_matrix_ = shader_->get_fixed_uniform("v_matrix");
		u_n_matrix_ = shader_->get_fixed_uniform("normal_matrix");
	}

	void lighting::set_light_power(int n, float lp)
	{
		ASSERT_LOG(n < light_power_.size(), "FATAL: LIGHTING: The reported number of elements for 'light_power' uniform is less than requested. " << n << " >= " << light_power_.size());
		light_power_[n] = lp;
		if(configure_uniforms_on_set_ && u_lightpower_ != -1) {
			manager m(shader_);
			glUniform1f(u_lightpower_, light_power_[n]);
		}
	}

	void lighting::set_light_power(const std::vector<float>& lp)
	{
		ASSERT_LOG(lp.size() == light_power_.size(), "FATAL: LIGHTING: The reported number of elements for 'light_power' uniform is less than requested. " << lp.size() << " != " << light_power_.size());
		light_power_ = lp;
		if(configure_uniforms_on_set_ && u_lightpower_ != -1) {
			manager m(shader_);
			glUniform1fv(u_lightpower_, light_power_.size(), &light_power_[0]);
		}
	}

	void lighting::set_light_position(int n, const glm::vec3& lp)
	{
		ASSERT_LOG(n < light_position_.size(), "FATAL: LIGHTING: The reported number of elements for 'light_position' uniform is less than requested. " << n << " >= " << light_position_.size());
		light_position_[n] = lp;
		if(u_lightposition_ != -1) {
			if(configure_uniforms_on_set_) {
				manager m(shader_);
				glUniform3fv(u_lightposition_, 1, glm::value_ptr(light_position_[n]));
			}
		} else {
			std::cerr << "WARNING: LIGHTING: set_light_position(" << n << ", [" << lp.x << "," << lp.y << "," << lp.z << "]) but no light position uniform" << std::endl;
		}
	}

	void lighting::set_light_position(const std::vector<glm::vec3>& lp)
	{
		ASSERT_LOG(lp.size() == light_position_.size(), "FATAL: LIGHTING: The reported number of elements for 'light_position' uniform is less than requested. " << lp.size() << " != " << light_position_.size());
		light_position_ = lp;
		if(configure_uniforms_on_set_ && u_lightposition_ != -1) {
			manager m(shader_);
			glUniform1fv(u_lightposition_, light_position_.size(), reinterpret_cast<const GLfloat*>(&light_position_[0]));
		}
	}

	void lighting::set_shininess(int n, float shiny)
	{
		ASSERT_LOG(n < shininess_.size(), "FATAL: LIGHTING: The reported number of elements for 'shininess' uniform is less than requested. " << n << " >= " << shininess_.size());
		shininess_[n] = shiny;
		if(configure_uniforms_on_set_ && u_shininess_ != -1) {
			manager m(shader_);
			glUniform1f(u_shininess_, shininess_[n]);
		}
	}

	void lighting::set_shininess(const std::vector<float>& shiny)
	{
		ASSERT_LOG(shiny.size() == shininess_.size(), "FATAL: LIGHTING: The reported number of elements for 'shininess' uniform is less than requested. " << shiny.size() << " != " << shininess_.size());
		shininess_ = shiny;
		if(configure_uniforms_on_set_ && u_shininess_ != -1) {
			manager m(shader_);
			glUniform1fv(u_shininess_, shininess_.size(), &shininess_[0]);
		}
	}

	void lighting::set_gamma(int n, float g)
	{
		ASSERT_LOG(n < gamma_.size(), "FATAL: LIGHTING: The reported number of elements for 'gamma' uniform is less than requested. " << n << " >= " << gamma_.size());
		gamma_[n] = g;
		if(configure_uniforms_on_set_ && u_gamma_ != -1) {
			manager m(shader_);
			glUniform1f(u_gamma_, gamma_[n]);
		}
	}

	void lighting::set_gamma(const std::vector<float>& g)
	{
		ASSERT_LOG(gamma_.size() == g.size(), "FATAL: LIGHTING: The reported number of elements for 'shininess' uniform is less than requested. " << g.size() << " != " << gamma_.size());
		gamma_ = g;
		if(configure_uniforms_on_set_ && u_gamma_ != -1) {
			manager m(shader_);
			glUniform1fv(u_gamma_, gamma_.size(), &gamma_[0]);
		}
	}

	void lighting::set_ambient_color(int n, const glm::vec3& ac)
	{
		ASSERT_LOG(n < ambient_color_.size(), "FATAL: LIGHTING: The reported number of elements for 'ambient_color' uniform is less than requested. " << n << " >= " << ambient_color_.size());
		ambient_color_[n] = ac;
		if(configure_uniforms_on_set_ && u_ambient_color_ != -1) {
			manager m(shader_);
			glUniform3fv(u_ambient_color_, 1, glm::value_ptr(ambient_color_[n]));
		}
	}

	void lighting::set_ambient_color(const std::vector<glm::vec3>& ac)
	{
		ASSERT_LOG(ac.size() == ambient_color_.size(), "FATAL: LIGHTING: The reported number of elements for 'ambient_color' uniform is less than requested. " << ac.size() << " != " << ambient_color_.size());
		ambient_color_ = ac;
		if(configure_uniforms_on_set_ && u_ambient_color_ != -1) {
			manager m(shader_);
			glUniform1fv(u_ambient_color_, ambient_color_.size(), reinterpret_cast<const GLfloat*>(&ambient_color_[0]));
		}
	}

	void lighting::set_ambient_intensity(int n, float ai)
	{
		ASSERT_LOG(n < ambient_intensity_.size(), "FATAL: LIGHTING: The reported number of elements for 'gamma' uniform is less than requested. " << n << " >= " << ambient_intensity_.size());
		ambient_intensity_[n] = ai;
		if(configure_uniforms_on_set_ && u_ambient_intensity_ != -1) {
			manager m(shader_);
			glUniform1f(u_ambient_intensity_, ambient_intensity_[n]);
		}
	}

	void lighting::set_ambient_intensity(const std::vector<float>& ai)
	{
		ASSERT_LOG(ambient_intensity_.size() == ai.size(), "FATAL: LIGHTING: The reported number of elements for 'shininess' uniform is less than requested. " << ai.size() << " != " << ambient_intensity_.size());
		ambient_intensity_ = ai;
		if(configure_uniforms_on_set_ && u_ambient_intensity_ != -1) {
			manager m(shader_);
			glUniform1fv(u_ambient_intensity_, ambient_intensity_.size(), &ambient_intensity_[0]);
		}
	}

	void lighting::set_specular_color(int n, const glm::vec3& sc)
	{
		ASSERT_LOG(n < specular_color_.size(), "FATAL: LIGHTING: The reported number of elements for 'ambient_color' uniform is less than requested. " << n << " >= " << specular_color_.size());
		specular_color_[n] = sc;
		if(configure_uniforms_on_set_ && u_specular_color_ != -1) {
			manager m(shader_);
			glUniform3fv(u_specular_color_, 1, glm::value_ptr(specular_color_[n]));
		}
	}

	void lighting::set_specular_color(const std::vector<glm::vec3>& sc)
	{
		ASSERT_LOG(sc.size() == specular_color_.size(), "FATAL: LIGHTING: The reported number of elements for 'specular_color' uniform is less than requested. " << sc.size() << " != " << specular_color_.size());
		ambient_color_ = sc;
		if(configure_uniforms_on_set_ && u_specular_color_ != -1) {
			manager m(shader_);
			glUniform1fv(u_specular_color_, specular_color_.size(), reinterpret_cast<const GLfloat*>(&specular_color_[0]));
		}
	}

	void lighting::set_light_color(int n, const glm::vec3& lc)
	{
		ASSERT_LOG(n < light_color_.size(), "FATAL: LIGHTING: The reported number of elements for 'ambient_color' uniform is less than requested. " << n << " >= " << light_color_.size());
		light_color_[n] = lc;
		if(configure_uniforms_on_set_ && u_light_color_ != -1) {
			manager m(shader_);
			glUniform3fv(u_light_color_, 1, glm::value_ptr(light_color_[n]));
		}
	}

	void lighting::set_light_color(const std::vector<glm::vec3>& lc)
	{
		ASSERT_LOG(lc.size() == light_color_.size(), "FATAL: LIGHTING: The reported number of elements for 'light_color' uniform is less than requested. " << lc.size() << " != " << light_color_.size());
		light_color_ = lc;
		if(configure_uniforms_on_set_ && u_light_color_ != -1) {
			manager m(shader_);
			glUniform1fv(u_light_color_, light_color_.size(), reinterpret_cast<const GLfloat*>(&light_color_[0]));
		}
	}

	void lighting::set_modelview_matrix(const glm::mat4& mm, const glm::mat4& vm)
	{
		if(u_m_matrix_ == -1 && u_v_matrix_ == -1 && u_n_matrix_ == -1) {
			return;
		}
		manager m(shader_);
		if(u_m_matrix_ != -1) {
			glUniformMatrix4fv(u_m_matrix_, 1, GL_FALSE, glm::value_ptr(mm));
		}
		if(u_v_matrix_ != -1) {
			glUniformMatrix4fv(u_v_matrix_, 1, GL_FALSE, glm::value_ptr(vm));
		}
		if(u_n_matrix_ != -1) {
			glm::mat4 nm = glm::inverseTranspose(vm * mm);
			glUniformMatrix4fv(u_n_matrix_, 1, GL_FALSE, glm::value_ptr(nm));
		}
	}

	void lighting::enable_light_source(int n, bool en)
	{
		ASSERT_LOG(n < lights_enabled_.size(), "FATAL: LIGHTING: The reported number of elements for 'enabled' uniform is less than requested. " << n << " >= " << lights_enabled_.size());
		lights_enabled_[n] = en;
		if(configure_uniforms_on_set_ && u_enabled_ != -1) {
			manager m(shader_);
			glUniform1i(u_enabled_, lights_enabled_[n]);
		}
	}

	/*class lighting_value_callable : public game_logic::formula_callable {
		variant get_value(const std::string& key) const {
			return variant();
		}
		void set_value(const std::string& key, const variant& value) {
		}
	public:
		explicit lighting_value_callable() : obj_(const_cast<custom_object*>(&obj))
		{}
	};*/

	BEGIN_DEFINE_CALLABLE_NOBASE(lighting)
	DEFINE_FIELD(shininess, "[decimal]")
		std::vector<variant> v;
		for(auto& s : obj.shininess()) {
			v.push_back(variant(s));
		}
		return variant(&v);
	DEFINE_SET_FIELD
		std::vector<float> v;
		for(size_t n = 0; n != value.num_elements(); ++n) {
			v.push_back(float(value[n].as_decimal().as_float()));
		}
		obj.set_shininess(v);

/*	DEFINE_FIELD(gamma, "decimal")
		return variant(obj.gamma());
	DEFINE_SET_FIELD
		obj.set_gamma(float(value.as_decimal().as_float()));
	DEFINE_FIELD(ambient_intensity, "decimal")
		return variant(obj.ambient_intensity());
	DEFINE_SET_FIELD
		obj.set_ambient_intensity(float(value.as_decimal().as_float()));
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
		obj.set_light_color(variant_to_vec3(value));*/
	DEFINE_FIELD(sunlight, "builtin sunlight|null")
		return variant(obj.sunlight_.get());
	DEFINE_SET_FIELD_TYPE("map|builtin sunlight|null")
		if(value.is_null()) {
			obj.sunlight_.reset();
		} else if(value.is_callable()) {
			obj.sunlight_ = value.try_convert<sunlight>();
		} else {
			obj.sunlight_.reset(new sunlight(obj.shader_, value));
		}
	END_DEFINE_CALLABLE(lighting)


	sunlight::sunlight(gles2::program_ptr shader, const variant& node)
		: shader_(shader), enabled_(false)
	{
		if(node.has_key("color")) {
			set_color(graphics::color(node["color"]));
		} else {
			set_color(graphics::color("white"));
		}

		if(node.has_key("direction")) {
			set_direction(variant_to_vec3(node["direction"]));
		} else {
			set_direction(glm::vec3(0,-1,0));
		}

		if(node.has_key("intensity")) {
			set_ambient_intensity(float(node["intensity"].as_decimal().as_float()));
		} else {
			set_ambient_intensity(1.0f);
		}
		configure_uniforms();
	}

	sunlight::~sunlight()
	{
	}

	void sunlight::set_all_uniforms() const
	{
		if(enabled_) { 
			manager m(shader_);
			glUniform1f(u_ambient_intensity_, abmient_intensity_);
			glUniform4fv(u_color_, 1, glm::value_ptr(glm::vec4(color_.r(), color_.g(), color_.b(), color_.a())));
			glUniform3fv(u_direction_, 1, glm::value_ptr(direction_));
		}
	}

	void sunlight::set_ambient_intensity(float f)
	{
		abmient_intensity_ = f;
		if(enabled_) {
			manager m(shader_);
			glUniform1f(u_ambient_intensity_, abmient_intensity_);
		}
	}

	void sunlight::set_color(const graphics::color& color)
	{
		color_ = color;
		if(enabled_) {
			manager m(shader_);
			glUniform4fv(u_color_, 1, glm::value_ptr(glm::vec4(color_.r(), color_.g(), color_.b(), color_.a())));
		}
	}

	void sunlight::set_direction(const glm::vec3& d)
	{
		direction_ = d;
		if(enabled_) {
			manager m(shader_);
			glUniform3fv(u_direction_, 1, glm::value_ptr(direction_));
		}
	}
	
	void sunlight::configure_uniforms()
	{
		u_color_ = shader_->get_fixed_uniform("sunlight.vColor");
		u_ambient_intensity_ = shader_->get_fixed_uniform("sunlight.fAmbientIntensity");
		u_direction_ = shader_->get_fixed_uniform("sunlight.vDirection");

		if(u_color_ != -1 && u_ambient_intensity_ != -1 && u_direction_ != -1) {
			enabled_ = true;
		} else {
			std::cerr << "Sunlight disabled" << std::endl;
		}
	}

	variant sunlight::write()
	{
		variant_builder res;
		if(!(color_ == graphics::color(255,255,255))) {
			res.add("color", color_.write());
		}
		if(direction().x != 0.0f && direction().y != 1.0f && direction().z != 0.0f) {
			res.add("direction", vec3_to_variant(direction()));
		}
		if(ambient_intensity() != 1.0f) {
			res.add("intensity", ambient_intensity());
		}
		return res.build();
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(sunlight)
	DEFINE_FIELD(color, "[decimal|int,decimal|int,decimal|int,decimal|int]")
		return obj.get_color().write();
	DEFINE_SET_FIELD
		obj.set_color(graphics::color(value));
	
	DEFINE_FIELD(direction, "[decimal|int,decimal|int,decimal|int]")
		return vec3_to_variant(obj.direction());
	DEFINE_SET_FIELD
		obj.set_direction(variant_to_vec3(value));

	DEFINE_FIELD(intensity, "decimal|int")
		return variant(obj.ambient_intensity());
	DEFINE_SET_FIELD
		obj.set_ambient_intensity(float(value.as_decimal().as_float()));
	END_DEFINE_CALLABLE(sunlight)
}

#endif
