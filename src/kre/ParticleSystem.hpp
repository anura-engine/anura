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

		struct particle_s
		{
			particle_s(const glm::vec3& v, const glm::vec3& ctr, const glm::vec4& qr, const glm::vec3& s, const glm::vec2& t, const glm::u8vec4& c)
				: vertex(v), center(ctr), q(qr), scale(s), texcoord(t), color(c) {}
			glm::vec3 vertex;
			glm::vec3 center;
			glm::vec4 q;
			glm::vec3 scale;
			glm::vec2 texcoord;
			glm::u8vec4 color;
		};

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
			// for animation of the texture co-ordinates.
			rectf area;
		};

		void init_physics_parameters(PhysicsParameters& pp); 

		// This structure should be POD (i.e. plain old data)
		struct Particle
		{
			Particle() : emitted_by(nullptr), init_pos(false) {}
			PhysicsParameters current;
			PhysicsParameters initial;
			// Still wavering over whether this should be std::weak_ptr<Emitter>
			Emitter* emitted_by;
			bool init_pos;
		};

		// General class for emitter objects which encapsulate and exposes physical parameters
		// Used as a base class for everything that is not 
		class EmitObject : public Particle
		{
		public:
			explicit EmitObject(std::weak_ptr<ParticleSystemContainer> parent);
			explicit EmitObject(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			virtual ~EmitObject() {}
			const std::string& getName() const { return name_; }
			void emitProcess(float t) {
				if(isEnabled()) {
					handleEmitProcess(t);
				}
			}
			void draw(const WindowPtr& wnd) const {
				handleDraw(wnd);
			}
			ParticleSystemContainerPtr getParentContainer() const;
			bool isEnabled() const { return enabled_; }
			void setEnable(bool en) { enabled_ = en; handleEnable(); }
			bool doDebugDraw() const { return do_debug_draw_; }

			void setDebugDraw(bool f) { do_debug_draw_ = f; }
			void setName(const std::string& name) { name_ = name; }
			variant write() const;
		protected:
			virtual bool durationExpired() { return false; }
		private:
			virtual void handleEmitProcess(float t) = 0;
			virtual void handleDraw(const WindowPtr& wnd) const {}
			virtual void handleEnable() {}
			virtual void handleWrite(variant_builder* build) const = 0;
			std::string name_;
			bool enabled_;
			bool do_debug_draw_;
			std::weak_ptr<ParticleSystemContainer> parent_container_;

			EmitObject() = delete;
		};

		class ParticleSystem : public EmitObject, public SceneObject
		{
		public:
			struct TranslationScope {
				explicit TranslationScope(const glm::vec3& v);
				~TranslationScope();
			};

			explicit ParticleSystem(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			ParticleSystem(const ParticleSystem& ps);
			void init();

			void setTextureNode(const variant& node);

			const EmitterPtr& getEmitter() const { return emitter_; }
			void setEmitter(const EmitterPtr& e) { emitter_ = e; init(); }
			const EmitterPtr& getActiveEmitter() const { return active_emitter_; }
			std::vector<AffectorPtr>& getAffectors() { return affectors_; }
			std::vector<Particle>& getActiveParticles() { return active_particles_; }

			int getParticleCount() const { return active_particles_.size(); };
			int getParticleQuota() const { return particle_quota_; }
			glm::vec3 getDefaultDimensions() const { return glm::vec3(default_particle_width_, default_particle_height_, default_particle_depth_); }

			float getElapsedTime() const { return elapsed_time_; }
			float getScaleVelocity() const { return scale_velocity_; }
			float getScaleTime() const { return scale_time_; }
			const glm::vec3& getScaleDimensions() const { return scale_dimensions_; }

			void setScaleVelocity(float sv) { scale_velocity_ = sv; }
			void setScaleTime(float st) { scale_time_ = st; }
			void setScaleDimensions(const glm::vec3& dim) { scale_dimensions_ = dim; }
			void setScaleDimensions(float x, float y, float z) { scale_dimensions_ = glm::vec3(x, y, z); }
			void setScaleDimensions(float* dim) { scale_dimensions_ = glm::vec3(dim[0], dim[1], dim[2]); }

			void setDefaultWidth(float w) { default_particle_width_ = w; }
			void setDefaultHeight(float h) { default_particle_height_ = h; }
			void setDefaultDepth(float d) { default_particle_depth_ = d; }

			void setParticleQuota(int q) { particle_quota_ = q; }

			bool hasMaxVelocity() const { return max_velocity_ != nullptr; }
			float getMaxVelocity() const { return *max_velocity_; }
			void setMaxVelocity(float mv) { max_velocity_.reset(new float(mv)); }
			void clearMaxVelocity() { max_velocity_.reset(); }

			bool useParticleSystemPosition() const { return use_position_; }
			void setUsePosition(bool f = true) { use_position_ = f; }

			static ParticleSystemPtr factory(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void initAttributes();

			void preRender(const WindowPtr& wnd) override;
			void postRender(const WindowPtr& wnd) override;

			void fastForward();

			std::pair<float,float> getFastForward() const;
			void setFastForward(const std::pair<float,float>& p);
		private:
			virtual void handleEmitProcess(float t) override;
			void update(float t);
			void handleWrite(variant_builder* build) const override;

			std::shared_ptr<Attribute<particle_s>> arv_;

			float elapsed_time_;
			float scale_velocity_;
			float scale_time_;
			glm::vec3 scale_dimensions_;

			std::unique_ptr<float> max_velocity_;

			float default_particle_width_;
			float default_particle_height_;
			float default_particle_depth_;

			int particle_quota_;

			std::unique_ptr<std::pair<float,float>> fast_forward_;

			// List of particles currently active.
			std::vector<Particle> active_particles_;
			EmitterPtr active_emitter_;

			EmitterPtr emitter_;
			std::vector<AffectorPtr> affectors_;

			variant texture_node_;
			bool use_position_;

			ParticleSystem() = delete;
		};

		class ParticleSystemContainer : public SceneNode
		{
		public:
			// Must be called after being created.
			explicit ParticleSystemContainer(std::weak_ptr<SceneGraph> sg, const variant& node);
			void init(const variant& node);
			ParticleSystemContainerPtr get_this_ptr();

			const ParticleSystemPtr& getParticleSystem() const { return particle_system_; }

			void process(float delta_time) override;

			variant write() const;

			static ParticleSystemContainerPtr create(std::weak_ptr<SceneGraph> sg, const variant& node);
		private:
			void notifyNodeAttached(std::weak_ptr<SceneNode> parent) override;

			ParticleSystemPtr particle_system_;
			
			ParticleSystemContainer() = delete;
			ParticleSystemContainer(ParticleSystemContainer const&) = delete;
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

		void convert_quat_to_axis_angle(const glm::quat& q, float* angle, glm::vec3* axis);
	}
}
