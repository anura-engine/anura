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

#pragma once

#include "ParticleSystemFwd.hpp"

namespace KRE
{
	namespace Particles
	{
		enum class EmitterType {
			POINT,
			LINE,
			BOX,
			CIRCLE,
			SPHERE_SURFACE,
		};

		class Emitter : public EmitObject
		{
		public:
			typedef std::pair<glm::vec4,glm::vec4> color_range;

			explicit Emitter(std::weak_ptr<ParticleSystemContainer> parent, EmitterType type);
			explicit Emitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node, EmitterType type);
			virtual ~Emitter();
			Emitter(const Emitter&);

			void init();
			void initPhysics();

			EmitterType getType() const { return type_; }

			int getEmittedParticleCountPerCycle(float t);
			color_vector getColor() const;

			void setEmissionRate(variant node);

			const glm::quat& getOrientation() const { return initial.orientation; }
			void setOrientation(const glm::quat& q) { current.orientation = initial.orientation = q; }
			bool hasOrientationRange() const { return orientation_range_ != nullptr; }
			void getOrientationRange(glm::quat* start, glm::quat* end) const;
			void setOrientationRange(const glm::quat& start, const glm::quat& end) {
				orientation_range_.reset(new std::pair<glm::quat, glm::quat>(start, end));
			}
			void clearOrientationRange() { orientation_range_.reset(); }

			const ParameterPtr& getEmissionRate() const { return emission_rate_; }
			const ParameterPtr& getTimeToLive() const { return time_to_live_; }
			const ParameterPtr& getVelocity() const { return velocity_; }
			const ParameterPtr& getAngle() const { return angle_; }
			const ParameterPtr& getMass() const { return mass_; }
			const ParameterPtr& getOrientationParam() const { return orientation_; }
			const ParameterPtr& getScaling() const { return scaling_; }

			const ParameterPtr& getDuration() const { return duration_; }
			const ParameterPtr& getRepeatDelay() const { return repeat_delay_; }


			const ParameterPtr& getParticleWidth() const { return particle_width_; }
			const ParameterPtr& getParticleHeight() const { return particle_height_; }
			const ParameterPtr& getParticleDepth() const { return particle_depth_; }

			bool getForceEmission() const { return force_emission_; }
			void setForceEmission(bool f) { force_emission_ = f; }

			bool getCanBeDeleted() const { return can_be_deleted_; }
			void setCanBeDeleted(bool f) { can_be_deleted_ = f; }

			const glm::vec4& getColorFloat() const { return color_; }
			void setColor(const glm::vec4& col) { color_ = col; }
			bool hasColorRange() const { return color_range_ != nullptr; }
			const color_range& getColorRange() const;
			void clearColorRange() { color_range_.reset(); }
			void setColorRange(const glm::vec4& start, const glm::vec4& end);

			bool isEmitOnly2D() const { return emit_only_2d_; }
			void setEmitOnly2D(bool f);

			bool doesOrientationFollowDirection() const { return orientation_follows_angle_; }
			void setOrientationFollowsDirection(bool f) { orientation_follows_angle_ = f; }

			virtual EmitterPtr clone() = 0;
			static EmitterPtr factory(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			static EmitterPtr factory(std::weak_ptr<ParticleSystemContainer> parent, EmitterType type);

			//make an emitter of a new type but with similar parameters to an existing emitter.
			static EmitterPtr factory_similar(std::weak_ptr<ParticleSystemContainer> parent, EmitterType type, const Emitter& existing);
		protected:
			virtual void internalCreate(Particle& p, float t) = 0;
			virtual bool durationExpired() override { return can_be_deleted_; }
			void writeInternal(variant_builder* build) const;
		private:
			virtual void handleEmitProcess(float t) override;
			virtual void handleDraw(const WindowPtr& wnd) const override;
			void handleEnable() override;
			void visualEmitProcess(float t);

			EmitterType type_;

			// These are generation parameters.
			ParameterPtr emission_rate_;
			ParameterPtr time_to_live_;
			ParameterPtr velocity_;
			ParameterPtr angle_;
			ParameterPtr orientation_;
			ParameterPtr scaling_;
			ParameterPtr mass_;
			// This is the duration that the emitter lives for
			ParameterPtr duration_;
			// this is the delay till the emitter repeats.
			ParameterPtr repeat_delay_;
			std::unique_ptr<std::pair<glm::quat, glm::quat>> orientation_range_;
			std::shared_ptr<color_range> color_range_;
			glm::vec4 color_;
			ParameterPtr particle_width_;
			ParameterPtr particle_height_;
			ParameterPtr particle_depth_;
			bool force_emission_;
			bool force_emission_processed_;
			bool can_be_deleted_;

			void initParticle(Particle& p, float t);
			void setParticleStartingValues(const std::vector<Particle>::iterator& start, const std::vector<Particle>::iterator& end);
			void createParticles(std::vector<Particle>& particles, std::vector<Particle>::iterator& start, std::vector<Particle>::iterator& end, float t);
			int calculateParticlesToEmit(float t, int quota, int current_size);
			void calculateQuota();

			float generateAngle() const;
			glm::vec3 getInitialDirection() const;

			//BoxOutlinePtr debug_draw_outline_;

			// working items
			// Any "left over" fractional count of emitted particles
			float emission_fraction_;
			// time till the emitter stops emitting.
			float duration_remaining_;
			// time remaining till a stopped emitter restarts.
			float repeat_delay_remaining_;

			int particles_remaining_;

			glm::vec3 scale_;

			bool emit_only_2d_;
			bool orientation_follows_angle_;

			Emitter() = delete;
		};

		class CircleEmitter : public Emitter
		{
		public:
			explicit CircleEmitter(std::weak_ptr<ParticleSystemContainer> parent);
			explicit CircleEmitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);

			const ParameterPtr& getRadius() const { return circle_radius_; }
			void setRadius(variant node);
			float getStep() const { return circle_step_; }
			void setStep(float step) { circle_step_ = step; }
			float getAngle() const { return circle_angle_; }
			void setAngle(float angle) { circle_angle_ = angle; }
			bool isRandomLocation() const { return circle_random_; }
			void setRandomLocation(bool f) { circle_random_ = f; }
			const glm::vec3& getNormal() const { return normal_; }
			void setNormal(const glm::vec3& n) { normal_ = glm::normalize(n); }
			void setNormal(float x, float y, float z) { normal_ = glm::normalize(glm::vec3(x, y, z)); }
			void setNormal(float* v) { normal_ = glm::normalize(glm::vec3(v[0], v[1], v[2])); }
		private:
			void internalCreate(Particle& p, float t) override;
			virtual EmitterPtr clone() override {
				return std::make_shared<CircleEmitter>(*this);
			}
			void handleWrite(variant_builder* build) const override;
			ParameterPtr circle_radius_;
			float circle_step_;
			float circle_angle_;
			bool circle_random_;
			glm::vec3 normal_;

			CircleEmitter() = delete;
		};

		class BoxEmitter : public Emitter
		{
		public:
			explicit BoxEmitter(std::weak_ptr<ParticleSystemContainer> parent);
			explicit BoxEmitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			const glm::vec3& getDimensions() const { return box_dimensions_; }
			void setDimensions(const glm::vec3& d) { box_dimensions_ = d; }
			void setDimensions(float x, float y, float z) { box_dimensions_ = glm::vec3(x, y, z); }
			void setDimensions(float* v) { box_dimensions_ = glm::vec3(v[0], v[1], v[2]); }
		private:
			void internalCreate(Particle& p, float t) override;
			virtual EmitterPtr clone() override {
				return std::make_shared<BoxEmitter>(*this);
			}
			void handleWrite(variant_builder* build) const override;
			glm::vec3 box_dimensions_;
			BoxEmitter() = delete;
		};

		class LineEmitter : public Emitter
		{
		public:
			explicit LineEmitter(std::weak_ptr<ParticleSystemContainer> parent);
			explicit LineEmitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);

			float getLineDeviation() const { return line_deviation_; }
			void setLineDeviation(float d) { line_deviation_ = d; }
			float getMinIncrement() const { return min_increment_; }
			void setMinIncrement(float minc) { min_increment_ = minc; }
			float getMaxIncrement() const { return max_increment_; }
			void setMaxIncrement(float maxc) { max_increment_ = maxc; }
		private:
			void internalCreate(Particle& p, float t) override;
			EmitterPtr clone() override {
				return std::make_shared<LineEmitter>(*this);
			}
			void handleWrite(variant_builder* build) const override;
			glm::vec3 line_end_;
			float line_deviation_;
			float min_increment_;
			float max_increment_;

			LineEmitter() = delete;
		};

		class PointEmitter : public Emitter
		{
		public:
			explicit PointEmitter(std::weak_ptr<ParticleSystemContainer> parent);
			explicit PointEmitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
		private:
			void internalCreate(Particle& p, float t) override;
			EmitterPtr clone() override {
				return std::make_shared<PointEmitter>(*this);
			}
			void handleWrite(variant_builder* build) const override;
			PointEmitter();
		};

		class SphereSurfaceEmitter : public Emitter
		{
		public:
			explicit SphereSurfaceEmitter(std::weak_ptr<ParticleSystemContainer> parent);
			explicit SphereSurfaceEmitter(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);

			const ParameterPtr& getRadius() const { return radius_; }
		private:
			void internalCreate(Particle& p, float t) override;
			EmitterPtr clone() override {
				return std::make_shared<SphereSurfaceEmitter>(*this);
			}
			void handleWrite(variant_builder* build) const override;
			ParameterPtr radius_;
			SphereSurfaceEmitter() = delete;
		};
	}
}
