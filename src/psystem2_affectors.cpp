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

#include "asserts.hpp"
#include "psystem2.hpp"
#include "psystem2_affectors.hpp"
#include "psystem2_emitters.hpp"
#include "psystem2_parameters.hpp"

namespace graphics
{
	namespace particles
	{
		class time_color_affector : public affector
		{
		public:
			explicit time_color_affector(particle_system_container* parent, const variant& node);
			virtual ~time_color_affector() {}
		protected:
			virtual void internal_apply(particle& p, float t);
			virtual affector* clone() {
				return new time_color_affector(*this);
			}
		private:
			enum ColourOperation {
				COLOR_OP_SET,
				COLOR_OP_MULTIPLY,
			};
			ColourOperation operation_;
			typedef std::pair<float,glm::vec4> tc_pair;
			std::vector<tc_pair> tc_data_;

			std::vector<tc_pair>::iterator find_nearest_color(float dt);

			time_color_affector();
		};

		class jet_affector : public affector
		{
		public:
			explicit jet_affector(particle_system_container* parent, const variant& node);
			virtual ~jet_affector() {}
		protected:
			virtual void internal_apply(particle& p, float t);
			virtual affector* clone() {
				return new jet_affector(*this);
			}
		private:
			parameter_ptr acceleration_;
			jet_affector();
		};
		// affectors to add: box_collider (width,height,depth, inner or outer collide, friction)
		// flock_centering.
		// forcefield (delta, force, octaves, frequency, amplitude, persistence, size, worldsize(w,h,d), movement(x,y,z),movement_frequency)
		// geometry_rotator (use own rotation, speed(parameter), axis(x,y,z))
		// inter_particle_collider (sounds like a lot of calculations)
		// line
		// linear_force
		// path_follower
		// plane_collider
		// scale_velocity (parameter_ptr scale; bool since_system_start, bool stop_at_flip)
		// sine_force
		// sphere_collider
		// texture_animator
		// texture_rotator
		// velocity matching

		class scale_affector : public affector
		{
		public:
			explicit scale_affector(particle_system_container* parent, const variant& node);
			virtual ~scale_affector() {}
		protected:
			virtual void internal_apply(particle& p, float t);
			virtual affector* clone() {
				return new scale_affector(*this);
			}
		private:
			parameter_ptr scale_x_;
			parameter_ptr scale_y_;
			parameter_ptr scale_z_;
			parameter_ptr scale_xyz_;
			bool since_system_start_;
			float calculate_scale(parameter_ptr s, const particle& p);
			scale_affector();
		};

		class vortex_affector : public affector
		{
		public:
			explicit vortex_affector(particle_system_container* parent, const variant& node);
			virtual ~vortex_affector() {}
		protected:
			virtual void internal_apply(particle& p, float t);
			virtual affector* clone() {
				return new vortex_affector(*this);
			}
		private:
			glm::quat rotation_axis_;
			parameter_ptr rotation_speed_;
			vortex_affector();
		};

		class gravity_affector : public affector
		{
		public:
			explicit gravity_affector(particle_system_container* parent, const variant& node);
			virtual ~gravity_affector()  {}
		protected:
			virtual void internal_apply(particle& p, float t);
			virtual affector* clone() {
				return new gravity_affector(*this);
			}
		private:
			float gravity_;
			gravity_affector();
		};

		class particle_follower_affector : public affector
		{
		public:
			explicit particle_follower_affector(particle_system_container* parent, const variant& node)
				: affector(parent, node),
				min_distance_(node["min_distance"].as_decimal(decimal(1.0)).as_float()),
				max_distance_(node["max_distance"].as_decimal(decimal(std::numeric_limits<float>::max())).as_float()) {
			}
			virtual ~particle_follower_affector() {}
		protected:
			virtual void handle_process(float t) {
				std::vector<particle>& particles = get_technique()->active_particles();
				// keeps particles following wihin [min_distance, max_distance]
				if(particles.size() < 1) {
					return;
				}
				prev_particle_ = particles.begin();
				for(auto p = particles.begin(); p != particles.end(); ++p) {
					internal_apply(*p, t);
					prev_particle_ = p;
				}
			}
			virtual void internal_apply(particle& p, float t) {
				auto distance = glm::length(p.current.position - prev_particle_->current.position);
				if(distance > min_distance_ && distance < max_distance_) {
					p.current.position = prev_particle_->current.position + (min_distance_/distance)*(p.current.position-prev_particle_->current.position);
				}
			}
			virtual affector* clone() {
				return new particle_follower_affector(*this);
			}
		private:
			float min_distance_;
			float max_distance_;
			std::vector<particle>::iterator prev_particle_;
			particle_follower_affector();
		};

		class align_affector : public affector
		{
		public:
			explicit align_affector(particle_system_container* parent, const variant& node) 
				: affector(parent, node), resize_(node["resize"].as_bool(false)) {
			}
			virtual ~align_affector() {}
		protected:
			virtual void internal_apply(particle& p, float t) {
				glm::vec3 distance = prev_particle_->current.position - p.current.position;
				if(resize_) {
					p.current.dimensions.y = glm::length(distance);
				}
				if(std::abs(glm::length(distance)) > 1e-12) {
					distance = glm::normalize(distance);
				}
				p.current.orientation.x = distance.x;
				p.current.orientation.y = distance.y;
				p.current.orientation.z = distance.z;
			}
			virtual void handle_process(float t) {
				std::vector<particle>& particles = get_technique()->active_particles();
				if(particles.size() < 1) {
					return;
				}
				prev_particle_ = particles.begin();				
				for(auto p = particles.begin(); p != particles.end(); ++p) {
					internal_apply(*p, t);
					prev_particle_ = p;
				}
			}
			virtual affector* clone() {
				return new align_affector(*this);
			}
		private:
			bool resize_;			
			std::vector<particle>::iterator prev_particle_;
			align_affector();
		};

		class randomiser_affector : public affector
		{
		public:
			explicit randomiser_affector(particle_system_container* parent, const variant& node) 
				: affector(parent, node), max_deviation_(0.0f), 
				time_step_(float(node["time_step"].as_decimal(decimal(0.0f)).as_float())), 
				random_direction_(node["use_direction"].as_bool(true)) {
				if(node.has_key("max_deviation_x")) {
					max_deviation_.x = float(node["max_deviation_x"].as_decimal().as_float());
				}
				if(node.has_key("max_deviation_y")) {
					max_deviation_.y = float(node["max_deviation_y"].as_decimal().as_float());
				}
				if(node.has_key("max_deviation_z")) {
					max_deviation_.z = float(node["max_deviation_z"].as_decimal().as_float());
				}
				last_update_time_[0] = last_update_time_[1] = 0.0f;
			}
			virtual ~randomiser_affector() {}
		protected:
			virtual void internal_apply(particle& p, float t) {
				if(random_direction_) {
					// change direction per update
					p.current.direction += glm::vec3(get_random_float(-max_deviation_.x, max_deviation_.x),
						get_random_float(-max_deviation_.y, max_deviation_.y),
						get_random_float(-max_deviation_.z, max_deviation_.z));
				} else {
					// change position per update.
					p.current.position += scale() * glm::vec3(get_random_float(-max_deviation_.x, max_deviation_.x),
						get_random_float(-max_deviation_.y, max_deviation_.y),
						get_random_float(-max_deviation_.z, max_deviation_.z));
				}
			}
			void handle_apply(std::vector<particle>& particles, float t) {
				last_update_time_[0] += t;
				if(last_update_time_[0] > time_step_) {
					last_update_time_[0] -= time_step_;
					for(auto& p : particles) {
						internal_apply(p, t);
					}
				}
			}
			void handle_apply(std::vector<emitter_ptr>& objs, float t) {
				last_update_time_[1] += t;
				if(last_update_time_[1] > time_step_) {
					last_update_time_[1] -= time_step_;
					for(auto e : objs) {
						internal_apply(*e, t);
					}
				}
			}
			virtual void handle_process(float t) {
				handle_apply(get_technique()->active_particles(), t);
				handle_apply(get_technique()->active_emitters(), t);
			}
			virtual affector* clone() {
				return new randomiser_affector(*this);
			}
		private:
			// randomiser (bool random_direction_, float time_step_ glm::vec3 max_deviation_)
			bool random_direction_;
			float time_step_;
			glm::vec3 max_deviation_;
			float last_update_time_[2];
			randomiser_affector();
		};

		class sine_force_affector : public affector
		{
		public:
			explicit sine_force_affector(particle_system_container* parent, const variant& node) 
				: affector(parent, node),
				min_frequency_(1.0f),
				max_frequency_(1.0f),
				angle_(0.0f),
				frequency_(1.0f),
				force_vector_(0.0f),
				scale_vector_(0.0f),
				fa_(FA_ADD)
			{
				if(node.has_key("max_frequency")) {
					max_frequency_ = float(node["max_frequency"].as_decimal().as_float());
					frequency_ = max_frequency_;
				}
				if(node.has_key("min_frequency")) {
					min_frequency_ = float(node["min_frequency"].as_decimal().as_float());					
					if(min_frequency_ > max_frequency_) {
						frequency_ = min_frequency_;
					}
				}
				if(node.has_key("force_vector")) {
					force_vector_ = variant_to_vec3(node["force_vector"]);
				}
				if(node.has_key("force_application")) {
					const std::string& fa = node["force_application"].as_string();
					if(fa == "average") {
						fa_ = FA_AVERAGE;
					} else if(fa == "add") {
						fa_ = FA_ADD;
					} else {
						ASSERT_LOG(false, "FATAL: PSYSTEM2: 'force_application' attribute should have value average or add");
					}
				}
			}
			virtual ~sine_force_affector() {}
		protected:
			virtual void handle_process(float t) {
				angle_ += /*2.0f * M_PI **/ frequency_ * t;
				float sine_value = sin(angle_);
				scale_vector_ = force_vector_ * t * sine_value;
				//std::cerr << "XXX: angle: " << angle_ << " scale_vec: " << scale_vector_ << std::endl;
				if(angle_ > M_PI*2.0f) {
					angle_ -= M_PI*2.0f;
					if(min_frequency_ != max_frequency_) {
						frequency_ = get_random_float(min_frequency_, max_frequency_);
					}
				}
				affector::handle_process(t);
			}
			virtual void internal_apply(particle& p, float t) {
				if(fa_ == FA_ADD) {
					p.current.direction += scale_vector_;
				} else {
					p.current.direction = (p.current.direction + force_vector_)/2.0f;
				}
			}
			virtual affector* clone() {
				return new sine_force_affector(*this);
			}
		private:
			enum ForceApplication {
				FA_ADD,
				FA_AVERAGE,
			};
			glm::vec3 force_vector_;
			glm::vec3 scale_vector_;
			float min_frequency_;
			float max_frequency_;
			float angle_;
			float frequency_;
			ForceApplication fa_;
			sine_force_affector();
		};

		affector::affector(particle_system_container* parent, const variant& node)
			: emit_object(parent, node), enabled_(node["enabled"].as_bool(true)), 
			mass_(float(node["mass_affector"].as_decimal(decimal(1.0)).as_float())),
			position_(0.0f), scale_(1.0f)
		{
			if(node.has_key("position")) {
				position_ = variant_to_vec3(node["position"]);
			}
			if(node.has_key("exclude_emitters")) {
				if(node["exclude_emitters"].is_list()) {
					excluded_emitters_ = node["exclude_emitters"].as_list_string();
				} else {
					excluded_emitters_.push_back(node["exclude_emitters"].as_string());
				}
			}
		}
		
		affector::~affector()
		{
		}

		void affector::handle_process(float t) 
		{
			ASSERT_LOG(technique_ != NULL, "FATAL: PSYSTEM2: technique_ is null");
			for(auto& e : technique_->active_emitters()) {
				ASSERT_LOG(e->emitted_by != NULL, "FATAL: PSYSTEM2: e->emitted_by is null");
				if(!is_emitter_excluded(e->emitted_by->name())) {
					internal_apply(*e,t);
				}
			}
			for(auto& p : technique_->active_particles()) {
				ASSERT_LOG(p.emitted_by != NULL, "FATAL: PSYSTEM2: p.emitted_by is null");
				if(!is_emitter_excluded(p.emitted_by->name())) {
					internal_apply(p,t);
				}
			}
		}

		bool affector::is_emitter_excluded(const std::string& name)
		{
			return std::find(excluded_emitters_.begin(), excluded_emitters_.end(), name) != excluded_emitters_.end();
		}

		affector* affector::factory(particle_system_container* parent, const variant& node)
		{
			ASSERT_LOG(node.has_key("type"), "FATAL: PSYSTEM2: affector must have 'type' attribute");
			const std::string& ntype = node["type"].as_string();
			if(ntype == "color" || ntype == "colour") {
				return new time_color_affector(parent, node);
			} else if(ntype == "jet") {
				return new jet_affector(parent, node);
			} else if(ntype == "vortex") {
				return new vortex_affector(parent, node);
			} else if(ntype == "gravity") {
				return new gravity_affector(parent, node);
			} else if(ntype == "scale") {
				return new scale_affector(parent, node);
			} else if(ntype == "particle_follower") {
				return new particle_follower_affector(parent, node);
			} else if(ntype == "align") {
				return new align_affector(parent, node);
			} else if(ntype == "randomiser" || ntype == "randomizer") {
				return new randomiser_affector(parent, node);
			} else if(ntype == "sine_force") {
				return new sine_force_affector(parent, node);
			} else {
				ASSERT_LOG(false, "FATAL: PSYSTEM2: Unrecognised affector type: " << ntype);
			}
			return NULL;
		}

		time_color_affector::time_color_affector(particle_system_container* parent, const variant& node)
			: affector(parent, node), operation_(time_color_affector::COLOR_OP_SET)
		{
			if(node.has_key("colour_operation")) {
				const std::string& op = node["colour_operation"].as_string();
				if(op == "multiply") {
					operation_ = COLOR_OP_MULTIPLY;
				} else if(op == "set") {
					operation_ = COLOR_OP_SET;
				} else {
					ASSERT_LOG(false, "FATAL: PSYSTEM2: unrecognised time_color affector operation: " << op);
				}
			}
			ASSERT_LOG(node.has_key("time_colour") || node.has_key("time_color"), "FATAL: PSYSTEM2: Must be a 'time_colour' attribute");
			const variant& tc_node = node.has_key("time_colour") ? node["time_colour"] : node["time_color"];
			if(tc_node.is_map()) {
				float t = tc_node["time"].as_decimal().as_float();
				glm::vec4 result;
				if(tc_node.has_key("color")) {
					ASSERT_LOG(tc_node["color"].is_list() && tc_node["color"].num_elements() == 4, "Expected vec4 variant but found " << tc_node["color"].write_json());
					result.r = tc_node["color"][0].as_decimal().as_float();
					result.g = tc_node["color"][1].as_decimal().as_float();
					result.b = tc_node["color"][2].as_decimal().as_float();
					result.a = tc_node["color"][3].as_decimal().as_float();
				} else if(tc_node.has_key("colour")) {
					ASSERT_LOG(tc_node["colour"].is_list() && tc_node["colour"].num_elements() == 4, "Expected vec4 variant but found " << tc_node["colour"].write_json());
					result.r = tc_node["colour"][0].as_decimal().as_float();
					result.g = tc_node["colour"][1].as_decimal().as_float();
					result.b = tc_node["colour"][2].as_decimal().as_float();
					result.a = tc_node["colour"][3].as_decimal().as_float();
				} else {
					ASSERT_LOG(false, "FATAL: PSYSTEM2, time_colour nodes must have a 'color' or 'colour' attribute");
				}
				tc_data_.push_back(std::make_pair(t, result));
			} else if(tc_node.is_list()) {
				for(size_t n = 0; n != tc_node.num_elements(); ++n) {
					float t = tc_node[n]["time"].as_decimal().as_float();
					glm::vec4 result;
					if(tc_node[n].has_key("color")) {
						ASSERT_LOG(tc_node[n]["color"].is_list() && tc_node[n]["color"].num_elements() == 4, "Expected vec4 variant but found " << tc_node[n]["color"].write_json());
						result.r = tc_node[n]["color"][0].as_decimal().as_float();
						result.g = tc_node[n]["color"][1].as_decimal().as_float();
						result.b = tc_node[n]["color"][2].as_decimal().as_float();
						result.a = tc_node[n]["color"][3].as_decimal().as_float();
					} else if(tc_node[n].has_key("colour")) {
						ASSERT_LOG(tc_node[n]["colour"].is_list() && tc_node[n]["colour"].num_elements() == 4, "Expected vec4 variant but found " << tc_node[n]["colour"].write_json());
						result.r = tc_node[n]["colour"][0].as_decimal().as_float();
						result.g = tc_node[n]["colour"][1].as_decimal().as_float();
						result.b = tc_node[n]["colour"][2].as_decimal().as_float();
						result.a = tc_node[n]["colour"][3].as_decimal().as_float();
					} else {
						ASSERT_LOG(false, "FATAL: PSYSTEM2, time_colour nodes must have a 'color' or 'colour' attribute");
					}
					tc_data_.push_back(std::make_pair(t, result));
				}
				std::sort(tc_data_.begin(), tc_data_.end(), [](const tc_pair& lhs, const tc_pair& rhs){
					return lhs.first < rhs.first;
				});
			}
		}

		void time_color_affector::internal_apply(particle& p, float t)
		{
			glm::vec4 c;
			float ttl_percentage = 1.0f - p.current.time_to_live / p.initial.time_to_live;
			auto it1 = find_nearest_color(ttl_percentage);
			auto it2 = it1 + 1;
			if(it2 != tc_data_.end()) {
				c = it1->second + ((it2->second - it1->second) * ((ttl_percentage - it1->first)/(it2->first - it1->first)));
			} else {
				c = it1->second;
			}
			if(operation_ == COLOR_OP_SET) {
				p.current.color = color_vector(color_vector::value_type(c.r*255.0f), 
					color_vector::value_type(c.g*255.0f), 
					color_vector::value_type(c.b*255.0f), 
					color_vector::value_type(c.a*255.0f));
			} else {
				p.current.color = color_vector(color_vector::value_type(c.r*p.initial.color.r), 
					color_vector::value_type(c.g*p.initial.color.g), 
					color_vector::value_type(c.b*p.initial.color.b), 
					color_vector::value_type(c.a*p.initial.color.a));
			}
		}

		// Find nearest iterator to the time fraction "dt"
		std::vector<time_color_affector::tc_pair>::iterator time_color_affector::find_nearest_color(float dt)
		{
			auto it = tc_data_.begin();
			for(; it != tc_data_.end(); ++it) {
				if(dt < it->first) {
					if(it == tc_data_.begin()) {
						return it;
					} else {
						return --it;
					}
				} 
			}
			return --it;
		}

		jet_affector::jet_affector(particle_system_container* parent, const variant& node)
			: affector(parent, node)
		{
			if(node.has_key("acceleration")) {
				acceleration_ = parameter::factory(node["acceleration"]);
			} else {
				acceleration_.reset(new fixed_parameter(1.0f));
			}
		}

		void jet_affector::internal_apply(particle& p, float t)
		{
			float scale = t * acceleration_->get_value(1.0f - p.current.time_to_live/p.initial.time_to_live);
			if(p.current.direction.x == 0 && p.current.direction.y == 0 && p.current.direction.z == 0) {
				p.current.direction += p.initial.direction * scale;
			} else {
				p.current.direction += p.initial.direction * scale;
			}
		}

		vortex_affector::vortex_affector(particle_system_container* parent, const variant& node)
			: affector(parent, node), rotation_axis_(1.0f, 0.0f, 0.0f, 0.0f)
		{
			if(node.has_key("rotation_speed")) {
				rotation_speed_ = parameter::factory(node["rotation_speed"]);
			} else {
				rotation_speed_.reset(new fixed_parameter(1.0f));
			}
			if(node.has_key("rotation_axis")) {
				rotation_axis_ = variant_to_quat(node["rotation_axis"]);
			}
		}

		void vortex_affector::internal_apply(particle& p, float t)
		{
			glm::vec3 local = p.current.position - position();
			//p.current.position = position() + glm::rotate(rotation_axis_, local);
			p.current.position = position() + rotation_axis_ * local;
			//p.current.direction = glm::rotate(rotation_axis_, p.current.direction);
			p.current.direction = rotation_axis_ * p.current.direction;
		}

		gravity_affector::gravity_affector(particle_system_container* parent, const variant& node)
			: affector(parent, node), gravity_(float(node["gravity"].as_decimal(decimal(1.0)).as_float()))
		{
		}

		void gravity_affector::internal_apply(particle& p, float t)
		{
			glm::vec3 d = position() - p.current.position;
			float len_sqr = d.x*d.x + d.y*d.y + d.z*d.z;
			if(len_sqr > 0) {
				float force = (gravity_ * p.current.mass * mass()) / len_sqr;
				p.current.direction += (force * t) * d;
			}
		}

		scale_affector::scale_affector(particle_system_container* parent, const variant& node)
			: affector(parent, node), since_system_start_(node["since_system_start"].as_bool(false))
		{
			if(node.has_key("scale_x")) {
				scale_x_ = parameter::factory(node["scale_x"]);
			}
			if(node.has_key("scale_y")) {
				scale_y_ = parameter::factory(node["scale_y"]);
			}
			if(node.has_key("scale_z")) {
				scale_z_ = parameter::factory(node["scale_z"]);
			}
			if(node.has_key("scale_xyz")) {
				scale_xyz_ = parameter::factory(node["scale_xyz"]);
			}
		}

		float scale_affector::calculate_scale(parameter_ptr s, const particle& p)
		{
			float scale;
			if(since_system_start_) {
				scale = s->get_value(get_technique()->get_particle_system()->elapsed_time());
			} else {
				scale = s->get_value(1.0f - p.current.time_to_live / p.initial.time_to_live);
			}
			return scale;
		}

		void scale_affector::internal_apply(particle& p, float t)
		{
			if(scale_xyz_) {
				float calc_scale = calculate_scale(scale_xyz_, p);
				float value = p.current.dimensions.x + calc_scale /** affector_scale.x*/;
				if(value > 0) {
					p.current.dimensions.x = value;
				}
				value = p.current.dimensions.y + calc_scale /** affector_scale.y*/;
				if(value > 0) {
					p.current.dimensions.y = value;
				}
				value = p.current.dimensions.z + calc_scale /** affector_scale.z*/;
				if(value > 0) {
					p.current.dimensions.z = value;
				}
			} else {
				if(scale_x_) {
					float calc_scale = calculate_scale(scale_x_, p);
					float value = p.current.dimensions.x + calc_scale /** affector_scale.x*/;
					if(value > 0) {
						p.current.dimensions.x = value;
					}
				}
				if(scale_y_) {
					float calc_scale = calculate_scale(scale_y_, p);
					float value = p.current.dimensions.x + calc_scale /** affector_scale.y*/;
					if(value > 0) {
						p.current.dimensions.y = value;
					}
				}
				if(scale_z_) {
					float calc_scale = calculate_scale(scale_z_, p);
					float value = p.current.dimensions.z + calc_scale /** affector_scale.z*/;
					if(value > 0) {
						p.current.dimensions.z = value;
					}
				}
			}
		}
	}
}
