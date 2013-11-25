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
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <cmath>
#include <chrono>

#include "camera.hpp"
#include "level.hpp"

#include "psystem2.hpp"
#include "psystem2_affectors.hpp"
#include "psystem2_parameters.hpp"
#include "psystem2_emitters.hpp"
#include "spline.hpp"

namespace shader
{
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

namespace graphics
{
	namespace particles
	{
		// This is set to be the frame rate/process interval
		// XXX: This really should be a global system constant somewhere
		const float process_step_time = 1.0f/50.0f;

		namespace 
		{
			std::default_random_engine& get_rng_engine() 
			{
				static std::unique_ptr<std::default_random_engine> res;
				if(res == NULL) {
					unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
					res.reset(new std::default_random_engine(seed));
				}
				return *res;
			}
		}

		void init_physics_parameters(physics_parameters& pp)
		{
			pp.position = glm::vec3(0.0f);
			pp.color = color_vector(255,255,255,255);
			pp.dimensions = glm::vec3(1.0f);
			pp.time_to_live = 10.0f;
			pp.mass = 1.0f;
			pp.velocity = 100.0f;
			pp.direction = glm::vec3(0.0f,1.0f,0.0f);
			pp.orientation = glm::quat(1.0f,0.0f,0.0f,0.0f);

		}

		float get_random_float(float min, float max)
		{
			std::uniform_real<float> gen(min, max);
			return gen(get_rng_engine());
		}

		std::ostream& operator<<(std::ostream& os, const glm::vec3& v)
		{
			os << "[" << v.x << "," << v.y << "," << v.z << "]";
			return os;
		}

		std::ostream& operator<<(std::ostream& os, const glm::vec4& v)
		{
			os << "[" << v.x << "," << v.y << "," << v.z << "," << v.w << "]";
			return os;
		}

		std::ostream& operator<<(std::ostream& os, const color_vector& c)
		{
			os << "[" << int(c.r) << "," << int(c.g) << "," << int(c.b) << "," << int(c.a) << "]";
			return os;
		}

		std::ostream& operator<<(std::ostream& os, const particle& p) 
		{
			/*os << "P"<< p.current.position 
				<< ", IP" << p.initial_position 
				<< ", DIM" << p.current.dimensions 
				<< ", DIR" << p.current.direction 
				<< ", TTL(" << p.current.time_to_live << ")" 
				<< ", ITTL(" <<  p.initial_time_to_live << ")"
				<< ", C" << p.current.color
				<< ", M(" << p.current.mass << ")"
				<< ", V(" << p.current.velocity << ")"
				<< ", A(" << p.current.acceleration << ")";*/
			return os;
		}

		particle_system::particle_system(const variant& node, game_logic::formula_callable* environment)
			: widget(node, environment), elapsed_time_(0.0f), scale_velocity_(1.0f), scale_time_(1.0f),
			scale_dimensions_(1.0f)
		{
			ASSERT_LOG(node.has_key("shader"), "Must supply a shader to draw particles with.");
			shader_ = gles2::shader_program::get_global(node["shader"].as_string())->shader();
			ASSERT_LOG(shader_ != NULL, "FATAL: PSYSTEM2: No shader given.");

			ASSERT_LOG(node.has_key("technique"), "FATAL: PSYSTEM2: Must have a list of techniques to create particles.");
			ASSERT_LOG(node["technique"].is_map() || node["technique"].is_list(), "FATAL: PSYSTEM2: 'technique' attribute must be map or list.");
			if(node["technique"].is_map()) {
				techniques_.push_back(technique_ptr(new technique(node["technique"], this, shader_)));
			} else {
				for(size_t n = 0; n != node["technique"].num_elements(); ++n) {
					techniques_.push_back(technique_ptr(new technique(node["technique"][n], this, shader_)));
				}
			}
			if(node.has_key("fast_forward")) {
				float ff_time = float(node["fast_forward"]["time"].as_decimal(decimal(0.0)).as_float());
				float ff_interval = float(node["fast_forward"]["time"].as_decimal(decimal(0.0)).as_float());
				fast_forward_.reset(new std::pair<float,float>(ff_time, ff_interval));
			}

			if(node.has_key("scale_velocity")) {
				scale_velocity_ = float(node["scale_velocity"].as_decimal().as_float());
			}
			if(node.has_key("scale_time")) {
				scale_time_ = float(node["scale_time"].as_decimal().as_float());
			}
			if(node.has_key("scale")) {
				scale_dimensions_ = variant_to_vec3(node["scale"]);
			}

			if(fast_forward_) {
				for(float t = 0; t < fast_forward_->first; t += fast_forward_->second) {
					update(fast_forward_->second);
					elapsed_time_ += fast_forward_->second;
				}
			}
		}

		particle_system::~particle_system()
		{
		}

		void particle_system::update(float dt)
		{
			for(auto t : techniques_) {
				t->process(dt);
			}
		}

		void particle_system::handle_draw() const
		{
#if defined(USE_SHADERS)
			shader::manager m(shader_);
#endif
			for(auto t : techniques_) {
				t->draw(shader_);
			}
		}

		void particle_system::handle_process()
		{
			update(process_step_time);
			elapsed_time_ += process_step_time;
		}

		technique::technique(const variant& node, particle_system* ps, gles2::program_ptr shader)
			: default_particle_width_(node["default_particle_width"].as_decimal(decimal(1.0)).as_float()),
			default_particle_height_(node["default_particle_height"].as_decimal(decimal(1.0)).as_float()),
			default_particle_depth_(node["default_particle_depth"].as_decimal(decimal(1.0)).as_float()),
			lod_index_(node["lod_index"].as_int(0)), velocity_(1.0f), particle_system_(ps)
		{
			ASSERT_LOG(ps != NULL, "FATAL: PSYSTEM2: particle_system was null");
			ASSERT_LOG(node.has_key("visual_particle_quota"), "FATAL: PSYSTEM2: 'technique' must have 'visual_particle_quota' attribute.");
			particle_quota_ = node["visual_particle_quota"].as_int();
			ASSERT_LOG(node.has_key("material"), "FATAL: PSYSTEM2: 'technique' must have 'material' attribute.");
			material_.reset(new material(node["material"]));
			//ASSERT_LOG(node.has_key("renderer"), "FATAL: PSYSTEM2: 'technique' must have 'renderer' attribute.");
			//renderer_.reset(new renderer(node["renderer"]));
			if(node.has_key("emitter")) {
				if(node["emitter"].is_map()) {
					emitters_.push_back(emitter::factory(node["emitter"], this));
				} else if(node["emitter"].is_list()) {
					for(size_t n = 0; n != node["emitter"].num_elements(); ++n) {
						emitters_.push_back(emitter::factory(node["emitter"][n], this));
					}
				} else {
					ASSERT_LOG(false, "FATAL: PSYSTEM2: 'emitter' attribute must be a list or map.");
				}
			}
			if(node.has_key("affector")) {
				if(node["affector"].is_map()) {
					affectors_.push_back(affector::factory(node["affector"], this));
				} else if(node["affector"].is_list()) {
					for(size_t n = 0; n != node["affector"].num_elements(); ++n) {
						affectors_.push_back(affector::factory(node["affector"][n], this));
					}
				} else {
					ASSERT_LOG(false, "FATAL: PSYSTEM2: 'affector' attribute must be a list or map.");
				}
			}
			if(node.has_key("max_velocity")) {
				max_velocity_.reset(new float(node["max_velocity"].as_decimal().as_float()));
			}

			// In order to create as few re-allocations of particles, reserve space here
			active_particles_.reserve(particle_quota_);

			a_dimensions_ = shader->get_fixed_attribute("dimensions");
			ASSERT_LOG(a_dimensions_ != -1, "FATAL: PSYSTEM2: No shader 'dimensions' attribute found.");
		}

		technique::~technique()
		{
		}

		void technique::process(float t)
		{
			// run emitters
			for(auto e : emitters_) {
				std::vector<particle>::iterator start, end;
				e->process(active_particles_, t, start, end);
			}

			// Decrement the ttl on particles
			for(auto& p : active_particles_) {
				p.current.time_to_live -= process_step_time;
			}

			// Kill end-of-life particles
			active_particles_.erase(std::remove_if(active_particles_.begin(), active_particles_.end(),
				[](decltype(active_particles_[0]) p){return p.current.time_to_live < 0.0f;}), 
				active_particles_.end());

			// Run affectors.
			for(auto a : affectors_) {
				a->apply(active_particles_, emitters_, t);
			}

			// update particle positions
			for(auto& p : active_particles_) {
				if(max_velocity_ && p.current.velocity*glm::length(p.current.direction) > *max_velocity_) {
					p.current.direction *= *max_velocity_ / glm::length(p.current.direction);
				}

				p.current.position += p.current.direction * /*scale_velocity * */ t;

				//std::cerr << p << std::endl;
			}

			//std::cerr << "XXX: Active Particle Count: " << active_particles_.size() << std::endl;
		}

		void technique::draw(gles2::program_ptr shader) const
		{
			if(material_) {
				material_->apply();
			}

			GLuint mvp_uniform = -1;
			if(mvp_uniform == -1) {
				mvp_uniform = gles2::active_shader()->shader()->get_fixed_uniform("mvp_matrix");
			}
			glm::mat4 mvp = level::current().camera()->projection_mat() * level::current().camera()->view_mat();
			glUniformMatrix4fv(mvp_uniform, 1, GL_FALSE, glm::value_ptr(mvp));

#if defined(USE_SHADERS)
			shader->vertex_array(3, GL_FLOAT, GL_FALSE, sizeof(particle), &active_particles_[0].current.position);
			shader->color_array(4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(particle), &active_particles_[0].current.color);
			shader->vertex_attrib_array(a_dimensions_, 3, GL_FLOAT, GL_FALSE, sizeof(particle), &active_particles_[0].current.dimensions);
			glDrawArrays(GL_POINTS, 0, active_particles_.size());
#endif
			if(material_) {
				material_->unapply();
			}
		}

		material::material(const variant& node)
			: name_(node["name"].as_string())
		{
			// XXX: technically a material could have multiple technique's and passes -- ignoring for now.
			ASSERT_LOG(node.has_key("technique"), "FATAL: PSYSTEM2: 'material' must have 'technique' attribute.");
			ASSERT_LOG(node["technique"].has_key("pass"), "FATAL: PSYSTEM2: 'material' must have 'pass' attribute.");
			const variant& pass = node["technique"]["pass"];
			use_lighting_ = pass["lighting"].as_bool(false);
			use_fog_ = pass["fog_override"].as_bool(false);
			do_depth_write_ = pass["depth_write"].as_bool(true);
			do_depth_check_ = pass["depth_check"].as_bool(true);
			blend_.sfactor = GL_SRC_ALPHA;
			blend_.dfactor = GL_ONE_MINUS_SRC_ALPHA;
			if(pass.has_key("scene_blend")) {
				const std::string& blend = pass["scene_blend"].as_string();
				if(blend == "add") {
					blend_.sfactor = GL_ONE;
					blend_.dfactor = GL_ONE;
				} else if(blend == "alpha_blend") {
					// leave as defaults.
				} else if(blend == "colour_blend") {
					blend_.sfactor = GL_SRC_COLOR;
					blend_.dfactor = GL_ONE_MINUS_SRC_COLOR;
				} else if(blend == "modulate") {
					blend_.sfactor = GL_DST_COLOR;
					blend_.dfactor = GL_ZERO;
				} else if(blend == "src_colour one") {
					blend_.sfactor = GL_SRC_COLOR;
					blend_.dfactor = GL_ONE;
				} else if(blend == "src_colour zero") {
					blend_.sfactor = GL_SRC_COLOR;
					blend_.dfactor = GL_ZERO;
				} else if(blend == "src_colour dest_colour") {
					blend_.sfactor = GL_SRC_COLOR;
					blend_.dfactor = GL_DST_COLOR;
				} else if(blend == "dest_colour one") {
					blend_.sfactor = GL_DST_COLOR;
					blend_.dfactor = GL_ONE;
				} else if(blend == "dest_colour src_colour") {
					blend_.sfactor = GL_DST_COLOR;
					blend_.dfactor = GL_SRC_COLOR;
				} else {
					ASSERT_LOG(false, "FATAL: PSYSTEM2: Unrecognised scene_blend mode " << blend);
				}
			}
			if(pass.has_key("texture_unit")) {
				if(pass["texture_unit"].is_map()) {
					tex_.push_back(texture::get(pass["texture_unit"]["texture"].as_string()));
					// XXX Deal with:
					// tex_address_mode clamp
					// wave_xform scroll_x sine 0 0.3 0 0.15
					// filtering none
					// See http://www.ogre3d.org/docs/manual/manual_17.html#Texture-Units for a full list.
				} else if(pass["texture_unit"].is_list()) {
					for(size_t n = 0; n != pass["texture_unit"].num_elements(); ++n) {
						tex_.push_back(texture::get(pass["texture_unit"]["texture"][n].as_string()));
					}
				} else {
					ASSERT_LOG(false, "FATAL: PSYSTEM2: 'texture_unit' attribute must be map or list ");
				}
			}
		}

		material::~material()
		{
		}

		void material::apply() const
		{
			if(tex_.size() >= 1) {
				tex_[0].set_as_current_texture();
				// set more textures here...
			}
			// use_lighting_ 
			// use_fog_
			// do_depth_write_
			// do_depth_check_
			// blend_
			if(do_depth_check_) {
				glEnable(GL_DEPTH_TEST);
			}
			glBlendFunc(blend_.sfactor, blend_.dfactor);
		}

		void material::unapply() const
		{
			if(do_depth_check_) {
				glDisable(GL_DEPTH_TEST);
			}
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}


		BEGIN_DEFINE_CALLABLE(particle_system, widget)
		DEFINE_FIELD(dummy, "null")
			return variant();
		END_DEFINE_CALLABLE(particle_system)
	}
}
