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

#pragma once

#include "ParticleSystemFwd.hpp"

namespace KRE
{
	namespace Particles
	{

		class emitter : public emit_object
		{
		public:
			explicit emitter(ParticleSystemContainer* parent, const variant& node);
			virtual ~emitter();
			emitter(const emitter&);

			int get_emitted_particle_count_per_cycle(float t);
			color_vector get_color() const;
			technique* get_technique() { return technique_; }
			void set_parent_technique(technique* tq) {
				technique_ = tq;
			}

			virtual emitter* clone() = 0;
			static emitter* factory(ParticleSystemContainer* parent, const variant& node);
		protected:
			virtual void internal_create(particle& p, float t) = 0;
			virtual void handle_process(float t);
			virtual void handle_draw() const;
			virtual bool duration_expired() { return can_be_deleted_; }

			enum EMITS_TYPE {
				EMITS_VISUAL,
				EMITS_EMITTER,
				EMITS_AFFECTOR,
				EMITS_TECHNIQUE,
				EMITS_SYSTEM,
			};
		private:
			technique* technique_;

			// These are generation parameters.
			parameter_ptr emission_rate_;
			parameter_ptr time_to_live_;
			parameter_ptr velocity_;
			parameter_ptr angle_;
			parameter_ptr mass_;
			// This is the duration that the emitter lives for
			parameter_ptr duration_;
			// this is the delay till the emitter repeats.
			parameter_ptr repeat_delay_;
			std::unique_ptr<std::pair<glm::quat, glm::quat>> orientation_range_;
			typedef std::pair<color_vector,color_vector> color_range;
			std::shared_ptr<color_range> color_range_;
			glm::vec4 color_;
			parameter_ptr particle_width_;
			parameter_ptr particle_height_;
			parameter_ptr particle_depth_;
			bool force_emission_;
			bool force_emission_processed_;
			bool can_be_deleted_;

			EMITS_TYPE emits_type_;
			std::string emits_name_;

			void init_particle(particle& p, float t);
			void set_particle_starting_values(const std::vector<particle>::iterator& start, const std::vector<particle>::iterator& end);
			void create_particles(std::vector<particle>& particles, std::vector<particle>::iterator& start, std::vector<particle>::iterator& end, float t);
			size_t calculate_particles_to_emit(float t, size_t quota, size_t current_size);

			float generate_angle() const;
			glm::vec3 get_initial_direction() const;

			//BoxOutlinePtr debug_draw_outline_;

			// working items
			// Any "left over" fractional count of emitted particles
			float emission_fraction_;
			// time till the emitter stops emitting.
			float duration_remaining_;
			// time remaining till a stopped emitter restarts.
			float repeat_delay_remaining_;

			emitter();
		};
	}
}