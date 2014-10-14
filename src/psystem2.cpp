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

#include "graphics.hpp"

#include "psystem2.hpp"
#include "psystem2_affectors.hpp"
#include "psystem2_parameters.hpp"
#include "psystem2_emitters.hpp"
#include "spline.hpp"

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
			std::uniform_real_distribution<float> gen(min, max);
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

		std::ostream& operator<<(std::ostream& os, const glm::quat& v)
		{
			os << "[" << v.w << "," << v.x << "," << v.y << "," << v.z << "]";
			return os;
		}

		std::ostream& operator<<(std::ostream& os, const color_vector& c)
		{
			os << "[" << int(c.r) << "," << int(c.g) << "," << int(c.b) << "," << int(c.a) << "]";
			return os;
		}

		std::ostream& operator<<(std::ostream& os, const particle& p) 
		{
			os << "P"<< p.current.position 
				<< ", IP" << p.initial.position 
				<< ", DIM" << p.current.dimensions 
				<< ", DIR" << p.current.direction 
				<< ", TTL(" << p.current.time_to_live << ")" 
				<< ", ITTL(" <<  p.initial.time_to_live << ")"
				<< ", C" << p.current.color
				<< ", M(" << p.current.mass << ")"
				<< ", V(" << p.current.velocity << ")"
				<< std::endl
				<< "\tO(" << p.current.orientation << ")"
				<< "\tIO(" << p.initial.orientation << ")"
				;
			return os;
		}

		// Compute any vector out of the infinite set perpendicular to v.
		glm::vec3 perpendicular(const glm::vec3& v) 
		{
			glm::vec3 perp = glm::cross(v, glm::vec3(1.0f,0.0f,0.0f));
			float len_sqr = perp.x*perp.x + perp.y*perp.y + perp.z*perp.z;
			if(len_sqr < 1e-12) {
				perp = glm::cross(v, glm::vec3(0.0f,1.0f,0.0f));
			}
			float len = glm::length(perp);
			if(len > 1e-14f) {
				return perp / len;
			}
			return perp;
		}

		glm::vec3 create_deviating_vector(float angle, const glm::vec3& v, const glm::vec3& up)
		{
			glm::vec3 up_up = up;
			if(up == glm::vec3(0.0f)) {
				up_up = perpendicular(v);
			}
			glm::quat q = glm::angleAxis(get_random_float(0.0f,360.0f), v);
			up_up = q * up_up;

			q = glm::angleAxis(angle, up_up);
			return q * v;
		}

		particle_system_widget::particle_system_widget(const variant& node, game_logic::formula_callable* environment)
			: widget(node, environment), particle_systems_(new particle_system_container(node))
		{
		}

		particle_system_widget::~particle_system_widget()
		{
		}

		void particle_system_widget::handle_draw() const
		{
			glTranslatef(x(), y(), 0.0f);
			particle_systems_->draw();
		}

		void particle_system_widget::handle_process()
		{
			particle_systems_->process();
		}

		particle_system::particle_system(particle_system_container* parent, const variant& node)
			: emit_object(parent, node), 
			elapsed_time_(0.0f), 
			scale_velocity_(1.0f), 
			scale_time_(1.0f),
			scale_dimensions_(1.0f)
		{
			ASSERT_LOG(node.has_key("shader"), "Must supply a shader to draw particles with.");
			shader_ = gles2::shader_program::get_global(node["shader"].as_string())->shader();
			ASSERT_LOG(shader_ != NULL, "FATAL: PSYSTEM2: No shader given.");

			ASSERT_LOG(node.has_key("technique"), "FATAL: PSYSTEM2: Must have a list of techniques to create particles.");
			ASSERT_LOG(node["technique"].is_map() || node["technique"].is_list(), "FATAL: PSYSTEM2: 'technique' attribute must be map or list.");
			if(node["technique"].is_map()) {
				parent_container()->add_technique(new technique(parent, node["technique"]));
			} else {
				for(size_t n = 0; n != node["technique"].num_elements(); ++n) {
					parent_container()->add_technique(new technique(parent, node["technique"][n]));
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
			// XXX: process "active_techniques" here
			if(node.has_key("active_techniques")) {
				if(node["active_techniques"].is_list()) {
					for(size_t n = 0; n != node["active_techniques"].num_elements(); ++n) {
						active_techniques_.push_back(parent_container()->clone_technique(node["active_techniques"][n].as_string()));
						active_techniques_.back()->set_parent(this);
						active_techniques_.back()->set_shader(shader_);
					}
				} else if(node["active_techniques"].is_string()) {
					active_techniques_.push_back(parent_container()->clone_technique(node["active_techniques"].as_string()));
					active_techniques_.back()->set_parent(this);
					active_techniques_.back()->set_shader(shader_);
				} else {
					ASSERT_LOG(false, "FATAL: PSYSTEM2: 'active_techniques' attribute must be list of strings or single string.");
				}
			} else {
				active_techniques_ = parent_container()->clone_techniques();
				for(auto tq : active_techniques_) {
					tq->set_parent(this);
					tq->set_shader(shader_);
				}
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

		particle_system::particle_system(const particle_system& ps)
			: emit_object(ps),
			elapsed_time_(0),
			scale_velocity_(ps.scale_velocity_),
			scale_time_(ps.scale_time_),
			scale_dimensions_(ps.scale_dimensions_),
			shader_(ps.shader_)
		{
			if(ps.fast_forward_) {
				fast_forward_.reset(new std::pair<float,float>(ps.fast_forward_->first, ps.fast_forward_->second));
			}
			for(auto tq : ps.active_techniques_) {
				active_techniques_.push_back(technique_ptr(new technique(*tq)));
			}
		}

		void particle_system::update(float dt)
		{
			for(auto t : active_techniques_) {
				t->process(dt);
			}
		}

		void particle_system::handle_draw() const
		{
#if defined(USE_SHADERS)
			shader::manager m(shader_);
#endif
			for(auto t : active_techniques_) {
				t->draw();
			}
		}

		void particle_system::handle_process(float t)
		{
			update(t);
			elapsed_time_ += t;
		}

		void particle_system::add_technique(technique_ptr tq)
		{
			active_techniques_.push_back(tq);
			tq->set_parent(this);
			tq->set_shader(shader_);
		}

		particle_system* particle_system::factory(particle_system_container* parent, const variant& node)
		{
			return new particle_system(parent, node);
		}

		technique::technique(particle_system_container* parent, const variant& node)
			: emit_object(parent, node), default_particle_width_(node["default_particle_width"].as_decimal(decimal(1.0)).as_float()),
			default_particle_height_(node["default_particle_height"].as_decimal(decimal(1.0)).as_float()),
			default_particle_depth_(node["default_particle_depth"].as_decimal(decimal(1.0)).as_float()),
			lod_index_(node["lod_index"].as_int(0)), velocity_(1.0f),
			emitter_quota_(node["emitted_emitter_quota"].as_int(50)),
			affector_quota_(node["emitted_affector_quota"].as_int(10)),
			technique_quota_(node["emitted_technique_quota"].as_int(10)),
			system_quota_(node["emitted_system_quota"].as_int(10))
		{
			ASSERT_LOG(node.has_key("visual_particle_quota"), "FATAL: PSYSTEM2: 'technique' must have 'visual_particle_quota' attribute.");
			particle_quota_ = node["visual_particle_quota"].as_int();
			ASSERT_LOG(node.has_key("material"), "FATAL: PSYSTEM2: 'technique' must have 'material' attribute.");
			material_.reset(new material(node["material"]));
			//ASSERT_LOG(node.has_key("renderer"), "FATAL: PSYSTEM2: 'technique' must have 'renderer' attribute.");
			//renderer_.reset(new renderer(node["renderer"]));
			if(node.has_key("emitter")) {
				if(node["emitter"].is_map()) {
					parent_container()->add_emitter(emitter::factory(parent, node["emitter"]));
				} else if(node["emitter"].is_list()) {
					for(size_t n = 0; n != node["emitter"].num_elements(); ++n) {
						parent_container()->add_emitter(emitter::factory(parent, node["emitter"][n]));
					}
				} else {
					ASSERT_LOG(false, "FATAL: PSYSTEM2: 'emitter' attribute must be a list or map.");
				}
			}
			if(node.has_key("affector")) {
				if(node["affector"].is_map()) {
					parent_container()->add_affector(affector::factory(parent, node["affector"]));
				} else if(node["affector"].is_list()) {
					for(size_t n = 0; n != node["affector"].num_elements(); ++n) {
						parent_container()->add_affector(affector::factory(parent, node["affector"][n]));
					}
				} else {
					ASSERT_LOG(false, "FATAL: PSYSTEM2: 'affector' attribute must be a list or map.");
				}
			}
			if(node.has_key("max_velocity")) {
				max_velocity_.reset(new float(node["max_velocity"].as_decimal().as_float()));
			}

			// conditional addition of emitters/affectors
			if(node.has_key("active_emitters")) {
				std::vector<std::string> active_emitters = node["active_emitters"].as_list_string();
				for(auto e : active_emitters) {
					auto em = parent_container()->clone_emitter(e);
					active_emitters_.push_back(em);
					em->set_parent_technique(this);
				}
			} else {
				for(auto es : parent_container()->clone_emitters()) {
					active_emitters_.push_back(es);
					es->set_parent_technique(this);
				}
			}
			if(node.has_key("active_affectors")) {
				std::vector<std::string> active_affectors = node["active_affectors"].as_list_string();
				for(auto a : active_affectors) {
					auto aff = parent_container()->clone_affector(a);
					active_affectors_.push_back(aff);
					aff->set_parent_technique(this);
				}
			} else {
				for(auto as : parent_container()->clone_affectors()) {
					active_affectors_.push_back(as);
					as->set_parent_technique(this);
				}
			}

			// In order to create as few re-allocations of particles, reserve space here
			active_particles_.reserve(particle_quota_);
		}

		technique::~technique()
		{
		}

		technique::technique(const technique& tq) 
			: emit_object(tq),
			default_particle_width_(tq.default_particle_width_),
			default_particle_height_(tq.default_particle_height_),
			default_particle_depth_(tq.default_particle_depth_),
			particle_quota_(tq.particle_quota_),
			emitter_quota_(tq.emitter_quota_),
			affector_quota_(tq.affector_quota_),
			technique_quota_(tq.technique_quota_),
			system_quota_(tq.system_quota_),
			lod_index_(tq.lod_index_),
			velocity_(tq.velocity_),
			material_(tq.material_),
			particle_system_(tq.particle_system_),
			a_dimensions_(tq.a_dimensions_),
			shader_(tq.shader_)			
		{
			if(tq.max_velocity_) {
				max_velocity_.reset(new float(*tq.max_velocity_));
			}
			// XXX I'm not sure this should clone all the currently active 
			// emitters/affectors, or whether we should maintain a list of 
			// emitters/affectors that were initially specified.
			for(auto e : tq.active_emitters_) {
				active_emitters_.push_back(emitter_ptr(e->clone()));
				active_emitters_.back()->set_parent_technique(this);
			}
			for(auto a : tq.active_affectors_) {
				active_affectors_.push_back(affector_ptr(a->clone()));
				active_affectors_.back()->set_parent_technique(this);
			}
			active_particles_.reserve(particle_quota_);
		}

		void technique::set_parent(particle_system* parent)
		{
			ASSERT_LOG(parent != NULL, "FATAL: PSYSTEM2: parent is null");
			particle_system_ = parent;
		}

		void technique::set_shader(gles2::program_ptr shader)
		{
			ASSERT_LOG(shader != NULL, "FATAL: PSYSTEM2: shader is null");
			shader_ = shader;

			a_dimensions_ = shader_->get_fixed_attribute("dimensions");
			ASSERT_LOG(a_dimensions_ != -1, "FATAL: PSYSTEM2: No shader 'dimensions' attribute found.");
		}

		void technique::add_emitter(emitter_ptr e) 
		{
			e->set_parent_technique(this);
			//active_emitters_.push_back(e);
			instanced_emitters_.push_back(e);
		}

		void technique::add_affector(affector_ptr a) 
		{
			a->set_parent_technique(this);
			//active_affectors_.push_back(a);
			instanced_affectors_.push_back(a);
		}


		void technique::handle_process(float t)
		{
			// run objects
			for(auto e : active_emitters_) {
				e->process(t);
			}
			for(auto e : instanced_emitters_) {
				e->process(t);
			}
			for(auto a : active_affectors_) {
				a->process(t);
			}
			for(auto a : instanced_affectors_) {
				a->process(t);
			}

			// Decrement the ttl on particles
			for(auto& p : active_particles_) {
				p.current.time_to_live -= process_step_time;
			}
			// Decrement the ttl on instanced emitters
			for(auto e : instanced_emitters_) {
				e->current.time_to_live -= process_step_time;
			}

			// Kill end-of-life particles
			active_particles_.erase(std::remove_if(active_particles_.begin(), active_particles_.end(),
				[](decltype(active_particles_[0]) p){return p.current.time_to_live < 0.0f;}), 
				active_particles_.end());
			// Kill end-of-life emitters
			instanced_emitters_.erase(std::remove_if(instanced_emitters_.begin(), instanced_emitters_.end(),
				[](decltype(instanced_emitters_[0]) e){return e->current.time_to_live < 0.0f;}), 
				instanced_emitters_.end());


			for(auto e : instanced_emitters_) {
				if(max_velocity_ && e->current.velocity*glm::length(e->current.direction) > *max_velocity_) {
					e->current.direction *= *max_velocity_ / glm::length(e->current.direction);
				}
				e->current.position += e->current.direction * /*scale_velocity * */ t;
				//std::cerr << *e << std::endl;
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
			//std::cerr << "XXX: Active Emitter Count: " << active_emitters_.size() << std::endl;
		}

		void technique::handle_draw() const
		{
			ASSERT_LOG(shader_ != NULL, "FATAL: PSYSTEM2: shader_ not set before draw called.");
			if(material_) {
				material_->apply();
			}

			GLuint mvp_uniform = -1;
			if(mvp_uniform == -1) {
				mvp_uniform = gles2::active_shader()->shader()->get_fixed_uniform("mvp_matrix");
			}
			//glm::mat4 mvp = level::current().camera()->projection_mat() * level::current().camera()->view_mat();
			//glm::mat4 mvp = get_main_window()->camera()->projection_mat() * get_main_window()->camera()->view_mat();
			glm::mat4 mvp = gles2::get_mvp_matrix();
			glUniformMatrix4fv(mvp_uniform, 1, GL_FALSE, glm::value_ptr(mvp));
			

			for(auto e :  active_emitters_) {
				e->draw();
			}
			for(auto e : instanced_emitters_) {
				e->draw();
			}

#if defined(USE_SHADERS)
			shader_->vertex_array(3, GL_FLOAT, GL_FALSE, sizeof(particle), &active_particles_[0].current.position);
			shader_->color_array(4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(particle), &active_particles_[0].current.color);
			shader_->vertex_attrib_array(a_dimensions_, 3, GL_FLOAT, GL_FALSE, sizeof(particle), &active_particles_[0].current.dimensions);
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

		particle_system_container::particle_system_container(const variant& node) 
		{
			if(node.has_key("systems")) {
				if(node["systems"].is_list()) {
					for(size_t n = 0; n != node["systems"].num_elements(); ++n) {
						add_particle_system(particle_system::factory(this, node["systems"][n]));
					}
				} else if(node["systems"].is_map()) {
					add_particle_system(particle_system::factory(this, node["systems"]));
				} else {
					ASSERT_LOG(false, "FATAL: PSYSTEM2: unrecognised type for 'systems' attribute must be list or map");
				}
			} else {
				add_particle_system(particle_system::factory(this, node));
			}

			if(node.has_key("active_systems")) {
				if(node["active_systems"].is_list()) {
					for(size_t n = 0; n != node["active_systems"].num_elements(); ++n) {
						active_particle_systems_.push_back(clone_particle_system(node["active_systems"][n].as_string()));
					}
				} else if(node["active_systems"].is_string()) {
					active_particle_systems_.push_back(clone_particle_system(node["active_systems"].as_string()));
				} else {
					ASSERT_LOG(false, "FATAL: PSYSTEM2: 'active_systems' attribute must be a string or list of strings.");
				}
			} else {
				active_particle_systems_ = clone_particle_systems();
			}
		}

		particle_system_container::~particle_system_container()
		{
		}

		variant particle_system_container::get_ffl_particle_systems() const
		{
			std::vector<variant> res;
			res.reserve(particle_systems_.size());
			for(auto p : particle_systems_) {
				res.push_back(variant(p.get()));
			}

			return variant(&res);
		}

		variant particle_system_container::get_ffl_techniques() const
		{
			std::vector<variant> res;
			res.reserve(techniques_.size());
			for(auto p : techniques_) {
				res.push_back(variant(p.get()));
			}

			return variant(&res);
		}

		variant particle_system_container::get_ffl_emitters() const
		{
			std::vector<variant> res;
			res.reserve(emitters_.size());
			for(auto p : emitters_) {
				res.push_back(variant(p.get()));
			}

			return variant(&res);
		}

		variant particle_system_container::get_ffl_affectors() const
		{
			std::vector<variant> res;
			res.reserve(affectors_.size());
			for(auto p : affectors_) {
				res.push_back(variant(p.get()));
			}

			return variant(&res);
		}

		void particle_system_container::set_ffl_particle_systems(variant value)
		{
			particle_systems_.clear();
			active_particle_systems_.clear();

			std::vector<variant> v = value.as_list();
			for(variant a : v) {
				add_particle_system(a.try_convert<particle_system>());
			}

			//if we set the ffl particle systems by default just make
			//them all active. Figure out a way to deactivate some later.
			active_particle_systems_ = particle_systems_;
		}

		void particle_system_container::set_ffl_techniques(variant value)
		{
			techniques_.clear();
			std::vector<variant> v = value.as_list();
			for(variant a : v) {
				add_technique(a.try_convert<technique>());
			}
		}

		void particle_system_container::set_ffl_emitters(variant value)
		{
			emitters_.clear();
			std::vector<variant> v = value.as_list();
			for(variant a : v) {
				add_emitter(a.try_convert<emitter>());
			}
		}

		void particle_system_container::set_ffl_affectors(variant value)
		{
			affectors_.clear();
			std::vector<variant> v = value.as_list();
			for(variant a : v) {
				add_affector(a.try_convert<affector>());
			}
		}

		void particle_system_container::draw() const
		{
			for(auto ps : active_particle_systems_) {
				ps->draw();
			}
		}

		void particle_system_container::process()
		{
			for(auto ps : active_particle_systems_) {
				ps->process(process_step_time);
			}
		}

		void particle_system_container::add_particle_system(particle_system* obj)
		{
			particle_systems_.push_back(particle_system_ptr(obj));
		}

		void particle_system_container::add_technique(technique* obj)
		{
			techniques_.push_back(technique_ptr(obj));
		}

		void particle_system_container::add_emitter(emitter* obj)
		{
			emitters_.push_back(emitter_ptr(obj));
		}

		void particle_system_container::add_affector(affector* obj) 
		{
			affectors_.push_back(affector_ptr(obj));
		}

		void particle_system_container::activate_particle_system(const std::string& name)
		{
			active_particle_systems_.push_back(clone_particle_system(name));
		}

		particle_system_ptr particle_system_container::clone_particle_system(const std::string& name)
		{
			for(auto ps : particle_systems_) {
				if(ps->name() == name) {
					return particle_system_ptr(new particle_system(*ps));
				}
			}
			ASSERT_LOG(false, "FATAL: PSYSTEM2: particle_system not found: " << name);
			return particle_system_ptr();
		}

		technique_ptr particle_system_container::clone_technique(const std::string& name)
		{
			for(auto tq : techniques_) {
				if(tq->name() == name) {
					return technique_ptr(new technique(*tq));
				}
			}
			ASSERT_LOG(false, "FATAL: PSYSTEM2: technique not found: " << name);
			return technique_ptr();
		}

		emitter_ptr particle_system_container::clone_emitter(const std::string& name)
		{
			for(auto e : emitters_) {
				if(e->name() == name) {
					return emitter_ptr(e->clone());
				}
			}
			ASSERT_LOG(false, "FATAL: PSYSTEM2: emitter not found: " << name);
			return emitter_ptr();
		}

		affector_ptr particle_system_container::clone_affector(const std::string& name)
		{
			for(auto a : affectors_) {
				if(a->name() == name) {
					return affector_ptr(a->clone());
				}
			}
			ASSERT_LOG(false, "FATAL: PSYSTEM2: affector not found: " << name);
			return affector_ptr();
		}

		std::vector<particle_system_ptr> particle_system_container::clone_particle_systems()
		{
			std::vector<particle_system_ptr> res;
			for(auto ps : particle_systems_) {
				res.push_back(particle_system_ptr(new particle_system(*ps)));
			}
			return res;
		}
		
		std::vector<technique_ptr> particle_system_container::clone_techniques()
		{
			std::vector<technique_ptr> res;
			for(auto tq : techniques_) {
				res.push_back(technique_ptr(new technique(*tq)));
			}
			return res;
		}

		std::vector<emitter_ptr> particle_system_container::clone_emitters()
		{
			std::vector<emitter_ptr> res;
			for(auto e : emitters_) {
				res.push_back(emitter_ptr(e->clone()));
			}
			return res;
		}

		std::vector<affector_ptr> particle_system_container::clone_affectors()
		{
			std::vector<affector_ptr> res;
			for(auto a : affectors_) {
				res.push_back(affector_ptr(a->clone()));
			}
			return res;
		}

		BEGIN_DEFINE_CALLABLE_NOBASE(emit_object)
		DEFINE_FIELD(dummy, "null")
			return variant();
		END_DEFINE_CALLABLE(emit_object)

		BEGIN_DEFINE_CALLABLE_NOBASE(particle_system_container)
		DEFINE_FIELD(dummy, "null")
			return variant();
		END_DEFINE_CALLABLE(particle_system_container)

		BEGIN_DEFINE_CALLABLE(particle_system_widget, widget)
		DEFINE_FIELD(particle_systems, "[builtin particle_system]")
			return obj.particle_systems_->get_ffl_particle_systems();
		DEFINE_SET_FIELD
			obj.particle_systems_->set_ffl_particle_systems(value);
		DEFINE_FIELD(techniques, "[builtin technique]")
			return obj.particle_systems_->get_ffl_techniques();
		DEFINE_SET_FIELD
			obj.particle_systems_->set_ffl_techniques(value);
		DEFINE_FIELD(emitters, "[builtin emitter]")
			return obj.particle_systems_->get_ffl_emitters();
		DEFINE_SET_FIELD
			obj.particle_systems_->set_ffl_emitters(value);
		DEFINE_FIELD(affectors, "[builtin affector]")
			return obj.particle_systems_->get_ffl_affectors();
		DEFINE_SET_FIELD
			obj.particle_systems_->set_ffl_affectors(value);

		//from FFL users can use these to construct particle system objects.
		BEGIN_DEFINE_FN(create_particle_system, "(map) -> builtin particle_system")
			return variant(particle_system::factory(obj.particle_systems_.get(), FN_ARG(0)));
		END_DEFINE_FN
		BEGIN_DEFINE_FN(create_technique, "(map) -> builtin technique")
			return variant(new technique(obj.particle_systems_.get(), FN_ARG(0)));
		END_DEFINE_FN
		BEGIN_DEFINE_FN(create_emitter, "(map) -> builtin emitter")
			return variant(emitter::factory(obj.particle_systems_.get(), FN_ARG(0)));
		END_DEFINE_FN
		BEGIN_DEFINE_FN(create_affector, "(map) -> builtin affector")
			return variant(affector::factory(obj.particle_systems_.get(), FN_ARG(0)));
		END_DEFINE_FN
		END_DEFINE_CALLABLE(particle_system_widget)

		BEGIN_DEFINE_CALLABLE(particle_system, emit_object)
		DEFINE_FIELD(dummy, "null")
			return variant();
		END_DEFINE_CALLABLE(particle_system)

		BEGIN_DEFINE_CALLABLE(technique, emit_object)
		DEFINE_FIELD(dummy, "null")
			return variant();
		END_DEFINE_CALLABLE(technique)
	}
}
