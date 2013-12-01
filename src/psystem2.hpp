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

namespace shader
{
	struct manager
	{
		manager(gles2::program_ptr shader) 
		{
			glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);
			glUseProgram(shader->get());
		}
		~manager()
		{
			glUseProgram(old_program);
		}
		GLint old_program;
	};
}

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
			emitter* emitted_by;
		};

		// General class for emitter objects which encapsulate and exposes physical parameters
		// Used as a base class for everything that is not 
		class emit_object : public particle
		{
		public:
			explicit emit_object(particle_system_container* parent, const variant& node) 
				: parent_container_(parent) {
				ASSERT_LOG(parent != NULL, "FATAL: PSYSTEM2: parent is null");
				if(node.has_key("name")) {
					name_ = node["name"].as_string();
				} else {
					std::stringstream ss;
					ss << "emit_object_" << int(get_random_float());
					name_ = ss.str();
				}
			}
			virtual ~emit_object() {}
			const std::string& name() const { return name_; }
			void process(float t) {
				handle_process(t);
			}
			void draw() const {
				handle_draw();
			}
			particle_system_container* parent_container() { 
				ASSERT_LOG(parent_container_ != NULL, "FATAL: PSYSTEM2: parent container is NULL");
				return parent_container_; 
			}
		protected:
			virtual void handle_process(float t) = 0;
			virtual void handle_draw() const {}
			virtual bool duration_expired() { return false; }
		private:
			std::string name_;
			particle_system_container* parent_container_;
			emit_object();
			//emit_object(const emit_object&);
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

		class technique  : public emit_object
		{
		public:
			explicit technique(particle_system_container* parent, const variant& node);
			technique(const technique& tq);
			virtual ~technique();

			size_t particle_count() const { return active_particles_.size(); };
			size_t quota() const { return particle_quota_; }
			size_t emitter_quota() const { return emitter_quota_; }
			size_t system_quota() const { return system_quota_; }
			size_t technique_quota() const { return technique_quota_; }
			size_t affector_quota() const { return affector_quota_; }
			glm::vec3 default_dimensions() const { return glm::vec3(default_particle_width_, default_particle_height_, default_particle_depth_); }
			particle_system* get_particle_system() { return particle_system_; }
			emit_object_ptr get_object(const std::string& name);
			void set_parent(particle_system* parent);
			void set_shader(gles2::program_ptr shader);
			// Direct access here for *speed* reasons.
			std::vector<particle>& active_particles() { return active_particles_; }
			std::vector<emitter_ptr>& active_emitters() { return instanced_emitters_; }
			std::vector<affector_ptr>& active_affectors() { return instanced_affectors_; }
			void add_emitter(emitter_ptr e);
			void add_affector(affector_ptr a);
		protected:
			virtual void handle_process(float t);
			virtual void handle_draw() const;
		private:
			float default_particle_width_;
			float default_particle_height_;
			float default_particle_depth_;
			size_t particle_quota_;
			size_t emitter_quota_;
			size_t affector_quota_;
			size_t technique_quota_;
			size_t system_quota_;
			int lod_index_;
			float velocity_;
			std::unique_ptr<float> max_velocity_;

			material_ptr material_;
			//renderer_ptr renderer_;
			std::vector<emitter_ptr> active_emitters_;
			std::vector<affector_ptr> active_affectors_;

			std::vector<emitter_ptr> instanced_emitters_;
			std::vector<affector_ptr> instanced_affectors_;

			// Parent particle system
			particle_system* particle_system_;

			GLuint a_dimensions_;

			gles2::program_ptr shader_;

			// List of particles currently active.
			std::vector<particle> active_particles_;

			technique();
		};

		class particle_system : public emit_object
		{
		public:
			explicit particle_system(particle_system_container* parent, const variant& node);
			particle_system(const particle_system& ps);
			virtual ~particle_system();

			float elapsed_time() const { return elapsed_time_; }
			float scale_velocity() const { return scale_velocity_; }
			float scale_time() const { return scale_time_; }
			const glm::vec3& scale_dimensions() const { return scale_dimensions_; }

			static particle_system* factory(particle_system_container* parent, const variant& node);

			void add_technique(technique_ptr tq);
			std::vector<technique_ptr>& active_techniques() { return active_techniques_; }
		protected:
			virtual void handle_draw() const;
			virtual void handle_process(float t);
		private:
			void update(float t);

			float elapsed_time_;
			float scale_velocity_;
			float scale_time_;
			glm::vec3 scale_dimensions_;

			std::unique_ptr<std::pair<float,float>> fast_forward_;

			// List of how to create and manipulate particles.
			std::vector<technique_ptr> active_techniques_;

			gles2::program_ptr shader_;

			particle_system();
		};

		class particle_system_container : public game_logic::formula_callable
		{
		public:
			explicit particle_system_container(const variant& node);
			virtual ~particle_system_container();

			void activate_particle_system(const std::string& name);
			std::vector<particle_system_ptr>& active_particle_systems() { return active_particle_systems_; }

			particle_system_ptr clone_particle_system(const std::string& name);
			technique_ptr clone_technique(const std::string& name);
			emitter_ptr clone_emitter(const std::string& name);
			affector_ptr clone_affector(const std::string& name);

			void add_particle_system(particle_system* obj);
			void add_technique(technique* obj);
			void add_emitter(emitter* obj);
			void add_affector(affector* obj);

			std::vector<particle_system_ptr> clone_particle_systems();
			std::vector<technique_ptr> clone_techniques();
			std::vector<emitter_ptr> clone_emitters();
			std::vector<affector_ptr> clone_affectors();

			void draw() const;
			void process();
		private:
			DECLARE_CALLABLE(particle_system_container);
			std::vector<particle_system_ptr> active_particle_systems_;

			std::vector<particle_system_ptr> particle_systems_;
			std::vector<technique_ptr> techniques_;
			std::vector<emitter_ptr> emitters_;
			std::vector<affector_ptr> affectors_;
			
			particle_system_container();
			particle_system_container(const particle_system_container&);
		};

		class particle_system_widget : public gui::widget
		{
		public:
			particle_system_widget(const variant& node, game_logic::formula_callable* environment);
			virtual ~particle_system_widget();
		protected:
			virtual void handle_draw() const;
			virtual void handle_process();
		private:
			DECLARE_CALLABLE(particle_system_widget);
			particle_system_container particle_systems_;
			particle_system_widget();
			particle_system_widget(const particle_system_widget&);
		};
	}
}
