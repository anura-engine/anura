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
		enum class EmitsType {
			VISUAL,
			EMITTER,
			AFFECTOR,
			TECHNIQUE,
			SYSTEM,
		};

		class Emitter : public EmitObject
		{
		public:
			explicit Emitter(ParticleSystemContainer* parent, const variant& node);
			virtual ~Emitter();
			Emitter(const Emitter&);

			int getEmittedParticleCountPerCycle(float t);
			color_vector getColor() const;
			Technique* getTechnique() { return technique_; }
			void setParentTechnique(Technique* tq) {
				technique_ = tq;
			}

			virtual Emitter* clone() = 0;
			static Emitter* factory(ParticleSystemContainer* parent, const variant& node);
		protected:
			virtual void internalCreate(Particle& p, float t) = 0;
			virtual bool durationExpired() { return can_be_deleted_; }
		private:
			virtual void handleEmitProcess(float t) override;
			virtual void handleDraw() const override;
			Technique* technique_;

			// These are generation parameters.
			ParameterPtr emission_rate_;
			ParameterPtr time_to_live_;
			ParameterPtr velocity_;
			ParameterPtr angle_;
			ParameterPtr mass_;
			// This is the duration that the emitter lives for
			ParameterPtr duration_;
			// this is the delay till the emitter repeats.
			ParameterPtr repeat_delay_;
			std::unique_ptr<std::pair<glm::quat, glm::quat>> orientation_range_;
			typedef std::pair<color_vector,color_vector> color_range;
			std::shared_ptr<color_range> color_range_;
			glm::vec4 color_;
			ParameterPtr particle_width_;
			ParameterPtr particle_height_;
			ParameterPtr particle_depth_;
			bool force_emission_;
			bool force_emission_processed_;
			bool can_be_deleted_;

			EmitsType emits_type_;
			std::string emits_name_;

			void initParticle(Particle& p, float t);
			void setParticleStartingValues(const std::vector<Particle>::iterator& start, const std::vector<Particle>::iterator& end);
			void createParticles(std::vector<Particle>& particles, std::vector<Particle>::iterator& start, std::vector<Particle>::iterator& end, float t);
			size_t calculateParticlesToEmit(float t, size_t quota, size_t current_size);

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

			Emitter();
		};
	}
}