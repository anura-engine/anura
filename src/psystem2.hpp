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

#include <memory>
#include <random>

#include "formula_callable_definition.hpp"
#include "graphics.hpp"
#include "psystem2_fwd.hpp"
#include "raster.hpp"
#include "texture.hpp"
#include "widget.hpp"

namespace graphics
{
	namespace particles
	{
		typedef glm::detail::tvec4<unsigned char> color_vector;

		struct physics_parameters
		{
			glm::vec3 position;
			color_vector color;
			glm::vec3 dimensions;
			float time_to_live;
			float mass;
			float velocity;
			glm::vec3 direction;
			glm::quat orientation;
		};

		void init_physics_parameters(physics_parameters& pp); 

		// This structure should be POD (i.e. plain old data)
		struct particle
		{
			physics_parameters current;
			physics_parameters initial;
		};

		class material
		{
		public:
			material(const variant& node);
			virtual ~material();

			void apply() const;
			void unapply() const;
		private:
			std::string name_;
			std::vector<texture> tex_;
			bool use_lighting_;
			bool use_fog_;
			bool do_depth_write_;
			bool do_depth_check_;
			blend_mode blend_;

			material();
			material(const material&);
		};
		typedef std::shared_ptr<material> material_ptr;

		class technique 
		{
		public:
			technique(const variant& node, particle_system* ps, gles2::program_ptr shader);
			virtual ~technique();
			void draw(gles2::program_ptr shader) const;
			void process(float t);
			size_t particle_count() const { return active_particles_.size(); };
			int quota() const { return particle_quota_; }
			glm::vec3 default_dimensions() const { return glm::vec3(default_particle_width_, default_particle_height_, default_particle_depth_); }
			particle_system* get_particle_system() { return particle_system_; }
		private:
			float default_particle_width_;
			float default_particle_height_;
			float default_particle_depth_;
			int particle_quota_;
			int lod_index_;
			float velocity_;
			std::unique_ptr<float> max_velocity_;

			material_ptr material_;
			//renderer_ptr renderer_;
			std::vector<emitter_ptr> emitters_;
			std::vector<affector_ptr> affectors_;

			// Parent particle system
			particle_system* particle_system_;

			GLuint a_dimensions_;

			// List of particles currently active.
			std::vector<particle> active_particles_;

			technique();
			technique(const technique&);
		};

		class particle_system : public gui::widget
		{
		public:
			particle_system(const variant& node, game_logic::formula_callable* environment);
			virtual ~particle_system();

			float elapsed_time() const { return elapsed_time_; }
			float scale_velocity() const { return scale_velocity_; }
			float scale_time() const { return scale_time_; }
			const glm::vec3& scale_dimensions() const { return scale_dimensions_; }
		protected:
			virtual void handle_draw() const;
			virtual void handle_process();
		private:
			DECLARE_CALLABLE(particle_system);

			void update(float t);

			float elapsed_time_;
			float scale_velocity_;
			float scale_time_;
			glm::vec3 scale_dimensions_;

			std::unique_ptr<std::pair<float,float>> fast_forward_;

			// List of how to create and manipulate particles.
			std::vector<technique_ptr> techniques_;

			gles2::program_ptr shader_;

			particle_system();
			particle_system(const particle_system&);
		};
	}
}
