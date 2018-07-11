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

#include "glm/gtx/transform.hpp"

namespace KRE
{
	namespace Particles
	{
		Emitter::Emitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node, EmitterType type)
			: EmitObject(parent, node), 
			  type_(type),
			  emission_fraction_(0.0f),
			  force_emission_(node["force_emission"].as_bool(false)),
			  force_emission_processed_(false), 
			  can_be_deleted_(node["can_be_deleted"].as_bool(true)),
			  color_(1.0f,1.0f,1.0f,1.0f),
			  duration_remaining_(0),
              repeat_delay_remaining_(0),
			  particles_remaining_(0),
			  scale_(1.0f),
			  emit_only_2d_(node["emit_only_2d"].as_bool(false)),
			  orientation_follows_angle_(node["orientation_follows_angle"].as_bool(false))
		{
			initPhysics();

			setEmissionRate(node["emission_rate"]);

			if(node.has_key("time_to_live")) {
				time_to_live_ = Parameter::factory(node["time_to_live"]);
			} else {
				time_to_live_.reset(new Parameter(10.0f));
			}
			if(node.has_key("velocity")) {
				velocity_ = Parameter::factory(node["velocity"]);
			} else {
				velocity_.reset(new Parameter(100.0f));
			}
			if(node.has_key("angle")) {
				angle_ = Parameter::factory(node["angle"]);
			} else {
				angle_.reset(new Parameter(20.0f));
			}
			if(node.has_key("rotation")) {
				orientation_ = Parameter::factory(node["rotation"]);
			} else {
				orientation_.reset(new Parameter(0.0f));
			}
			if(node.has_key("scaling")) {
				scaling_ = Parameter::factory(node["scaling"]);
			} else {
				scaling_.reset(new Parameter(1.0f));
			}
			if(node.has_key("mass")) {
				mass_ = Parameter::factory(node["mass"]);
			} else {
				mass_.reset(new Parameter(1.0f));
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
			// Set a default duration for the emitter.
			if(duration_) {
				duration_remaining_ = duration_->getValue(0);
			}
			if(repeat_delay_) {
				repeat_delay_remaining_ = repeat_delay_->getValue(0);
			}			
		}

		Emitter::Emitter(std::weak_ptr<ParticleSystemContainer> parent, EmitterType type)
			: EmitObject(parent), 
			  type_(type),
			  emission_rate_(new Parameter(10.0f)),
			  time_to_live_(new Parameter(4.0f)),
			  velocity_(new Parameter(100.0f)),
			  angle_(new Parameter(20.0f)),
			  orientation_(new Parameter(0.0f)),
			  scaling_(new Parameter(1.0f)),
			  mass_(new Parameter(1.0f)),
			  duration_(nullptr),
			  repeat_delay_(nullptr),
			  orientation_range_(nullptr),
			  color_range_(nullptr),
			  color_(1.0f,1.0f,1.0f,1.0f),
			  particle_width_(nullptr),
			  particle_height_(nullptr),
			  particle_depth_(nullptr),
			  force_emission_(false),
			  force_emission_processed_(false), 
			  can_be_deleted_(false),
			  emission_fraction_(0.0f),
			  duration_remaining_(0),
			  repeat_delay_remaining_(0),
			  particles_remaining_(0),
			  scale_(1.0f),
			  emit_only_2d_(false),
			  orientation_follows_angle_(false)
		{
			initPhysics();
		}

		Emitter::Emitter(const Emitter& e)
			: EmitObject(e),
			  type_(e.type_),
			  emission_rate_(e.emission_rate_),
			  time_to_live_(e.time_to_live_),
			  velocity_(e.velocity_),
			  angle_(e.angle_),
			  orientation_(e.orientation_),
			  scaling_(e.scaling_),
			  mass_(e.mass_),
			  duration_(e.duration_),
			  repeat_delay_(e.repeat_delay_),
			  particle_width_(e.particle_width_),
			  particle_height_(e.particle_height_),
			  particle_depth_(e.particle_depth_),
			  force_emission_(e.force_emission_),
			  force_emission_processed_(false),
			  can_be_deleted_(false),
			  emission_fraction_(0),
			  duration_remaining_(0),
			  color_(e.color_),
              repeat_delay_remaining_(0),
			  particles_remaining_(e.particles_remaining_),
			  scale_(e.scale_),
			  emit_only_2d_(e.emit_only_2d_),
			  orientation_follows_angle_(e.orientation_follows_angle_)
		{
			if(e.orientation_range_) {
				orientation_range_.reset(new std::pair<glm::quat,glm::quat>(e.orientation_range_->first, e.orientation_range_->second));
			}
			if(e.color_range_) {
				color_range_.reset(new color_range(e.color_range_->first, e.color_range_->second));
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

		Emitter::~Emitter()
		{
		}

		void Emitter::writeInternal(variant_builder* build) const
		{
			switch (type_) {
				case KRE::Particles::EmitterType::POINT:
					build->add("type", "point");
					break;
				case KRE::Particles::EmitterType::LINE:
					build->add("type", "line");
					break;
				case KRE::Particles::EmitterType::BOX:
					build->add("type", "box");
					break;
				case KRE::Particles::EmitterType::CIRCLE:
					build->add("type", "circle");
					break;
				case KRE::Particles::EmitterType::SPHERE_SURFACE:
					build->add("type", "sphere_surface");
					break;
				default: break;
			}
			if(force_emission_) {
				build->add("force_emission", force_emission_);
			}
			if(!can_be_deleted_) {
				build->add("can_be_deleted", can_be_deleted_);
			}
			if(emission_rate_ /*&& emission_rate_->getType() != ParameterType::FIXED && emission_rate_->getValue() != 10.0f*/) {
				build->add("emission_rate", emission_rate_->write());
			}
			if(time_to_live_ /*&& time_to_live_->getType() != ParameterType::FIXED && time_to_live_->getValue() != 10.0f*/) {
				build->add("time_to_live", time_to_live_->write());
			}
			if(orientation_) {
				build->add("rotation", orientation_->write());
			}
			if(scaling_) {
				build->add("scaling", scaling_->write());
			}
			if(velocity_ /*&& velocity_->getType() != ParameterType::FIXED && velocity_->getValue() != 10.0f*/) {
				build->add("velocity", velocity_->write());
			}
			if(angle_ /*&& angle_->getType() != ParameterType::FIXED && angle_->getValue() != 20.0f*/) {
				build->add("angle", angle_->write());
			}
			if(mass_ /*&& mass_->getType() != ParameterType::FIXED && mass_->getValue() != 1.0f*/) {
				build->add("mass", mass_->write());
			}
			if(duration_) {
				build->add("duration", duration_->write());
			}
			if(initial.position != glm::vec3(0.0f)) {
				build->add("position", vec3_to_variant(initial.position));
			}
			if(initial.orientation != glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) {
				build->add("orientation", quat_to_variant(initial.orientation));
			}
			if(orientation_range_) {
				build->add("orientation_start", quat_to_variant(orientation_range_->first));
				build->add("orientation_end", quat_to_variant(orientation_range_->second));
			}
			if(color_ != glm::vec4(1.0f)) {
				build->add("color", vec4_to_variant(color_));
			}
			if(color_range_) {
				build->add("start_color_range", vec4_to_variant(color_range_->first));
				build->add("end_color_range", vec4_to_variant(color_range_->second));
			}
			if(particle_width_ != nullptr && particle_width_ == particle_height_ && particle_width_ == particle_depth_) {
				build->add("all_dimensions", particle_width_->write());
			} else {
				if(particle_width_ != nullptr) {
					build->add("particle_width", particle_width_->write());
				}
				if(particle_height_ != nullptr) {
					build->add("particle_height", particle_height_->write());
				}
				if(particle_depth_ != nullptr) {
					build->add("particle_depth", particle_depth_->write());
				}
			}
			if(emit_only_2d_) {
				build->add("emit_only_2d", emit_only_2d_);
			}
			if(orientation_follows_angle_) {
				build->add("orientation_follows_angle", orientation_follows_angle_);
			}
		}

		void Emitter::initPhysics()
		{
			init_physics_parameters(initial);
			init_physics_parameters(current);
			initial.time_to_live = current.time_to_live = 100000000.0;
			initial.velocity = current.velocity = 0;
		}

		void Emitter::init()
		{
			calculateQuota();
		}

		void Emitter::setEmissionRate(variant node)
		{
			if(node.is_null() == false) {
				emission_rate_ = Parameter::factory(node);
			} else {
				emission_rate_.reset(new Parameter(10));
			}
		}

		void CircleEmitter::setRadius(variant node)
		{
			if(node.is_null() == false) {
				circle_radius_ = Parameter::factory(node);
			} else {
				circle_radius_.reset(new Parameter(10));
			}
		}

		void Emitter::calculateQuota()
		{
			auto& psystem = getParentContainer()->getParticleSystem();
			particles_remaining_ = psystem->getParticleQuota();
		}

		void Emitter::visualEmitProcess(float t)
		{
			auto& psystem = getParentContainer()->getParticleSystem();
			std::vector<Particle>& particles = psystem->getActiveParticles();
			std::vector<Particle>::iterator start;

			int cnt = calculateParticlesToEmit(t, particles_remaining_, particles.size());
			if(duration_) {
				particles_remaining_ -= cnt;
				if(particles_remaining_ <= 0) {
					setEnable(false);
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

		void Emitter::handleEnable()
		{
			auto& psystem = getParentContainer()->getParticleSystem();
			if(isEnabled()) {
				if(duration_) {
					duration_remaining_ = duration_->getValue(psystem->getElapsedTime());
					calculateQuota();
				}
				if(duration_remaining_ > 0) {
					repeat_delay_remaining_ = 0;
				}
			} else {
				if(repeat_delay_) {
					repeat_delay_remaining_ = repeat_delay_->getValue(psystem->getElapsedTime());
				} else {
					setEnable(true);
				}
				if(repeat_delay_remaining_ > 0) {
					duration_remaining_ = 0;
				}
			}
		}

		void Emitter::handleEmitProcess(float t) 
		{
			if(isEnabled()) {
				visualEmitProcess(t);
				
				if(duration_) {
					duration_remaining_ -= t;
					if(duration_remaining_ < 0.0f) {
						setEnable(false);					
					}
				}
			} else if(repeat_delay_) {
				repeat_delay_remaining_ -= t;
				if(repeat_delay_remaining_ < 0) {
					setEnable(true);
				}
			}
		}

		void Emitter::setEmitOnly2D(bool f) 
		{ 
			emit_only_2d_ = f; 
		}

		int Emitter::calculateParticlesToEmit(float t, int quota, int current_size)
		{
			auto& psystem = getParentContainer()->getParticleSystem();
			int cnt = 0;
			if(force_emission_) {
				if(!force_emission_processed_) {
					// Single shot of all particles at once.
					cnt = static_cast<int>(emission_rate_->getValue(psystem->getElapsedTime()));
					if(cnt < 0) {
						cnt = 0;
					}
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
			auto& psystem = getParentContainer()->getParticleSystem();
			init_physics_parameters(p.initial);
			init_physics_parameters(p.current);
			p.initial.position = current.position;
			if(emit_only_2d_) {
				p.initial.position.z = 0.0f;
			}
			p.initial.color = getColor();
			p.initial.time_to_live = time_to_live_->getValue(psystem->getElapsedTime());
			p.initial.velocity = velocity_->getValue(psystem->getElapsedTime());
			p.initial.mass = mass_->getValue(psystem->getElapsedTime());
			p.initial.dimensions = psystem->getDefaultDimensions();
			if(particle_width_ != nullptr) {
				p.initial.dimensions.x = particle_width_->getValue(t);
			}
			if(particle_height_ != nullptr) {
				p.initial.dimensions.y = particle_height_->getValue(t);
			}
			if(particle_depth_ != nullptr) {
				p.initial.dimensions.z = particle_depth_->getValue(t);
			}
			const float scale_value = scaling_->getValue(psystem->getElapsedTime());
			p.initial.dimensions.x *= scale_.x * scale_value;
			p.initial.dimensions.y *= scale_.y * scale_value;
			p.initial.dimensions.z *= scale_.z;
			if(orientation_range_) {
				p.initial.orientation = glm::slerp(orientation_range_->first, orientation_range_->second, get_random_float(0.0f,1.0f));
			} else {
				const float angle = orientation_->getValue(psystem->getElapsedTime());
				const auto qaxis = glm::angleAxis(angle / 180.0f * static_cast<float>(M_PI), glm::vec3(0.0f, 0.0f, 1.0f));
				p.initial.orientation = qaxis * glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			}
			p.initial.direction = getInitialDirection();
			if(emit_only_2d_) {
				p.initial.direction.z = 0.0f;
			}
			if(orientation_follows_angle_) {
				glm::vec3 a(0.0f, 1.0f, 0.0f);
				glm::vec3 v = -glm::cross(p.initial.direction, a);
				float angle = acos(glm::dot(p.initial.direction, a) / (glm::length(p.initial.direction) * glm::length(a)));
				glm::mat4 rotmat = glm::rotate(angle, v);

				p.current.orientation = p.initial.orientation = glm::toQuat(rotmat);
			}
			//std::cerr << "initial direction: " << p.initial.direction << " vel = " << p.initial.velocity << "\n";
			p.emitted_by = this;
		}

		void Emitter::getOrientationRange(glm::quat* start, glm::quat* end) const
		{
			ASSERT_LOG(orientation_range_ != nullptr, "Orientation range not defined.");
			if(start) {
				*start = orientation_range_->first;
			}
			if(end) {
				*end = orientation_range_->second;
			}
		}

		int Emitter::getEmittedParticleCountPerCycle(float t)
		{
			ASSERT_LOG(emission_rate_ != nullptr, "emission_rate_ is nullptr");
			// at each step we produce emission_rate()*process_step_time particles.
			float cnt = 0;
			float particles_per_cycle = emission_rate_->getValue(t) * t;
			if(particles_per_cycle < 0) {
				particles_per_cycle = 0;
			}
			emission_fraction_ = std::modf(emission_fraction_ + particles_per_cycle, &cnt);
			//LOG_DEBUG("EPCPC: frac: " << emission_fraction_ << ", integral: " << cnt << ", ppc: " << particles_per_cycle << ", time: " << t);
			return static_cast<int>(cnt);
		}

		float Emitter::generateAngle() const
		{
			auto& psystem = getParentContainer()->getParticleSystem();
			float angle = angle_->getValue(psystem->getElapsedTime());
			return angle;
		}

		glm::vec3 Emitter::getInitialDirection() const
		{
			float angle = generateAngle();
			if(angle != 0) {
				return create_deviating_vector(angle, initial.direction);
			}
			return initial.direction;
		}

		color_vector Emitter::getColor() const
		{
			if(color_range_) {
				return glm::tvec4<unsigned char>(
					get_random_float(color_range_->first.r,color_range_->second.r) * 255.0f,
					get_random_float(color_range_->first.g,color_range_->second.g) * 255.0f,
					get_random_float(color_range_->first.b,color_range_->second.b) * 255.0f,
					get_random_float(color_range_->first.a,color_range_->second.a) * 255.0f);
			}
			color_vector c;
			c.r = uint8_t(color_.r * 255.0f);
			c.g = uint8_t(color_.g * 255.0f);
			c.b = uint8_t(color_.b * 255.0f);
			c.a = uint8_t(color_.a * 255.0f);
			return c;
		}

		const Emitter::color_range& Emitter::getColorRange() const 
		{ 
			ASSERT_LOG(color_range_ != nullptr, "Color range is empty.");
			return *color_range_; 
		}

		void Emitter::setColorRange(const glm::vec4& start, const glm::vec4& end)
		{
			color_range_.reset(new color_range(start, end));
		}

		void Emitter::handleDraw(const WindowPtr& wnd) const
		{
			auto& psystem = getParentContainer()->getParticleSystem();
			//if(isEnabled()) {
				static DebugDrawHelper ddh;
				ddh.update(current.position - current.dimensions / 2.0f, current.position + current.dimensions / 2.0f, Color::colorGreen());
				ddh.setCamera(psystem->getCamera());
				ddh.useGlobalModelMatrix(psystem->ignoreGlobalModelMatrix());
				ddh.setDepthEnable(true);
				wnd->render(&ddh);
			//}
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

		EmitterPtr Emitter::factory(std::weak_ptr<ParticleSystemContainer> parent, EmitterType type)
		{
			switch (type) {
				case EmitterType::POINT:
					return std::make_shared<PointEmitter>(parent);
				case EmitterType::LINE:
					return std::make_shared<LineEmitter>(parent);
				case EmitterType::BOX:
					return std::make_shared<BoxEmitter>(parent);
				case EmitterType::CIRCLE:
					return std::make_shared<CircleEmitter>(parent);
				case EmitterType::SPHERE_SURFACE:
					return std::make_shared<SphereSurfaceEmitter>(parent);
				default:
					ASSERT_LOG(false, "Unkown emitter type given: " << static_cast<int>(type));
					break;
			}
			return nullptr;
		}

		EmitterPtr Emitter::factory_similar(std::weak_ptr<ParticleSystemContainer> parent, EmitterType type, const Emitter& existing)
		{
			EmitterPtr result = factory(parent, type);
			result->emission_rate_ = existing.emission_rate_;
			result->time_to_live_ = existing.time_to_live_;
			result->velocity_ = existing.velocity_;
			result->angle_ = existing.angle_;
			result->mass_ = existing.mass_;
			result->orientation_ = existing.orientation_;
			result->scaling_ = existing.scaling_;
			result->duration_ = existing.duration_;
			result->repeat_delay_ = existing.repeat_delay_;
			result->color_range_ = existing.color_range_;
			result->color_= existing.color_;
			result->particle_width_ = existing.particle_width_;
			result->particle_height_ = existing.particle_height_;
			result->particle_depth_ = existing.particle_depth_;
			result->force_emission_ = existing.force_emission_;
			result->force_emission_processed_ = existing.force_emission_processed_;
			result->can_be_deleted_ = existing.can_be_deleted_;
			result->scale_ = existing.scale_;
			result->emit_only_2d_ = existing.emit_only_2d_;
			return result;
		}


		CircleEmitter::CircleEmitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 			
			: Emitter(parent, node, EmitterType::CIRCLE), 
			  circle_radius_(Parameter::factory(node["circle_radius"])),
			  circle_step_(node["circle_step"].as_float(0.1f)), 
			  circle_angle_(node["circle_angle"].as_float(0)), 
			  circle_random_(node["emit_random"].as_bool(true)),
			  normal_(0.0f, 1.0f, 0.0f)
		{
			if(node.has_key("normal")) {
				normal_ = variant_to_vec3(node["normal"]);
			}
		}

		CircleEmitter::CircleEmitter(std::weak_ptr<ParticleSystemContainer> parent)
			: Emitter(parent, EmitterType::CIRCLE), 
			  circle_radius_(new Parameter(1.0f)),
			  circle_step_(0.1f), 
			  circle_angle_(0.0f), 
			  circle_random_(true),
			  normal_(0.0f, 1.0f, 0.0f)
		{
		}

		void CircleEmitter::handleWrite(variant_builder* build) const 
		{
			Emitter::writeInternal(build);
			if(circle_radius_ != nullptr /*&& circle_radius_->getType() != ParameterType::FIXED && circle_radius_->getValue() != 1.0f*/) {
				build->add("circle_radius", circle_radius_->write());
			}
			if(circle_step_ != 0.1f) {
				build->add("circle_step", circle_step_);
			}
			if(circle_angle_ != 0.0f) {
				build->add("circle_angle", circle_angle_);
			}
			if(circle_random_ == false) {
				build->add("emit_random", circle_random_);
			}
			if(normal_ != glm::vec3(0.0f, 1.0f, 0.0f)) {
				build->add("normal", vec3_to_variant(normal_));
			}
		}

		void CircleEmitter::internalCreate(Particle& p, float t)
		{
			float angle = 0.0f;
			if(circle_random_) {
				angle = get_random_float(0.0f, static_cast<float>(2.0 * M_PI));
			} else {
				angle = t * circle_step_;
			}
			const float theta = angle + circle_angle_ / 180.0f * static_cast<float>(M_PI);

			const float r = circle_radius_->getValue();
			if(isEmitOnly2D()) {
				p.initial.position.x += r * sin(theta);
				p.initial.position.y += r * cos(theta);
			} else {
				//NB: must have a sane normal set for this code to work.
				const float s = 1.0f / (normal_.x * normal_.x + + normal_.y * normal_.y + normal_.z * normal_.z);
				const float v1x = s * normal_.z;
				const float v1y = 0.0f;
				const float v1z = s * -normal_.x;

				// Calculate v2 as cross product of v3 and v1.
				// Since v1y is 0, it could be removed from the following calculations. Keeping it for consistency.
				const float v2x = normal_.y * v1z - normal_.z * v1y;
				const float v2y = normal_.z * v1x - normal_.x * v1z;
				const float v2z = normal_.x * v1y - normal_.y * v1x;

				// For each circle point.
				p.initial.position.x += r * (v1x * cos(theta) + v2x * sin(theta));
				p.initial.position.y += r * (v1y * cos(theta) + v2y * sin(theta));
				p.initial.position.z += r * (v1z * cos(theta) + v2z * sin(theta));
			}

		}

		BoxEmitter::BoxEmitter(std::weak_ptr<ParticleSystemContainer> parent) 
			: Emitter(parent, EmitterType::BOX), 
			  box_dimensions_(1.0f) 
		{
		}

		BoxEmitter::BoxEmitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
			: Emitter(parent, node, EmitterType::BOX), 
			  box_dimensions_(1.0f) 
		{
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

		void BoxEmitter::internalCreate(Particle& p, float t) 
		{
			p.initial.position.x += get_random_float(0.0f, box_dimensions_.x) - box_dimensions_.x/2;
			p.initial.position.y += get_random_float(0.0f, box_dimensions_.y) - box_dimensions_.y/2;
			p.initial.position.z += get_random_float(0.0f, box_dimensions_.z) - box_dimensions_.z/2;
		}

		void BoxEmitter::handleWrite(variant_builder* build) const 
		{
			Emitter::writeInternal(build);
			if(box_dimensions_.x != 1.0f) {
				build->add("box_width", box_dimensions_.x);
			}
			if(box_dimensions_.y != 1.0f) {
				build->add("box_height", box_dimensions_.y);
			}
			if(box_dimensions_.z != 1.0f) {
				build->add("box_depth", box_dimensions_.z);
			}
		}


		LineEmitter::LineEmitter(std::weak_ptr<ParticleSystemContainer> parent) 
			: Emitter(parent, EmitterType::LINE), 
			  line_end_(0.0f), 
			  line_deviation_(0.0f),
			  min_increment_(0.0f), 
			  max_increment_(0.0f) 
		{
		}

		LineEmitter::LineEmitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
			: Emitter(parent, node, EmitterType::LINE), 
			  line_end_(0.0f), 
			  line_deviation_(0.0f),
			  min_increment_(0.0f), 
			  max_increment_(0.0f) 
		{
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

		void LineEmitter::internalCreate(Particle& p, float t)
		{
			// XXX todo
		}

		void LineEmitter::handleWrite(variant_builder* build) const 
		{
			Emitter::writeInternal(build);
			if(line_deviation_!= 0.0f) {
				build->add("max_deviation", line_deviation_);
			}
			if(min_increment_!= 0.0f) {
				build->add("min_increment", min_increment_);
			}
			if(max_increment_!= 0.0f) {
				build->add("max_increment", max_increment_);
			}
			if(line_end_ != glm::vec3(0.0f)) {
				build->add("line_end", vec3_to_variant(line_end_));
			}
		}


		PointEmitter::PointEmitter(std::weak_ptr<ParticleSystemContainer> parent) 
			: Emitter(parent, EmitterType::POINT) 
		{
		}

		PointEmitter::PointEmitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
			: Emitter(parent, node, EmitterType::POINT) 
		{
		}

		void PointEmitter::internalCreate(Particle& p, float t) 
		{
			// intentionally does nothing.
		}

		void PointEmitter::handleWrite(variant_builder* build) const 
		{
			Emitter::writeInternal(build);
			// no extra parameters.
		}


		SphereSurfaceEmitter::SphereSurfaceEmitter(std::weak_ptr<ParticleSystemContainer> parent) 
			: Emitter(parent, EmitterType::SPHERE_SURFACE), 
			  radius_(new Parameter(1.0f)) 
		{
		}

		SphereSurfaceEmitter::SphereSurfaceEmitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
			: Emitter(parent, node, EmitterType::SPHERE_SURFACE), 
			  radius_(nullptr) 
		{
			if(node.has_key("radius")) {
				radius_ = Parameter::factory(node["radius"]);
			} else {
				radius_.reset(new Parameter(1.0f));
			}
		}

		void SphereSurfaceEmitter::internalCreate(Particle& p, float t) 
		{
			float theta = get_random_float(0, 2.0f * static_cast<float>(M_PI));
			float phi = acos(get_random_float(-1.0f, 1.0f));
			float r = radius_->getValue(t);
			p.initial.position.x += r * sin(phi) * cos(theta);
			p.initial.position.y += r * sin(phi) * sin(theta);
			p.initial.position.z += r * cos(phi);
		}

		void SphereSurfaceEmitter::handleWrite(variant_builder* build) const 
		{
			Emitter::writeInternal(build);
			if(radius_ != nullptr/* && radius_->getType() != ParameterType::FIXED && radius_->getValue() != 1.0f*/) {
				build->add("radius", radius_->write());
			}
		}

	}
}
