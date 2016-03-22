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
#include "WindowManager.hpp"
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
				  circle_random_(node["emit_random"].as_bool(true)),
				  use_x_(false), use_y_(false), use_z_(false)
			{
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
			bool use_x_;
			bool use_y_;
			bool use_z_;

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
			virtual EmitterPtr clone() override {
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
				  radius_(node["radius"].as_float(1.0f)) 
			{}
		protected:
			void internalCreate(Particle& p, float t) {
				float theta = get_random_float(0, 2.0f * static_cast<float>(M_PI));
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
			  duration_remaining_(0),
              repeat_delay_remaining_(0),
			  particles_remaining_(0),
			  scale_(1.0f)
		{
			init_physics_parameters(initial);
			init_physics_parameters(current);
			initial.time_to_live = current.time_to_live = 100000000.0;
			initial.velocity = current.velocity = 0;

			setEmissionRate(node["emission_rate"]);

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
			}
			if(node.has_key("repeat_delay")) {
				repeat_delay_ = Parameter::factory(node["repeat_delay"]);
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
				glm::tvec4<unsigned char> start;
				glm::tvec4<unsigned char> end;
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
			if(node.has_key("all_dimensions")) {
				particle_depth_ = particle_height_ = particle_width_ = Parameter::factory(node["all_dimensions"]);
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
			// Set a default duration for the emitter.
			if(duration_) {
				duration_remaining_ = duration_->getValue(0);
			}
			if(repeat_delay_) {
				repeat_delay_remaining_ = repeat_delay_->getValue(0);
			}
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
              repeat_delay_remaining_(0),
			  particles_remaining_(e.particles_remaining_),
			  scale_(e.scale_)
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
			if(duration_) {
				duration_remaining_ = duration_->getValue(0);
			}
			if(repeat_delay_) {
				repeat_delay_remaining_ = repeat_delay_->getValue(0);
			}
		}

		void Emitter::setEmissionRate(variant node)
		{
			if(node.is_null() == false) {
				emission_rate_ = Parameter::factory(node);
			} else {
				emission_rate_.reset(new FixedParameter(10));
			}
		}

		void Emitter::calculateQuota()
		{
			auto tq = getTechnique();
			switch(emits_type_)
			{
			case EmitsType::VISUAL: particles_remaining_ = tq->getQuota(); break;
			case EmitsType::EMITTER: particles_remaining_ = tq->getEmitterQuota(); break;
			case EmitsType::AFFECTOR: particles_remaining_ = tq->getAffectorQuota(); break;
			case EmitsType::TECHNIQUE: particles_remaining_ = tq->getTechniqueQuota(); break;
			case EmitsType::SYSTEM: particles_remaining_ = tq->getSystemQuota(); break;
			default: 
				ASSERT_LOG(false, "emits_type_ unknown: " << static_cast<int>(emits_type_));
				break;
			}
		}

		void Emitter::visualEmitProcess(float t)
		{
			auto tq = getTechnique();
			std::vector<Particle>& particles = tq->getActiveParticles();
			std::vector<Particle>::iterator start;

			int cnt = calculateParticlesToEmit(t, particles_remaining_, particles.size());
			if(duration_) {
				particles_remaining_ -= cnt;
				if(particles_remaining_ <= 0) {
					enable(false);
				}
			}

			//LOG_DEBUG(name() << " emits " << cnt << " particles, " << particles_remaining_ << " remain. active_particles=" << particles.size() << ", t=" << getTechnique()->getParticleSystem()->getElapsedTime());

			// XXX: techincally this shouldn't be needed as we reserve the default quota upon initialising
			// the particle list. We could hit some pathological case where we allocate particles past
			// the quota (since it isn't enforced yet). This saves us from start from being invalidated
			// if push_back were to cause a reallocation.
			auto last_index = particles.size();
			particles.resize(particles.size() + cnt);
			//start = particles.end();
			for(int n = 0; n != cnt; ++n) {
				Particle p;
				initParticle(p, t);
				particles[n+last_index] = p;
			}

			start = particles.begin() + last_index;
			for(auto it = start; it != particles.end(); ++it) {
				internalCreate(*it, t);
			}
			setParticleStartingValues(start, particles.end());
		}

		void Emitter::emitterEmitProcess(float t)
		{
			auto tq = getTechnique();
			std::vector<EmitterPtr>& emitters = tq->getActiveEmitters();

			int cnt = getEmittedParticleCountPerCycle(t);
			if(duration_) {
				particles_remaining_ -= cnt;
				if(particles_remaining_ <= 0) {
					enable(false);
				}
			}

			if(cnt <= 0) {
				return;
			}

			auto container = getParentContainer();

			for(int i = 0; i < cnt; ++i) {
				EmitterPtr spawned_child = container->cloneEmitter(emits_name_);	
				spawned_child->init(getTechnique());
				initParticle(*spawned_child, t);
				internalCreate(*spawned_child, t);
				memcpy(&spawned_child->current, &spawned_child->initial, sizeof(spawned_child->current));
				tq->getActiveEmitters().push_back(spawned_child);
			}
		}

		void Emitter::handleEnable()
		{
			if(isEnabled()) {
				if(duration_) {
					duration_remaining_ = duration_->getValue(getTechnique()->getParticleSystem()->getElapsedTime());
					calculateQuota();
				}
				if(duration_remaining_ > 0) {
					repeat_delay_remaining_ = 0;
				}
			} else {
				if(repeat_delay_) {
					repeat_delay_remaining_ = repeat_delay_->getValue(getTechnique()->getParticleSystem()->getElapsedTime());
				} else {
					enable(true);
				}
				if(repeat_delay_remaining_ > 0) {
					duration_remaining_ = 0;
				}
			}
		}

		void Emitter::handleEmitProcess(float t) 
		{
			if(isEnabled()) {
				switch(emits_type_) {
				case EmitsType::VISUAL:		visualEmitProcess(t); break;
				case EmitsType::EMITTER:	emitterEmitProcess(t); break;
				case EmitsType::AFFECTOR:	// XXX writeme
				case EmitsType::TECHNIQUE:	// XXX writeme
				case EmitsType::SYSTEM:		// XXX writeme
				default: 
					ASSERT_LOG(false, "Unhandled emits_type_: " << static_cast<int>(emits_type_));
					break;
				}

				if(duration_) {
					duration_remaining_ -= t;
					if(duration_remaining_ < 0.0f) {
						enable(false);					
					}
				}
			} else if(repeat_delay_) {
				repeat_delay_remaining_ -= t;
				if(repeat_delay_remaining_ < 0) {
					enable(true);
				}
			}
		}

		int Emitter::calculateParticlesToEmit(float t, int quota, int current_size)
		{
			int cnt = 0;
			if(force_emission_) {
				if(!force_emission_processed_) {
					// Single shot of all particles at once.
					cnt = static_cast<int>(emission_rate_->getValue(getTechnique()->getParticleSystem()->getElapsedTime()));
					force_emission_processed_ = true;
				}
			} else {
				cnt = getEmittedParticleCountPerCycle(t);
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
			if(particle_width_ != nullptr) {
				p.initial.dimensions.x = particle_width_->getValue(t);
			}
			if(particle_height_ != nullptr) {
				p.initial.dimensions.y = particle_height_->getValue(t);
			}
			if(particle_depth_ != nullptr) {
				p.initial.dimensions.z = particle_depth_->getValue(t);
			}
			p.initial.dimensions.x *= scale_.x;
			p.initial.dimensions.y *= scale_.y;
			p.initial.dimensions.z *= scale_.z;
			if(orientation_range_) {
				p.initial.orientation = glm::slerp(orientation_range_->first, orientation_range_->second, get_random_float(0.0f,1.0f));
			} else {
				p.initial.orientation = current.orientation;
			}
			p.initial.direction = getInitialDirection();
			//std::cerr << "initial direction: " << p.initial.direction << " vel = " << p.initial.velocity << "\n";
			p.emitted_by = this;
		}

		int Emitter::getEmittedParticleCountPerCycle(float t)
		{
			ASSERT_LOG(emission_rate_ != nullptr, "emission_rate_ is nullptr");
			// at each step we produce emission_rate()*process_step_time particles.
			float cnt = 0;
			const float particles_per_cycle = emission_rate_->getValue(t) * t;
			emission_fraction_ = std::modf(emission_fraction_ + particles_per_cycle, &cnt);
			//LOG_DEBUG("EPCPC: frac: " << emission_fraction_ << ", integral: " << cnt << ", ppc: " << particles_per_cycle << ", time: " << t);
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
			//std::cerr << "angle:" << angle << "\n";
			if(angle != 0) {
				return create_deviating_vector(angle, initial.direction);
			}
			return initial.direction;
		}

		color_vector Emitter::getColor() const
		{
			if(color_range_) {
				return glm::tvec4<unsigned char>(
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

		void Emitter::handleDraw(const WindowPtr& wnd) const
		{
			//if(isEnabled()) {
				static DebugDrawHelper ddh;
				ddh.update(current.position - current.dimensions / 2.0f, current.position + current.dimensions / 2.0f, Color::colorGreen());
				ddh.setCamera(getTechnique()->getCamera());
				ddh.useGlobalModelMatrix(getTechnique()->ignoreGlobalModelMatrix());
				ddh.setDepthEnable(true);
				wnd->render(&ddh);
			//}
		}

		TechniquePtr Emitter::getTechnique() const
		{
			auto tq = technique_.lock();
			ASSERT_LOG(tq != nullptr, "No parent technique found.");
			return tq;
		}

		void Emitter::init(std::weak_ptr<Technique> tq)
		{
			technique_ = tq;
			calculateQuota();
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
