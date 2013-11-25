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
			explicit time_color_affector(const variant& node, technique* tech);
			virtual ~time_color_affector();
		protected:
			virtual void handle_apply(std::vector<particle>& particles, float t);
			virtual void handle_apply(std::vector<emitter_ptr>& emitters, float t) {
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
			time_color_affector(const time_color_affector&);
		};

		class jet_affector : public affector
		{
		public:
			explicit jet_affector(const variant& node, technique* tech);
			virtual ~jet_affector();
		protected:
			virtual void handle_apply(std::vector<particle>& particles, float t);
			virtual void handle_apply(std::vector<emitter_ptr>& emitters, float t) {
			}
		private:
			parameter_ptr acceleration_;
			jet_affector();
			jet_affector(const jet_affector&);
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
		// randomiser (bool random_direction_, float time_step_ glm::vec3 max_deviation_)
		// scale_velocity (parameter_ptr scale; bool since_system_start, bool stop_at_flip)
		// sine_force
		// sphere_collider
		// texture_animator
		// texture_rotator
		// velocity matching

		class scale_affector : public affector
		{
		public:
			explicit scale_affector(const variant& node, technique* tech);
			virtual ~scale_affector();
		protected:
			virtual void handle_apply(std::vector<particle>& particles, float t);
			virtual void handle_apply(std::vector<emitter_ptr>& emitters, float t) {
			}
		private:
			parameter_ptr scale_x_;
			parameter_ptr scale_y_;
			parameter_ptr scale_z_;
			parameter_ptr scale_xyz_;
			bool since_system_start_;
			float calculate_scale(parameter_ptr s, const particle& p);
			scale_affector();
			scale_affector(const scale_affector&);
		};

		class vortex_affector : public affector
		{
		public:
			explicit vortex_affector(const variant& node, technique* tech);
			virtual ~vortex_affector();
		protected:
			virtual void handle_apply(std::vector<particle>& particles, float t);
			virtual void handle_apply(std::vector<emitter_ptr>& emitters, float t) {
			}
		private:
			glm::quat rotation_axis_;
			parameter_ptr rotation_speed_;
			vortex_affector();
			vortex_affector(const vortex_affector&);
		};

		class gravity_affector : public affector
		{
		public:
			explicit gravity_affector(const variant& node, technique* tech);
			virtual ~gravity_affector();
		protected:
			virtual void handle_apply(std::vector<particle>& particles, float t);
			virtual void handle_apply(std::vector<emitter_ptr>& emitters, float t) {
			}
		private:
			float gravity_;
			gravity_affector();
			gravity_affector(const gravity_affector&);
		};

		class particle_follower_affector : public affector
		{
		public:
			explicit particle_follower_affector(const variant& node, technique* tech);
			virtual ~particle_follower_affector();
		protected:
			virtual void handle_apply(std::vector<particle>& particles, float t);
			virtual void handle_apply(std::vector<emitter_ptr>& emitters, float t) {
			}
		private:
			float min_distance_;
			float max_distance_;
			particle_follower_affector();
			particle_follower_affector(const particle_follower_affector&);
		};

		class align_affector : public affector
		{
		public:
			explicit align_affector(const variant& node, technique* tech) 
				: affector(node, tech), resize_(node["resize"].as_bool(false)) {
			}
			virtual ~align_affector() {
			}
		protected:
			virtual void handle_apply(std::vector<particle>& particles, float t) {
				auto prev_p = particles.begin();
				for(auto p = particles.begin()+1; p != particles.end(); ++p) {
					glm::vec3 distance = prev_p->current.position - p->current.position;
					if(resize_) {
						p->current.dimensions.y = glm::length(distance);
					}
					distance = glm::normalize(distance);
					p->current.orientation.x = distance.x;
					p->current.orientation.y = distance.y;
					p->current.orientation.z = distance.z;
					prev_p = p;
				}
			}
			virtual void handle_apply(std::vector<emitter_ptr>& emitters, float t) {
				// we don't apply this affector to emitters.
			}
		private:
			bool resize_;			
			align_affector();
			align_affector(const align_affector&);
		};


		affector::affector(const variant& node, technique* tech)
			: enabled_(node["enabled"].as_bool(true)), 
			mass_(float(node["mass_affector"].as_decimal(decimal(1.0)).as_float())),
			position_(0.0f), technique_(tech)
		{
			ASSERT_LOG(technique_ != NULL, "FATAL: PSYSTEM2: technique_ is null");
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

		void affector::apply(std::vector<particle>& particles, std::vector<emitter_ptr>& emitters, float t)
		{
			if(enabled_) {
				handle_apply(particles, t);
				handle_apply(emitters, t);
			}
		}

		bool affector::is_emitter_excluded(const std::string& name)
		{
			return std::find(excluded_emitters_.begin(), excluded_emitters_.end(), name) != excluded_emitters_.end();
		}

		affector_ptr affector::factory(const variant& node, technique* tech)
		{
			ASSERT_LOG(node.has_key("type"), "FATAL: PSYSTEM2: affector must have 'type' attribute");
			const std::string& ntype = node["type"].as_string();
			if(ntype == "color" || ntype == "colour") {
				return affector_ptr(new time_color_affector(node, tech));
			} else if(ntype == "jet") {
				return affector_ptr(new jet_affector(node, tech));
			} else if(ntype == "vortex") {
				return affector_ptr(new vortex_affector(node, tech));
			} else if(ntype == "gravity") {
				return affector_ptr(new gravity_affector(node, tech));
			} else if(ntype == "scale") {
				return affector_ptr(new scale_affector(node, tech));
			} else if(ntype == "particle_follower") {
				return affector_ptr(new particle_follower_affector(node, tech));
			} else if(ntype == "align") {
				return affector_ptr(new align_affector(node, tech));
			} else {
				ASSERT_LOG(false, "FATAL: PSYSTEM2: Unrecognised affector type: " << ntype);
			}
			return affector_ptr();
		}

		time_color_affector::time_color_affector(const variant& node, technique* tech)
			: affector(node, tech), operation_(time_color_affector::COLOR_OP_SET)
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

		time_color_affector::~time_color_affector()
		{
		}

		void time_color_affector::handle_apply(std::vector<particle>& particles, float t)
		{
			for(auto& p : particles) {
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

		jet_affector::jet_affector(const variant& node, technique* tech)
			: affector(node, tech)
		{
			if(node.has_key("acceleration")) {
				acceleration_ = parameter::factory(node["acceleration"]);
			} else {
				acceleration_.reset(new fixed_parameter(1.0f));
			}
		}

		jet_affector::~jet_affector()
		{
		}

		void jet_affector::handle_apply(std::vector<particle>& particles, float t)
		{
			for(auto& p : particles) {
				float scale = t * acceleration_->get_value(1.0f - p.current.time_to_live/p.initial.time_to_live);
				if(p.current.direction.x == 0 && p.current.direction.y == 0 && p.current.direction.z == 0) {
					p.current.direction += p.initial.direction * scale;
				} else {
					p.current.direction += p.initial.direction * scale;
				}
			}
		}

		vortex_affector::vortex_affector(const variant& node, technique* tech)
			: affector(node, tech), rotation_axis_(1.0f, 0.0f, 0.0f, 0.0f)
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

		vortex_affector::~vortex_affector()
		{
		}

		void vortex_affector::handle_apply(std::vector<particle>& particles, float t)
		{
			for(auto& p : particles) {
				glm::vec3 local = p.current.position - position();
				p.current.position = position() + glm::rotate(rotation_axis_, local);
				p.current.direction = glm::rotate(rotation_axis_, p.current.direction);
			}
		}

		gravity_affector::gravity_affector(const variant& node, technique* tech)
			: affector(node, tech), gravity_(float(node["gravity"].as_decimal(decimal(1.0)).as_float()))
		{
		}

		gravity_affector::~gravity_affector()
		{
		}

		void gravity_affector::handle_apply(std::vector<particle>& particles, float t)
		{
			for(auto& p : particles) {
				glm::vec3 d = position() - p.current.position;
				float len_sqr = d.x*d.x + d.y*d.y + d.z*d.z;
				if(len_sqr > 0) {
					float force = (gravity_ * p.current.mass * mass()) / len_sqr;
					p.current.direction += (force * t) * d;
				}
			}
		}

		scale_affector::scale_affector(const variant& node, technique* tech)
			: affector(node, tech), since_system_start_(node["since_system_start"].as_bool(false))
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

		scale_affector::~scale_affector()
		{
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

		void scale_affector::handle_apply(std::vector<particle>& particles, float t)
		{
			for(auto& p : particles) {
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

		particle_follower_affector::particle_follower_affector(const variant& node, technique* tech)
			: affector(node, tech),
			min_distance_(node["min_distance"].as_decimal(decimal(1.0)).as_float()),
			max_distance_(node["max_distance"].as_decimal(decimal(std::numeric_limits<float>::max())).as_float())
		{
		}

		particle_follower_affector::~particle_follower_affector()
		{
		}

		void particle_follower_affector::handle_apply(std::vector<particle>& particles, float t)
		{
			// keeps particles following wihin [min_distance, max_distance]
			if(particles.size() < 2) {
				return;
			}
			auto prev_p = particles.begin();
			for(auto p = particles.begin()+1; p != particles.end(); ++p) {
				auto distance = glm::length(p->current.position - prev_p->current.position);
				if(distance > min_distance_ && distance < max_distance_) {
					p->current.position = prev_p->current.position + (min_distance_/distance)*(p->current.position-prev_p->current.position);
				}
				prev_p = p;
			}
		}

	}
}
