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

#include "psystem2_fwd.hpp"

namespace graphics
{
	namespace particles
	{
		class emitter
		{
		public:
			emitter(const variant& node, technique* tech);
			virtual ~emitter();

			void process(std::vector<particle>& particles, float t, std::vector<particle>::iterator& start, std::vector<particle>::iterator& end);

			int get_emitted_particle_count_per_cycle(float t);
			glm::detail::tvec4<unsigned char> get_color() const;
			const std::string& name() const { return name_; }

			static emitter_ptr factory(const variant& node, technique* tech);
		protected:
			virtual void handle_process(const std::vector<particle>::iterator& start, const std::vector<particle>::iterator& end, float t) = 0;
			//parameter_ptr time_to_live() const { return time_to_live_; }
			//parameter_ptr velocity() const { return velocity_; }
			//const glm::vec3& direction() const { return current_.direction; }
			//const glm::vec3& position() const { return current_.position; }
			//parameter_ptr mass() const { return mass_; }
			//parameter_ptr duration() const { return duration_; }
			//parameter_ptr repeat_delay() const { return repeat_delay_; }
			//const glm::detail::tvec4<unsigned char>& color() const { return color_; }
		private:
			technique* technique_;

			// these are parameters in effect of the emitter
			physics_parameters current_;
			physics_parameters initial_;

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
			parameter_ptr particle_width_;
			parameter_ptr particle_height_;
			parameter_ptr particle_depth_;
			std::string name_;

			void init_particle(particle& p, float t);
			void set_particle_starting_values(const std::vector<particle>::iterator start, const std::vector<particle>::iterator end);
			void create_particles(std::vector<particle>& particles, std::vector<particle>::iterator* start, std::vector<particle>::iterator* end, float t);

			float generate_angle() const;
			glm::vec3 get_initial_direction(const glm::vec3& up) const;

			// working items
			// Any "left over" fractional count of emitted particles
			float emission_fraction_;
			// time till the emitter stops emitting.
			float duration_remaining_;
			// time remaining till a stopped emitter restarts.
			float repeat_delay_remaining_;

			emitter();
			emitter(const emitter&);
		};
	}
}