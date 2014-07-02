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
#include <sstream>
#include <glm/glm.hpp>

#include "../asserts.hpp"
#include "AttributeSet.hpp"
#include "Material.hpp"
#include "ParticleSystemFwd.hpp"
#include "SceneNode.hpp"
#include "SceneObject.hpp"
#include "SceneUtil.hpp"

namespace KRE
{
	namespace Particles
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
			explicit emit_object(ParticleSystemContainer* parent, const variant& node) 
				: parent_container_(parent) {
				ASSERT_LOG(parent != NULL, "PSYSTEM2: parent is null");
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
				handleProcess(t);
			}
			void draw() const {
				handleDraw();
			}
			ParticleSystemContainer* parent_container() { 
				ASSERT_LOG(parent_container_ != NULL, "PSYSTEM2: parent container is NULL");
				return parent_container_; 
			}
		protected:
			virtual void handleProcess(float t) = 0;
			virtual void handleDraw() const {}
			virtual bool duration_expired() { return false; }
		private:
			std::string name_;
			ParticleSystemContainer* parent_container_;
			emit_object();
			//emit_object(const emit_object&);
		};

		class technique  : public emit_object, public SceneObject
		{
		public:
			explicit technique(ParticleSystemContainer* parent, const variant& node);
			technique(const technique& tq);
			virtual ~technique();

			size_t particle_count() const { return active_particles_.size(); };
			size_t quota() const { return particle_quota_; }
			size_t emitter_quota() const { return emitter_quota_; }
			size_t system_quota() const { return system_quota_; }
			size_t technique_quota() const { return technique_quota_; }
			size_t affector_quota() const { return affector_quota_; }
			glm::vec3 default_dimensions() const { return glm::vec3(default_particle_width_, default_particle_height_, default_particle_depth_); }
			ParticleSystem* get_ParticleSystem() { return ParticleSystem_; }
			emit_object_ptr get_object(const std::string& name);
			void setParent(ParticleSystem* parent);
			// Direct access here for *speed* reasons.
			std::vector<particle>& active_particles() { return active_particles_; }
			std::vector<emitter_ptr>& active_emitters() { return instanced_emitters_; }
			std::vector<affector_ptr>& active_affectors() { return instanced_affectors_; }
			void add_emitter(emitter_ptr e);
			void add_affector(affector_ptr a);
			void preRender() override;
		protected:
			DisplayDeviceDef Attach(const DisplayDevicePtr& dd);

			virtual void handleProcess(float t);
		private:
			void Init();

			std::shared_ptr<Attribute<vertex_texture_color>> arv_;

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

			//renderer_ptr renderer_;
			std::vector<emitter_ptr> active_emitters_;
			std::vector<affector_ptr> active_affectors_;

			std::vector<emitter_ptr> instanced_emitters_;
			std::vector<affector_ptr> instanced_affectors_;

			// Parent particle system
			ParticleSystem* ParticleSystem_;

			// List of particles currently active.
			std::vector<particle> active_particles_;

			technique();
		};

		class ParticleSystem : public emit_object, public SceneNode
		{
		public:
			explicit ParticleSystem(SceneGraph* sg, ParticleSystemContainer* parent, const variant& node);
			ParticleSystem(const ParticleSystem& ps);
			virtual ~ParticleSystem();

			float elapsed_time() const { return elapsed_time_; }
			float scale_velocity() const { return scale_velocity_; }
			float scale_time() const { return scale_time_; }
			const glm::vec3& scale_dimensions() const { return scale_dimensions_; }

			static ParticleSystem* factory(ParticleSystemContainer* parent, const variant& node);

			void add_technique(technique_ptr tq);
			std::vector<technique_ptr>& active_techniques() { return active_techniques_; }

			void NodeAttached() override;
		protected:
			virtual void handleProcess(float t);
		private:
			void update(float t);

			float elapsed_time_;
			float scale_velocity_;
			float scale_time_;
			glm::vec3 scale_dimensions_;

			std::unique_ptr<std::pair<float,float>> fast_forward_;

			// List of how to create and manipulate particles.
			std::vector<technique_ptr> active_techniques_;

			ParticleSystem();
		};

		class ParticleSystemContainer : public SceneNode
		{
		public:
			explicit ParticleSystemContainer(SceneGraph* sg, const variant& node);
			virtual ~ParticleSystemContainer();

			void activate_ParticleSystem(const std::string& name);
			std::vector<ParticleSystemPtr>& active_ParticleSystems() { return active_ParticleSystems_; }

			ParticleSystemPtr clone_ParticleSystem(const std::string& name);
			technique_ptr clone_technique(const std::string& name);
			emitter_ptr clone_emitter(const std::string& name);
			affector_ptr clone_affector(const std::string& name);

			void addParticleSystem(ParticleSystem* obj);
			void add_technique(technique* obj);
			void add_emitter(emitter* obj);
			void add_affector(affector* obj);

			std::vector<ParticleSystemPtr> clone_ParticleSystems();
			std::vector<technique_ptr> clone_techniques();
			std::vector<emitter_ptr> clone_emitters();
			std::vector<affector_ptr> clone_affectors();

			void Process(double current_time) override;
			void NodeAttached() override;
		private:
			std::vector<ParticleSystemPtr> active_ParticleSystems_;

			std::vector<ParticleSystemPtr> ParticleSystems_;
			std::vector<technique_ptr> techniques_;
			std::vector<emitter_ptr> emitters_;
			std::vector<affector_ptr> affectors_;
			
			ParticleSystemContainer();
			ParticleSystemContainer(const ParticleSystemContainer&);
		};

		std::ostream& operator<<(std::ostream& os, const glm::vec3& v);
		std::ostream& operator<<(std::ostream& os, const glm::vec4& v);
		std::ostream& operator<<(std::ostream& os, const glm::quat& v);

		glm::vec3 perpendicular(const glm::vec3& v) ;
		glm::vec3 create_deviating_vector(float angle, const glm::vec3& v, const glm::vec3& up = glm::vec3(0.0f));
	}
}
