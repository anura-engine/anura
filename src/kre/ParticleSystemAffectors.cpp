/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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
#include "ParticleSystem.hpp"
#include "ParticleSystemAffectors.hpp"
#include "ParticleSystemEmitters.hpp"
#include "ParticleSystemParameters.hpp"
#include "variant_utils.hpp"
#include "spline3d.hpp"

namespace KRE
{
	namespace Particles
	{
		class TimeColorAffector : public Affector
		{
		public:
			explicit TimeColorAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);

			void init(const variant& node) override;
		protected:
			virtual void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<TimeColorAffector>(*this);
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

			TimeColorAffector();
		};

		class JetAffector : public Affector
		{
		public:
			explicit JetAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;
		protected:
			virtual void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<JetAffector>(*this);
			}
		private:
			ParameterPtr acceleration_;
			JetAffector();
		};
		// affectors to add: box_collider (width,height,depth, inner or outer collide, friction)
		// forcefield (delta, force, octaves, frequency, amplitude, persistence, size, worldsize(w,h,d), movement(x,y,z),movement_frequency)
		// geometry_rotator (use own rotation, speed(parameter), axis(x,y,z))
		// inter_particle_collider (sounds like a lot of calculations)
		// line
		// linear_force
		// path_follower
		// plane_collider
		// scale_velocity (parameter_ptr scale; bool since_system_start, bool stop_at_flip)
		// sphere_collider
		// texture_animator
		// texture_rotator
		// velocity matching

		class LinearForceAffector : public Affector
		{
		public:
			explicit LinearForceAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
				: Affector(parent, node)
			{
				init(node);
			}

			void init(const variant& node) override
			{
				if(node.has_key("force")) {
					force_ = Parameter::factory(node["force"]);
				} else {
					force_.reset(new FixedParameter(1.0f));
				}

				direction_ = variant_to_vec3(node["direction"]);
			}

		protected:
			virtual void internalApply(Particle& p, float t) override {
				float scale = t * force_->getValue(1.0f - p.current.time_to_live/p.initial.time_to_live);
				p.current.position += direction_*scale;
			}

			AffectorPtr clone() const override {
				return std::make_shared<LinearForceAffector>(*this);
			}

		private:
			ParameterPtr force_;
			glm::vec3 direction_;
			LinearForceAffector();
		};

		class ScaleAffector : public Affector
		{
		public:
			explicit ScaleAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;
		protected:
			virtual void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<ScaleAffector>(*this);
			}
		private:
			ParameterPtr scale_x_;
			ParameterPtr scale_y_;
			ParameterPtr scale_z_;
			ParameterPtr scale_xyz_;
			bool since_system_start_;
			float calculateScale(ParameterPtr s, const Particle& p);
			ScaleAffector();
		};

		class VortexAffector : public Affector
		{
		public:
			explicit VortexAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;
		protected:
			virtual void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<VortexAffector>(*this);
			}
		private:
			glm::vec3 rotation_axis_;
			ParameterPtr rotation_speed_;
			VortexAffector();
		};

		class GravityAffector : public Affector
		{
		public:
			explicit GravityAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;
		protected:
			virtual void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<GravityAffector>(*this);
			}
		private:
			ParameterPtr gravity_;
			GravityAffector();
		};

		class ParticleFollowerAffector : public Affector
		{
		public:
			explicit ParticleFollowerAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
				: Affector(parent, node),
				  min_distance_(node["min_distance"].as_float(1.0f)),
				  max_distance_(node["max_distance"].as_float(std::numeric_limits<float>::max())) {
				init(node);
			}
			void init(const variant& node) override {
			}
		protected:
			virtual void handleEmitProcess(float t) override {
				std::vector<Particle>& particles = getTechnique()->getActiveParticles();
				// keeps particles following wihin [min_distance, max_distance]
				if(particles.size() < 1) {
					return;
				}
				prev_particle_ = particles.begin();
				for(auto p = particles.begin(); p != particles.end(); ++p) {
					internalApply(*p, t);
					prev_particle_ = p;
				}
			}
			virtual void internalApply(Particle& p, float t) override {
				auto distance = glm::length(p.current.position - prev_particle_->current.position);
				if(distance > min_distance_ && distance < max_distance_) {
					p.current.position = prev_particle_->current.position + (min_distance_/distance)*(p.current.position-prev_particle_->current.position);
				}
			}
			AffectorPtr clone() const override {
				return std::make_shared<ParticleFollowerAffector>(*this);
			}
		private:
			float min_distance_;
			float max_distance_;
			std::vector<Particle>::iterator prev_particle_;
			ParticleFollowerAffector();
		};

		class AlignAffector : public Affector
		{
		public:
			explicit AlignAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
				: Affector(parent, node), 
				  resize_(false) 
			{
				init(node);
			}
			void init(const variant& node) override {
				resize_ = (node["resize"].as_bool(false));
			}
		protected:
			virtual void internalApply(Particle& p, float t) override {
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
			virtual void handleEmitProcess(float t) override {
				std::vector<Particle>& particles = getTechnique()->getActiveParticles();
				if(particles.size() < 1) {
					return;
				}
				prev_particle_ = particles.begin();				
				for(auto p = particles.begin(); p != particles.end(); ++p) {
					internalApply(*p, t);
					prev_particle_ = p;
				}
			}
			virtual AffectorPtr clone() const override {
				return std::make_shared<AlignAffector>(*this);
			}
		private:
			bool resize_;			
			std::vector<Particle>::iterator prev_particle_;
			AlignAffector();
		};

		class FlockCenteringAffector : public Affector
		{
		public:
			explicit FlockCenteringAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
				: Affector(parent, node), 
                  average_(0.0f)
			{
				init(node);
			}
			void init(const variant& node) override {
			}
		protected:
			virtual void internalApply(Particle& p, float t) override {
				p.current.direction = (average_ - p.current.position) * t;
			}
			virtual void handleEmitProcess(float t) override {
				std::vector<Particle>& particles = getTechnique()->getActiveParticles();
				if(particles.size() < 1) {
					return;
				}
				auto count = particles.size();
				glm::vec3 sum(0.0f);
				for(const auto& p : particles) {
					sum += p.current.position;
				}
				average_ /= static_cast<float>(count);

				prev_particle_ = particles.begin();				
				for(auto p = particles.begin(); p != particles.end(); ++p) {
					internalApply(*p, t);
					prev_particle_ = p;
				}
			}
			AffectorPtr clone() const override {
				return std::make_shared<FlockCenteringAffector>(*this);
			}
		private:
			glm::vec3 average_;
			std::vector<Particle>::iterator prev_particle_;
			FlockCenteringAffector();
		};

		class BlackHoleAffector : public Affector
		{
		public:
			explicit BlackHoleAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
				: Affector(parent, node), 
				  velocity_(0.0), 
				  acceleration_(0.0)
			{
				init(node);
			}
			void init(const variant& node) override {
				velocity_ = (node["velocity"].as_float());
				acceleration_ = (node["acceleration"].as_float());
				
			}
		private:
			virtual void handleEmitProcess(float t) override {
				velocity_ += acceleration_;
				Affector::handleEmitProcess(t);
			}

			virtual void internalApply(Particle& p, float t) override {
				glm::vec3 diff = getPosition() - p.current.position;
				float len = glm::length(diff);
				if(len > velocity_) {
					diff *= velocity_/len;
				} else {
					p.current.time_to_live = 0;
				}

				p.current.position += diff;
			}

			AffectorPtr clone() const override {
				return std::make_shared<BlackHoleAffector>(*this);
			}

			float velocity_, acceleration_;
		};

		class PathFollowerAffector : public Affector
		{
		public:
			explicit PathFollowerAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
				: Affector(parent, node)
			{
				init(node);
			}

			void init(const variant& node) override
			{
				ASSERT_LOG(node.has_key("path") && node["path"].is_list(),
					"path_follower must have a 'path' attribute.");
				for(unsigned n = 0; n != node["path"].num_elements(); ++n) {
					const auto& pt = node["path"][n];
					ASSERT_LOG(pt.is_list() && pt.num_elements() > 0, "points in path must be lists of more than one element.");
					const double x = pt[0].as_float();
					const double y = pt.num_elements() > 1 ? pt[1].as_float() : 0.0;
					const double z = pt.num_elements() > 2 ? pt[2].as_float() : 0.0;
					points_.emplace_back(x,y,z);
				}
				spl_ = std::make_shared<geometry::spline3d<float>>(points_);
			}
		protected:
			virtual void internalApply(Particle& p, float t) override {
				const float time_fraction = p.current.time_to_live / p.initial.time_to_live;
				const float time_fraction_next = (p.current.time_to_live + t) > p.initial.time_to_live 
					? 1.0f 
					: (p.current.time_to_live + t) / p.initial.time_to_live;
				p.current.position += spl_->interpolate(time_fraction_next) - spl_->interpolate(time_fraction);
			}
			virtual void handleEmitProcess(float t) override {
				std::vector<Particle>& particles = getTechnique()->getActiveParticles();
				if(particles.size() < 1) {
					return;
				}

				prev_particle_ = particles.begin();				
				for(auto p = particles.begin(); p != particles.end(); ++p) {
					internalApply(*p, t);
					prev_particle_ = p;
				}
			}
			AffectorPtr clone() const override {
				return std::make_shared<PathFollowerAffector>(*this);
			}
		private:
			std::shared_ptr<geometry::spline3d<float>> spl_;
			std::vector<glm::vec3> points_;
			std::vector<Particle>::iterator prev_particle_;
			PathFollowerAffector();
		};

		class RandomiserAffector : public Affector
		{
		public:
			explicit RandomiserAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
				: Affector(parent, node), 
				  max_deviation_(0.0f), 
				  time_step_(0),
				  random_direction_(true)
			{
				init(node);
			}

			void init(const variant& node) override
			{
				time_step_ = (float(node["time_step"].as_float(0)));
				random_direction_ = (node["use_direction"].as_bool(true));

				if(node.has_key("max_deviation_x")) {
					max_deviation_.x = float(node["max_deviation_x"].as_float());
				}
				if(node.has_key("max_deviation_y")) {
					max_deviation_.y = float(node["max_deviation_y"].as_float());
				}
				if(node.has_key("max_deviation_z")) {
					max_deviation_.z = float(node["max_deviation_z"].as_float());
				}
				last_update_time_[0] = last_update_time_[1] = 0.0f;
			}
		protected:
			virtual void internalApply(Particle& p, float t) override {
				if(random_direction_) {
					// change direction per update
					p.current.direction += glm::vec3(get_random_float(-max_deviation_.x, max_deviation_.x),
						get_random_float(-max_deviation_.y, max_deviation_.y),
						get_random_float(-max_deviation_.z, max_deviation_.z));
				} else {
					// change position per update.
					p.current.position += getScale() * glm::vec3(get_random_float(-max_deviation_.x, max_deviation_.x),
						get_random_float(-max_deviation_.y, max_deviation_.y),
						get_random_float(-max_deviation_.z, max_deviation_.z));
				}
			}
			void handle_apply(std::vector<Particle>& particles, float t) {
				last_update_time_[0] += t;
				if(last_update_time_[0] > time_step_) {
					last_update_time_[0] -= time_step_;
					for(auto& p : particles) {
						internalApply(p, t);
					}
				}
			}
			void handle_apply(std::vector<EmitterPtr>& objs, float t) {
				last_update_time_[1] += t;
				if(last_update_time_[1] > time_step_) {
					last_update_time_[1] -= time_step_;
					for(auto e : objs) {
						internalApply(*e, t);
					}
				}
			}
			virtual void handleProcess(float t) {
				handle_apply(getTechnique()->getActiveParticles(), t);
				handle_apply(getTechnique()->getActiveEmitters(), t);
			}
			AffectorPtr clone() const override {
				return std::make_shared<RandomiserAffector>(*this);
			}
		private:
			// randomiser (bool random_direction_, float time_step_ glm::vec3 max_deviation_)
			bool random_direction_;
			float time_step_;
			glm::vec3 max_deviation_;
			float last_update_time_[2];
			RandomiserAffector();
		};

		class SineForceAffector : public Affector
		{
		public:
			explicit SineForceAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
				: Affector(parent, node),
				  min_frequency_(1.0f),
				  max_frequency_(1.0f),
				  angle_(0.0f),
				  frequency_(1.0f),
				  force_vector_(0.0f),
				  scale_vector_(0.0f),
				  fa_(FA_ADD)
			{
				init(node);
			}

			void init(const variant& node) override
			{
				if(node.has_key("max_frequency")) {
					max_frequency_ = float(node["max_frequency"].as_float());
					frequency_ = max_frequency_;
				}
				if(node.has_key("min_frequency")) {
					min_frequency_ = float(node["min_frequency"].as_float());					
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
						ASSERT_LOG(false, "'force_application' attribute should have value average or add");
					}
				}
			}
		protected:
			virtual void handleEmitProcess(float t) override {
				angle_ += /*2.0f * M_PI **/ frequency_ * t;
				float sine_value = sin(angle_);
				scale_vector_ = force_vector_ * t * sine_value;
				//std::cerr << "XXX: angle: " << angle_ << " scale_vec: " << scale_vector_ << std::endl;
				if(angle_ > float(M_PI*2.0f)) {
					angle_ -= float(M_PI*2.0f);
					if(min_frequency_ != max_frequency_) {
						frequency_ = get_random_float(min_frequency_, max_frequency_);
					}
				}
				Affector::handleEmitProcess(t);
			}
			virtual void internalApply(Particle& p, float t) override {
				if(fa_ == FA_ADD) {
					p.current.direction += scale_vector_;
				} else {
					p.current.direction = (p.current.direction + force_vector_)/2.0f;
				}
			}
			AffectorPtr clone() const override {
				return std::make_shared<SineForceAffector>(*this);
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
			SineForceAffector();
		};

		Affector::Affector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: EmitObject(parent, node), 
			  mass_(float(node["mass_affector"].as_float(1.0f))),
			  position_(0.0f), 
			  scale_(1.0f),
			  node_(node)
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
		
		Affector::~Affector()
		{
		}

		TechniquePtr Affector::getTechnique() const
		{
			auto tq = technique_.lock();
			ASSERT_LOG(tq != nullptr, "No parent technique found.");
			return tq;
		}

		void Affector::handleEmitProcess(float t) 
		{
			auto tq = getTechnique();
			for(auto& e : tq->getActiveEmitters()) {
				if(e->emitted_by != nullptr) {
					if(!isEmitterExcluded(e->emitted_by->name())) {
						internalApply(*e,t);
					}
				}
			}
			for(auto& p : tq->getActiveParticles()) {
				ASSERT_LOG(p.emitted_by != nullptr, "p.emitted_by is null");
				if(!isEmitterExcluded(p.emitted_by->name())) {
					internalApply(p,t);
				}
			}
		}

		bool Affector::isEmitterExcluded(const std::string& name) const
		{
			return std::find(excluded_emitters_.begin(), excluded_emitters_.end(), name) != excluded_emitters_.end();
		}

		AffectorPtr Affector::factory(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
		{
			ASSERT_LOG(node.has_key("type"), "affector must have 'type' attribute");
			const std::string& ntype = node["type"].as_string();
			if(ntype == "color" || ntype == "colour") {
				return std::make_shared<TimeColorAffector>(parent, node);
			} else if(ntype == "jet") {
				return std::make_shared<JetAffector>(parent, node);
			} else if(ntype == "vortex") {
				return std::make_shared<VortexAffector>(parent, node);
			} else if(ntype == "gravity") {
				return std::make_shared<GravityAffector>(parent, node);
			} else if(ntype == "linear_force") {
				return std::make_shared<LinearForceAffector>(parent, node);
			} else if(ntype == "scale") {
				return std::make_shared<ScaleAffector>(parent, node);
			} else if(ntype == "particle_follower") {
				return std::make_shared<ParticleFollowerAffector>(parent, node);
			} else if(ntype == "align") {
				return std::make_shared<AlignAffector>(parent, node);
			} else if(ntype == "randomiser" || ntype == "randomizer") {
				return std::make_shared<RandomiserAffector>(parent, node);
			} else if(ntype == "sine_force" || ntype == "sin_force") {
				return std::make_shared<SineForceAffector>(parent, node);
			} else if(ntype == "path_follower") {
				return std::make_shared<PathFollowerAffector>(parent, node);
			} else if(ntype == "black_hole") {
				return std::make_shared<BlackHoleAffector>(parent, node);
			} else if(ntype == "flock_centering") {
				return std::make_shared<FlockCenteringAffector>(parent, node);
			} else {
				ASSERT_LOG(false, "Unrecognised affector type: " << ntype);
			}
			return nullptr;
		}

		TimeColorAffector::TimeColorAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: Affector(parent, node), 
			  operation_(TimeColorAffector::COLOR_OP_SET)
		{
			init(node);
		}

		void TimeColorAffector::init(const variant& node)
		{
			std::string op;
			if(node.has_key("color_operation")) {
				op = node["color_operation"].as_string();
			} else if(node.has_key("colour_operation")) {
				op = node["colour_operation"].as_string();
			}
			if(!op.empty()) {
				if(op == "multiply") {
					operation_ = COLOR_OP_MULTIPLY;
				} else if(op == "set") {
					operation_ = COLOR_OP_SET;
				} else {
					ASSERT_LOG(false, "unrecognised time_color affector operation: " << op);
				}
			}
			ASSERT_LOG(node.has_key("time_colour") || node.has_key("time_color"), "Must be a 'time_colour' attribute");
			const variant& tc_node = node.has_key("time_colour") ? node["time_colour"] : node["time_color"];
			if(tc_node.is_map()) {
				float t = tc_node["time"].as_float();
				glm::vec4 result;
				if(tc_node.has_key("color")) {
					ASSERT_LOG(tc_node["color"].is_list() && tc_node["color"].num_elements() == 4, "Expected vec4 variant but found " << tc_node["color"].write_json());
					result.r = tc_node["color"][0].as_float();
					result.g = tc_node["color"][1].as_float();
					result.b = tc_node["color"][2].as_float();
					result.a = tc_node["color"][3].as_float();
				} else if(tc_node.has_key("colour")) {
					ASSERT_LOG(tc_node["colour"].is_list() && tc_node["colour"].num_elements() == 4, "Expected vec4 variant but found " << tc_node["colour"].write_json());
					result.r = tc_node["colour"][0].as_float();
					result.g = tc_node["colour"][1].as_float();
					result.b = tc_node["colour"][2].as_float();
					result.a = tc_node["colour"][3].as_float();
				} else {
					ASSERT_LOG(false, "PSYSTEM2, time_colour nodes must have a 'color' or 'colour' attribute");
				}
				tc_data_.push_back(std::make_pair(t, result));
			} else if(tc_node.is_list()) {
				for(int n = 0; n != tc_node.num_elements(); ++n) {
					float t = tc_node[n]["time"].as_float();
					glm::vec4 result;
					if(tc_node[n].has_key("color")) {
						ASSERT_LOG(tc_node[n]["color"].is_list() && tc_node[n]["color"].num_elements() == 4, "Expected vec4 variant but found " << tc_node[n]["color"].write_json());
						result.r = tc_node[n]["color"][0].as_float();
						result.g = tc_node[n]["color"][1].as_float();
						result.b = tc_node[n]["color"][2].as_float();
						result.a = tc_node[n]["color"][3].as_float();
					} else if(tc_node[n].has_key("colour")) {
						ASSERT_LOG(tc_node[n]["colour"].is_list() && tc_node[n]["colour"].num_elements() == 4, "Expected vec4 variant but found " << tc_node[n]["colour"].write_json());
						result.r = tc_node[n]["colour"][0].as_float();
						result.g = tc_node[n]["colour"][1].as_float();
						result.b = tc_node[n]["colour"][2].as_float();
						result.a = tc_node[n]["colour"][3].as_float();
					} else {
						ASSERT_LOG(false, "PSYSTEM2, time_colour nodes must have a 'color' or 'colour' attribute");
					}
					tc_data_.push_back(std::make_pair(t, result));
				}
				std::sort(tc_data_.begin(), tc_data_.end(), [](const tc_pair& lhs, const tc_pair& rhs){
					return lhs.first < rhs.first;
				});
			}
		}

		void TimeColorAffector::internalApply(Particle& p, float t)
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
		std::vector<TimeColorAffector::tc_pair>::iterator TimeColorAffector::find_nearest_color(float dt)
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

		JetAffector::JetAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: Affector(parent, node)
		{
			init(node);
		}

		void JetAffector::init(const variant& node)
		{
			if(node.has_key("acceleration")) {
				acceleration_ = Parameter::factory(node["acceleration"]);
			} else {
				acceleration_.reset(new FixedParameter(1.0f));
			}
		}

		void JetAffector::internalApply(Particle& p, float t)
		{
			float scale = t * acceleration_->getValue(1.0f - p.current.time_to_live/p.initial.time_to_live);
			if(p.current.direction.x == 0 && p.current.direction.y == 0 && p.current.direction.z == 0) {
				p.current.direction += p.initial.direction * scale;
			} else {
				p.current.direction += p.initial.direction * scale;
			}
		}

		VortexAffector::VortexAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: Affector(parent, node), 
			  rotation_axis_(0.0f, 1.0f, 0.0f)
		{
			init(node);
		}

		void VortexAffector::init(const variant& node)
		{
			if(node.has_key("rotation_speed")) {
				rotation_speed_ = Parameter::factory(node["rotation_speed"]);
			} else {
				rotation_speed_.reset(new FixedParameter(1.0f));
			}
			if(node.has_key("rotation_axis")) {
				rotation_axis_ = variant_to_vec3(node["rotation_axis"]);
			}
		}

		void VortexAffector::internalApply(Particle& p, float t)
		{
			glm::vec3 local = p.current.position - getPosition();
			float spd = rotation_speed_->getValue(getTechnique()->getParticleSystem()->getElapsedTime());
			glm::quat rotation = glm::angleAxis(glm::radians(spd), rotation_axis_);
			p.current.position = getPosition() + rotation * local;
			p.current.direction = rotation * p.current.direction;
		}

		GravityAffector::GravityAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: Affector(parent, node), 
			  gravity_()
		{
			init(node);
		}

		void GravityAffector::init(const variant& node)
		{
			if(node.has_key("gravity")) {
				gravity_ = Parameter::factory(node["gravity"]);
			} else {
				gravity_ .reset(new FixedParameter(1.0f));
			}
		}

		void GravityAffector::internalApply(Particle& p, float t)
		{
			glm::vec3 d = getPosition() - p.current.position;
			float len_sqr = sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
			if(len_sqr > 0) {
				float force = (gravity_->getValue(t) * p.current.mass * mass()) / len_sqr;
				p.current.direction += (force * t) * d;
			}
		}

		ScaleAffector::ScaleAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: Affector(parent, node), 
			  since_system_start_(false)
		{
			init(node);
		}

		void ScaleAffector::init(const variant& node)
		{
			since_system_start_ = (node["since_system_start"].as_bool(false));
			if(node.has_key("scale_x")) {
				scale_x_ = Parameter::factory(node["scale_x"]);
			}
			if(node.has_key("scale_y")) {
				scale_y_ = Parameter::factory(node["scale_y"]);
			}
			if(node.has_key("scale_z")) {
				scale_z_ = Parameter::factory(node["scale_z"]);
			}
			if(node.has_key("scale_xyz")) {
				scale_xyz_ = Parameter::factory(node["scale_xyz"]);
			}
		}

		float ScaleAffector::calculateScale(ParameterPtr s, const Particle& p)
		{
			float scale;
			if(since_system_start_) {
				scale = s->getValue(getTechnique()->getParticleSystem()->getElapsedTime());
			} else {
				scale = s->getValue(1.0f - p.current.time_to_live / p.initial.time_to_live);
			}
			return scale;
		}

		void ScaleAffector::internalApply(Particle& p, float t)
		{
			if(scale_xyz_) {
				float calc_scale = calculateScale(scale_xyz_, p);
				float value = p.initial.dimensions.x * calc_scale * getScale().x;
				if(value > 0) {
					p.current.dimensions.x = value;
				}
				value = p.initial.dimensions.y * calc_scale * getScale().y;
				if(value > 0) {
					p.current.dimensions.y = value;
				}
				value = p.initial.dimensions.z * calc_scale * getScale().z;
				if(value > 0) {
					p.current.dimensions.z = value;
				}
			} else {
				if(scale_x_) {
					float calc_scale = calculateScale(scale_x_, p);
					float value = p.initial.dimensions.x * calc_scale * getScale().x;
					if(value > 0) {
						p.current.dimensions.x = value;
					}
				}
				if(scale_y_) {
					float calc_scale = calculateScale(scale_y_, p);
					float value = p.initial.dimensions.x * calc_scale * getScale().y;
					if(value > 0) {
						p.current.dimensions.y = value;
					}
				}
				if(scale_z_) {
					float calc_scale = calculateScale(scale_z_, p);
					float value = p.initial.dimensions.z * calc_scale * getScale().z;
					if(value > 0) {
						p.current.dimensions.z = value;
					}
				}
			}
		}
	}
}
