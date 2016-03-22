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

#include <memory>
#include <random>
#include <sstream>
#include <glm/glm.hpp>
#include <glm/vec4.hpp>

#include "asserts.hpp"
#include "AttributeSet.hpp"
#include "ParticleSystemFwd.hpp"
#include "SceneNode.hpp"
#include "SceneObject.hpp"
#include "SceneUtil.hpp"
#include "Texture.hpp"

namespace KRE
{
	namespace Particles
	{
		typedef glm::tvec4<unsigned char> color_vector;

		struct vertex_texture_color3
		{
			vertex_texture_color3(const glm::vec3& v, const glm::vec2& t, const glm::u8vec4& c)
				: vertex(v), texcoord(t), color(c) {}
			glm::vec3 vertex;
			glm::vec2 texcoord;
			glm::u8vec4 color;
		};

		struct vertex_color3
		{
			vertex_color3(const glm::vec3& v, const glm::u8vec4& c) : vertex(v), color(c) {}
			glm::vec3 vertex;
			glm::u8vec4 color;
		};

		struct PhysicsParameters
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

		void init_physics_parameters(PhysicsParameters& pp); 

		// This structure should be POD (i.e. plain old data)
		struct Particle
		{
			Particle() : emitted_by(nullptr) {}
			PhysicsParameters current;
			PhysicsParameters initial;
			// Still wavering over whether this should be std::weak_ptr<Emitter>
			Emitter* emitted_by;
		};

		// General class for emitter objects which encapsulate and exposes physical parameters
		// Used as a base class for everything that is not 
		class EmitObject : public Particle
		{
		public:
			explicit EmitObject(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			virtual ~EmitObject() {}
			const std::string& name() const { return name_; }
			void emitProcess(float t) {
				handleEmitProcess(t);
			}
			void draw(const WindowPtr& wnd) const {
				handleDraw(wnd);
			}
			ParticleSystemContainerPtr getParentContainer() const;
			virtual const glm::vec3& getPosition() const;
			virtual void setPosition(const glm::vec3& pos) {}
			bool isEnabled() const { return enabled_; }
			void enable(bool en) { enabled_ = en; handleEnable(); }
			bool doDebugDraw() const { return do_debug_draw_; }
		protected:
			virtual bool durationExpired() { return false; }
		private:
			virtual void handleEmitProcess(float t) = 0;
			virtual void handleDraw(const WindowPtr& wnd) const {}
			virtual void handleEnable() {}
			std::string name_;
			bool enabled_;
			bool do_debug_draw_;
			std::weak_ptr<ParticleSystemContainer> parent_container_;

			EmitObject();
		};

		class Technique  : public EmitObject, public SceneObject, public std::enable_shared_from_this<Technique>
		{
		public:
			explicit Technique(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			Technique(const Technique& tq);

			int getParticleCount() const { return active_particles_.size(); };
			int getQuota() const { return particle_quota_; }
			int getEmitterQuota() const { return emitter_quota_; }
			int getSystemQuota() const { return system_quota_; }
			int getTechniqueQuota() const { return technique_quota_; }
			int getAffectorQuota() const { return affector_quota_; }
			glm::vec3 getDefaultDimensions() const { return glm::vec3(default_particle_width_, default_particle_height_, default_particle_depth_); }
			ParticleSystemPtr getParticleSystem() const;
			EmitObjectPtr getEmitObject(const std::string& name);
			void setParent(std::weak_ptr<ParticleSystem> parent);
			// Direct access here for *speed* reasons.
			std::vector<Particle>& getActiveParticles() { return active_particles_; }
			std::vector<EmitterPtr>& getActiveEmitters() { return active_emitters_; }
			std::vector<AffectorPtr>& getActiveAffectors() { return active_affectors_; }
			void preRender(const WindowPtr& wnd) override;
			void postRender(const WindowPtr& wnd) override;

			static TechniquePtr create(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			TechniquePtr clone() const;
		private:
			void init(const variant& node);
			void initAttributes();
			void handleEmitProcess(float t) override;

			std::shared_ptr<Attribute<vertex_texture_color3>> arv_;

			float default_particle_width_;
			float default_particle_height_;
			float default_particle_depth_;
			int lod_index_;

			int particle_quota_;
			int emitter_quota_;
			int affector_quota_;
			int technique_quota_;
			int system_quota_;
			float velocity_;
			std::unique_ptr<float> max_velocity_;

			//renderer_ptr renderer_;
			std::vector<EmitterPtr> active_emitters_;
			std::vector<AffectorPtr> active_affectors_;

			std::vector<EmitterPtr> child_emitters_;
			std::vector<AffectorPtr> child_affectors_;

			// List of particles currently active.
			std::vector<Particle> active_particles_;

			// Parent particle system
			std::weak_ptr<ParticleSystem> parent_particle_system_;

			Technique() = delete;
		};

		class ParticleSystem : public EmitObject, public SceneNode
		{
		public:
			explicit ParticleSystem(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			ParticleSystem(const ParticleSystem& ps);

			ParticleSystemPtr get_this_ptr();

			float getElapsedTime() const { return elapsed_time_; }
			float getScaleVelocity() const { return scale_velocity_; }
			float getScaleTime() const { return scale_time_; }
			const glm::vec3& getScaleDimensions() const { return scale_dimensions_; }

			void addTechnique(TechniquePtr tq);
			std::vector<TechniquePtr>& getActiveTechniques() { return active_techniques_; }

			static ParticleSystemPtr factory(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			ParticleSystemPtr clone() const;

			void fastForward();
		private:
			void init(const variant& node);
			void notifyNodeAttached(std::weak_ptr<SceneNode> parent) override;
			virtual void handleEmitProcess(float t) override;
			void update(float t);

			float elapsed_time_;
			float scale_velocity_;
			float scale_time_;
			glm::vec3 scale_dimensions_;

			std::unique_ptr<std::pair<float,float>> fast_forward_;

			// List of how to create and manipulate particles.
			std::vector<TechniquePtr> active_techniques_;

			ParticleSystem();
		};

		class ParticleSystemContainer : public SceneNode
		{
		public:
			// Must be called after being created.
			explicit ParticleSystemContainer(std::weak_ptr<SceneGraph> sg, const variant& node);
			void init(const variant& node);
			ParticleSystemContainerPtr get_this_ptr();

			void getActivateParticleSystem(const std::string& name);
			std::vector<ParticleSystemPtr>& getActiveParticleSystems() { return active_particle_systems_; }
			const std::vector<ParticleSystemPtr>& getAllParticleSystems() const { return particle_systems_; }
			const std::vector<TechniquePtr>& getTechniques() const { return techniques_; }
			const std::vector<EmitterPtr>& getEmitters() const { return emitters_; }
			const std::vector<AffectorPtr>& getAffectors() const { return affectors_; }

			ParticleSystemPtr cloneParticleSystem(const std::string& name);
			TechniquePtr cloneTechnique(const std::string& name);
			EmitterPtr cloneEmitter(const std::string& name);
			AffectorPtr cloneAffector(const std::string& name);

			void addParticleSystem(ParticleSystemPtr obj);
			void addTechnique(TechniquePtr obj);
			void addEmitter(EmitterPtr obj);
			void addAffector(AffectorPtr obj);

			std::vector<ParticleSystemPtr> cloneParticleSystems();
			std::vector<TechniquePtr> cloneTechniques();
			std::vector<EmitterPtr> cloneEmitters();
			std::vector<AffectorPtr> cloneAffectors();

			void process(float delta_time) override;

			static ParticleSystemContainerPtr create(std::weak_ptr<SceneGraph> sg, const variant& node);
		private:
			void notifyNodeAttached(std::weak_ptr<SceneNode> parent) override;

			std::vector<ParticleSystemPtr> active_particle_systems_;

			std::vector<ParticleSystemPtr> particle_systems_;
			std::vector<TechniquePtr> techniques_;
			std::vector<EmitterPtr> emitters_;
			std::vector<AffectorPtr> affectors_;
			
			ParticleSystemContainer();
			ParticleSystemContainer(const ParticleSystemContainer&);
		};

		std::ostream& operator<<(std::ostream& os, const glm::vec3& v);
		std::ostream& operator<<(std::ostream& os, const glm::vec4& v);
		std::ostream& operator<<(std::ostream& os, const glm::quat& v);

		glm::vec3 perpendicular(const glm::vec3& v) ;
		glm::vec3 create_deviating_vector(float angle, const glm::vec3& v, const glm::vec3& up = glm::vec3(0.0f));
		
		class DebugDrawHelper : public SceneObject
		{
		public:
			DebugDrawHelper();
			void update(const glm::vec3& p1, const glm::vec3& p2, const Color& color);
		private:
			std::shared_ptr<Attribute<vertex_color3>> attrs_;
		};
	}
}
