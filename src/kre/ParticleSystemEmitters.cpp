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

namespace KRE
{
	namespace Particles
	{
		class CircleEmitter : public Emitter
		{
		public:
			CircleEmitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 			
				: Emitter(parent, node), 
				  circle_radius_(Parameter::factory(node["circle_radius"])),
				  circle_step_(node["circle_step"].as_float(0.1f)), 
				  circle_angle_(node["circle_angle"].as_float(0)), 
				  circle_random_(node["emit_random"].as_bool(true)) {
			}
		protected:
			void internalCreate(Particle& p, float t) {
				float angle = 0.0f;
				if(circle_random_) {
					angle = get_random_float(0.0f, float(2.0 * M_PI));
				} else {
					angle = t * circle_step_;
				}

				const float r = circle_radius_->getValue();
				p.initial.position.x += r * sin(angle + circle_angle_);
				p.initial.position.y += r * cos(angle + circle_angle_);
			}
			virtual EmitterPtr clone() {
				return std::make_shared<CircleEmitter>(*this);
			}
		private:
			ParameterPtr circle_radius_;
			float circle_step_;
			float circle_angle_;
			bool circle_random_;

			CircleEmitter();
		};

		class BoxEmitter : public Emitter
		{
		public:
			BoxEmitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
				: Emitter(parent, node), 
				  box_dimensions_(100.0f) {
				if(node.has_key("box_width")) {
					box_dimensions_.x = node["box_width"].as_float();
				}
				if(node.has_key("box_height")) {
					box_dimensions_.y = node["box_height"].as_float();
				}
				if(node.has_key("box_depth")) {
					box_dimensions_.z = node["box_depth"].as_float();
				}
			}
		protected:
			void internalCreate(Particle& p, float t) {
				p.initial.position.x += get_random_float(0.0f, box_dimensions_.x) - box_dimensions_.x/2;
				p.initial.position.y += get_random_float(0.0f, box_dimensions_.y) - box_dimensions_.y/2;
				p.initial.position.z += get_random_float(0.0f, box_dimensions_.z) - box_dimensions_.z/2;
			}
			virtual EmitterPtr clone() {
				return std::make_shared<BoxEmitter>(*this);
			}
		private:
			glm::vec3 box_dimensions_;
			BoxEmitter();
		};

		class LineEmitter : public Emitter
		{
		public:
			LineEmitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
				: Emitter(parent, node), 
				  line_end_(0.0f), 
				  line_deviation_(0.0f),
				  min_increment_(0.0f), 
				  max_increment_(0.0f) {
				if(node.has_key("max_deviation")) {
					line_deviation_ = node["max_deviation"].as_float();
				}
				if(node.has_key("min_increment")) {
					min_increment_ = node["min_increment"].as_float();
				}
				if(node.has_key("max_increment")) {
					max_increment_ = node["max_increment"].as_float();
				}
				// XXX line_end_ ?
			}
		protected:
			void internalCreate(Particle& p, float t) override {
				// XXX todo
			}
			virtual EmitterPtr clone() {
				return std::make_shared<LineEmitter>(*this);
			}
		private:
			glm::vec3 line_end_;
			float line_deviation_;
			float min_increment_;
			float max_increment_;

			LineEmitter();
		};

		class PointEmitter : public Emitter
		{
		public:
			PointEmitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
				: Emitter(parent, node) 
			{}
		protected:
			void internalCreate(Particle& p, float t) {
				// intentionally does nothing.
			}
			virtual EmitterPtr clone() {
				return std::make_shared<PointEmitter>(*this);
			}
		private:
			PointEmitter();
		};

		class SphereSurfaceEmitter : public Emitter
		{
		public:
			SphereSurfaceEmitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
				: Emitter(parent, node), 
				  radius_(node["radius"].as_float(1.0)) 
			{}
		protected:
			void internalCreate(Particle& p, float t) {
				float theta = get_random_float(0, 2.0f*static_cast<float>(M_PI));
				float phi = acos(get_random_float(-1.0f, 1.0f));
				p.initial.position.x += radius_ * sin(phi) * cos(theta);
				p.initial.position.y += radius_ * sin(phi) * sin(theta);
				p.initial.position.z += radius_ * cos(phi);
			}
			virtual EmitterPtr clone() {
				return std::make_shared<SphereSurfaceEmitter>(*this);
			}
		private:
			float radius_;
			SphereSurfaceEmitter();
		};


		Emitter::Emitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: EmitObject(parent, node), 
			  emission_fraction_(0.0f),
			  force_emission_(node["force_emission"].as_bool(false)),
			  force_emission_processed_(false), 
			  can_be_deleted_(false),
			  emits_type_(EmitsType::VISUAL),
			  color_(1.0f,1.0f,1.0f,1.0f),
              repeat_delay_remaining_(0)
		{
			init_physics_parameters(initial);
			init_physics_parameters(current);
			initial.time_to_live = current.time_to_live = 3;

			if(node.has_key("emission_rate")) {
				emission_rate_ = Parameter::factory(node["emission_rate"]);
			} else {
				emission_rate_.reset(new FixedParameter(10));
			}
			if(node.has_key("time_to_live")) {
				time_to_live_ = Parameter::factory(node["time_to_live"]);
			} else {
				time_to_live_.reset(new FixedParameter(10.0f));
			}
			if(node.has_key("velocity")) {
				velocity_ = Parameter::factory(node["velocity"]);
			} else {
				velocity_.reset(new FixedParameter(100.0f));
			}
			if(node.has_key("angle")) {
				angle_ = Parameter::factory(node["angle"]);
			} else {
				angle_.reset(new FixedParameter(20.0f));
			}
			if(node.has_key("mass")) {
				mass_ = Parameter::factory(node["mass"]);
			} else {
				mass_.reset(new FixedParameter(1.0f));
			}
			if(node.has_key("duration")) {
				duration_ = Parameter::factory(node["duration"]);
			} else {
				duration_.reset(new FixedParameter(0.0f));
			}
			if(node.has_key("repeat_delay")) {
				repeat_delay_ = Parameter::factory(node["repeat_delay"]);
			} else {
				repeat_delay_.reset(new FixedParameter(0.0f));
			}
			if(node.has_key("direction")) {
				initial.direction = current.direction = variant_to_vec3(node["direction"]);
			}
			if(node.has_key("position")) {
				initial.position = current.position = variant_to_vec3(node["position"]);
			}
			if(node.has_key("orientation")) {
				initial.orientation = current.orientation = variant_to_quat(node["orientation"]);
			}
			if(node.has_key("orientation_start") && node.has_key("orientation_end")) {
				orientation_range_.reset(new std::pair<glm::quat, glm::quat>(variant_to_quat(node["orientation_start"]), variant_to_quat(node["orientation_end"])));
			}
			if(node.has_key("color")) {
				color_ = variant_to_vec4(node["color"]);
			} else if(node.has_key("colour")) {
				color_ = variant_to_vec4(node["colour"]);
			}
			if(node.has_key("start_colour_range") && node.has_key("end_colour_range")) {
				glm::detail::tvec4<unsigned char> start;
				glm::detail::tvec4<unsigned char> end;
				ASSERT_LOG(node["start_colour_range"].is_list() && node["start_colour_range"].num_elements() == 4,
					"'start_colour_range' should be a list of 4 elements.");
				start.r = node["start_colour_range"][0].as_int32();
				start.g = node["start_colour_range"][1].as_int32();
				start.b = node["start_colour_range"][2].as_int32();
				start.a = node["start_colour_range"][3].as_int32();
				ASSERT_LOG(node["end_colour_range"].is_list() && node["end_colour_range"].num_elements() == 4,
					"'end_colour_range' should be a list of 4 elements.");
				end.r = node["end_colour_range"][0].as_int32();
				end.g = node["end_colour_range"][1].as_int32();
				end.b = node["end_colour_range"][2].as_int32();
				end.a = node["end_colour_range"][3].as_int32();
				color_range_.reset(new color_range(std::make_pair(start,end)));
			}
			if(node.has_key("particle_width")) {
				particle_width_ = Parameter::factory(node["particle_width"]);
			}
			if(node.has_key("particle_height")) {
				particle_height_ = Parameter::factory(node["particle_height"]);
			}
			if(node.has_key("particle_depth")) {
				particle_depth_ = Parameter::factory(node["particle_depth"]);
			}
			if(node.has_key("emits_type")) {
				ASSERT_LOG(node.has_key("emits_name"), 
					"Emitters that specify the 'emits_type' attribute must give have and 'emits_type' attribute");
				const std::string& etype = node["emits_type"].as_string();
				if(etype == "emitter_particle") {
					emits_type_ = EmitsType::EMITTER;
				} else if(etype == "visual_particle") {
					emits_type_ = EmitsType::VISUAL;
				} else if(etype == "technique_particle") {
					emits_type_ = EmitsType::TECHNIQUE;
				} else if(etype == "affector_particle") {
					emits_type_ = EmitsType::AFFECTOR;
				} else if(etype == "system_particle") {
					emits_type_ = EmitsType::SYSTEM;
				} else {
					ASSERT_LOG(false, "Unrecognised 'emit_type' attribute value: " << etype);
				}
				emits_name_ = node["emits_name"].as_string();
			}
			if(node.has_key("debug_draw") && node["debug_draw"].as_bool()) {
				ASSERT_LOG(false, "Implement debug draw.");
			}
			// Set a default duration for the emitter.
			ASSERT_LOG(duration_ != nullptr, "duration_ is null");
			duration_remaining_ = duration_->getValue(0);
		}

		Emitter::~Emitter()
		{
		}

		Emitter::Emitter(const Emitter& e)
			: EmitObject(e),
			  emission_rate_(e.emission_rate_),
			  time_to_live_(e.time_to_live_),
			  velocity_(e.velocity_),
			  angle_(e.angle_),
			  mass_(e.mass_),
			  duration_(e.duration_),
			  repeat_delay_(e.repeat_delay_),
			  particle_width_(e.particle_width_),
			  particle_height_(e.particle_height_),
			  particle_depth_(e.particle_depth_),
			  force_emission_(e.force_emission_),
			  force_emission_processed_(false),
			  can_be_deleted_(false),
			  emits_type_(e.emits_type_),
			  emits_name_(e.emits_name_),
			  emission_fraction_(0),
			  duration_remaining_(0),
			  color_(e.color_),
              repeat_delay_remaining_(0)
		{
			if(e.orientation_range_) {
				orientation_range_.reset(new std::pair<glm::quat,glm::quat>(e.orientation_range_->first, e.orientation_range_->second));
			}
			if(e.color_range_) {
				color_range_.reset(new color_range(color_range_->first, color_range_->second));
			}
			/*if(e.debug_draw_outline_) {
				debug_draw_outline_.reset(new BoxOutline());
				debug_draw_outline_->setColor(e.debug_draw_outline_->get_color());
			}*/
			duration_remaining_ = duration_->getValue(0);
		}

		void Emitter::handleEmitProcess(float t) 
		{
			auto tq = getTechnique();
			std::vector<Particle>& particles = tq->getActiveParticles();

			float duration = duration_->getValue(static_cast<float>(t));
			if(duration == 0.0f || duration_remaining_ >= 0.0f) {
				if(emits_type_ == EmitsType::VISUAL) {
					std::vector<Particle>::iterator start;

					//create_particles(particles, start, end, t);
					size_t cnt = calculateParticlesToEmit(t, tq->getQuota(), particles.size());
					// XXX: techincally this shouldn't be needed as we reserve the default quota upon initialising
					// the particle list. We could hit some pathological case where we allocate particles past
					// the quota (since it isn't enforced yet). This saves us from start from being invalidated
					// if push_back were to cause a reallocation.
					auto last_index = particles.size();
					particles.resize(particles.size() + cnt);
					//start = particles.end();
					for(size_t n = 0; n != cnt; ++n) {
						Particle p;
						initParticle(p, t);
						particles[n+last_index] = p;
					}

                    start = particles.begin() + last_index;
					for(auto it = start; it != particles.end(); ++it) {
						internalCreate(*it, t);
					}
					setParticleStartingValues(start, particles.end());
				} else {
					if(emits_type_ == EmitsType::EMITTER) {
						size_t cnt = calculateParticlesToEmit(t, tq->getEmitterQuota(), tq->getInstancedEmitters().size());
						//std::cerr << "XXX: Emitting " << cnt << " emitters" << std::endl;
						for(int n = 0; n != cnt; ++n) {
							EmitterPtr e = getParentContainer()->cloneEmitter(emits_name_);
							e->emitted_by = this;
							initParticle(*e, t);
							internalCreate(*e, t);
							memcpy(&e->current, &e->initial, sizeof(e->current));
							tq->addEmitter(e);
						}
					} else if(emits_type_ == EmitsType::AFFECTOR) {
						size_t cnt = calculateParticlesToEmit(t, tq->getAffectorQuota(), tq->getInstancedAffectors().size());
						for(int n = 0; n != cnt; ++n) {
							AffectorPtr a = getParentContainer()->cloneAffector(emits_name_);
							a->emitted_by = this;
							initParticle(*a, t);
							internalCreate(*a, t);
							memcpy(&a->current, &a->initial, sizeof(a->current));
							tq->addAffector(a);
						}
					} else if(emits_type_ == EmitsType::TECHNIQUE) {
						size_t cnt = calculateParticlesToEmit(t, tq->getTechniqueQuota(), tq->getParticleSystem()->getActiveTechniques().size());
						for(int n = 0; n != cnt; ++n) {
							TechniquePtr tq = getParentContainer()->cloneTechnique(emits_name_);
							tq->emitted_by = this;
							initParticle(*tq, t);
							internalCreate(*tq, t);
							memcpy(&tq->current, &tq->initial, sizeof(tq->current));
							tq->getParticleSystem()->addTechnique(tq);
						}
					} else if(emits_type_ == EmitsType::SYSTEM) {
						size_t cnt = calculateParticlesToEmit(t, tq->getSystemQuota(), getParentContainer()->getActiveParticleSystems().size());
						for(int n = 0; n != cnt; ++n) {
							ParticleSystemPtr ps = getParentContainer()->cloneParticleSystem(emits_name_);
							ps->emitted_by = this;
							initParticle(*ps, t);
							internalCreate(*ps, t);
							memcpy(&ps->current, &ps->initial, sizeof(ps->current));
							getParentContainer()->addParticleSystem(ps);
						}
					} else {
						ASSERT_LOG(false, "unknown emits_type: " << static_cast<int>(emits_type_));
					}
				}

				duration_remaining_ -= static_cast<float>(t);
				if(duration_remaining_ < 0.0f) {
					ASSERT_LOG(repeat_delay_ != nullptr, "repeat_delay_ is null");
					repeat_delay_remaining_ = repeat_delay_->getValue(t);
				}
			} else {
				repeat_delay_remaining_ -= static_cast<float>(t);
				if(repeat_delay_remaining_ < 0.0f) {
					ASSERT_LOG(duration_ != nullptr, "duration_ is null");
					duration_remaining_ = duration_->getValue(t);
				}
			}
		}

		size_t Emitter::calculateParticlesToEmit(float t, size_t quota, size_t current_size)
		{
			size_t cnt = 0;
			if(force_emission_) {
				if(!force_emission_processed_) {
					// Single shot of all particles at once.
					cnt = static_cast<size_t>(emission_rate_->getValue(getTechnique()->getParticleSystem()->getElapsedTime()));
					force_emission_processed_ = true;
				}
			} else {
				cnt = getEmittedParticleCountPerCycle(t);
			}
			if(current_size + cnt > quota) {
				if(current_size >= quota) {
					cnt = 0;
				} else {
					cnt = quota - current_size;
				}
			}
			return cnt;
		}

		void Emitter::createParticles(std::vector<Particle>& particles, 
			std::vector<Particle>::iterator& start, 
			std::vector<Particle>::iterator& end, 
			float t)
		{
		}

		void Emitter::setParticleStartingValues(const std::vector<Particle>::iterator& start, const std::vector<Particle>::iterator& end)
		{
			for(auto p = start; p != end; ++p) {
				memcpy(&p->current, &p->initial, sizeof(p->current));
			}
		}

		void Emitter::initParticle(Particle& p, float t)
		{
			auto ps = getTechnique()->getParticleSystem();
			init_physics_parameters(p.initial);
			init_physics_parameters(p.current);
			p.initial.position = current.position;
			p.initial.color = getColor();
			p.initial.time_to_live = time_to_live_->getValue(ps->getElapsedTime());
			p.initial.velocity = velocity_->getValue(ps->getElapsedTime());
			p.initial.mass = mass_->getValue(ps->getElapsedTime());
			p.initial.dimensions = getTechnique()->getDefaultDimensions();
			if(orientation_range_) {
				p.initial.orientation = glm::slerp(orientation_range_->first, orientation_range_->second, get_random_float(0.0f,1.0f));
			} else {
				p.initial.orientation = current.orientation;
			}
			p.initial.direction = getInitialDirection();
			p.emitted_by = this;
		}

		int Emitter::getEmittedParticleCountPerCycle(float t)
		{
			ASSERT_LOG(emission_rate_ != nullptr, "emission_rate_ is nullptr");
			// at each step we produce emission_rate()*process_step_time particles.
			float cnt = 0;
			emission_fraction_ = std::modf(emission_fraction_ + emission_rate_->getValue(t)*t, &cnt);
			return static_cast<int>(cnt);
		}

		float Emitter::generateAngle() const
		{
			float angle = angle_->getValue(getTechnique()->getParticleSystem()->getElapsedTime());
			if(angle_->type() == ParameterType::FIXED) {
				return get_random_float() * angle;
			}
			return angle;
		}

		glm::vec3 Emitter::getInitialDirection() const
		{
			float angle = generateAngle();
			//std::cerr << "angle:" << angle;
			if(angle != 0) {
				return create_deviating_vector(angle, current.direction);
			}
			return current.direction;
		}

		color_vector Emitter::getColor() const
		{
			if(color_range_) {
				return glm::detail::tvec4<unsigned char>(
					get_random_float(color_range_->first.r,color_range_->second.r),
					get_random_float(color_range_->first.g,color_range_->second.g),
					get_random_float(color_range_->first.b,color_range_->second.b),
					get_random_float(color_range_->first.a,color_range_->second.a));
			}
			color_vector c;
			c.r = uint8_t(color_.r * 255.0f);
			c.g = uint8_t(color_.g * 255.0f);
			c.b = uint8_t(color_.b * 255.0f);
			c.a = uint8_t(color_.a * 255.0f);
			return c;
		}

		void Emitter::handleDraw() const
		{
		}

		TechniquePtr Emitter::getTechnique() const
		{
			auto tq = technique_.lock();
			ASSERT_LOG(tq != nullptr, "No parent technique found.");
			return tq;
		}

		EmitterPtr Emitter::factory(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
		{
			ASSERT_LOG(node.has_key("type"), "emitter must have 'type' attribute");
			const std::string& ntype = node["type"].as_string();
			if(ntype == "circle") {
				return std::make_shared<CircleEmitter>(parent, node);
			} else if(ntype == "box") {
				return std::make_shared<BoxEmitter>(parent, node);
			} else if(ntype == "line") {
				return std::make_shared<LineEmitter>(parent, node);
			} else if(ntype == "point") {
				return std::make_shared<PointEmitter>(parent, node);
			} else if(ntype == "sphere_surface") {
				return std::make_shared<SphereSurfaceEmitter>(parent, node);
			}
			ASSERT_LOG(false, "Unrecognised emitter type: " << ntype);
			return nullptr;
		}
	}
}
