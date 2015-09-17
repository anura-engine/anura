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

#include "particle_system_proxy.hpp"
#include "profile_timer.hpp"

#include "formula_callable.hpp"
#include "ParticleSystem.hpp"
#include "SceneGraph.hpp"
#include "SceneNode.hpp"
#include "RenderManager.hpp"
#include "WindowManager.hpp"

namespace graphics
{
	using namespace KRE;

	ParticleSystemContainerProxy::ParticleSystemContainerProxy(const variant& node)
		: scene_(SceneGraph::create("ParticleSystemContainerProxy")),
		  root_(scene_->getRootNode()),
		  rmanager_(),
		  particle_system_container_(),
		  last_process_time_(-1),
		  running_(true)
	{
		root_->setNodeName("root_node");

		rmanager_ = std::make_shared<RenderManager>();
		rmanager_->addQueue(0, "PS");

		particle_system_container_ = Particles::ParticleSystemContainer::create(scene_, node);
		root_->attachNode(particle_system_container_);
	}

	void ParticleSystemContainerProxy::draw(const WindowPtr& wnd) const
	{
		if(running_) {
			scene_->renderScene(rmanager_);
			rmanager_->render(wnd);
		}
	}

	void ParticleSystemContainerProxy::process()
	{
		if(!running_) {
			last_process_time_ = profile::get_tick_time();
			return;
		}
		ASSERT_LOG(scene_ != nullptr, "Scene is empty");
		float delta_time = 0.0f;
		if(last_process_time_ == -1) {
			last_process_time_ = profile::get_tick_time();
		}
		auto current_time = profile::get_tick_time();
		delta_time = (current_time - last_process_time_) / 1000.0f;
		scene_->process(delta_time);
		last_process_time_ = current_time;
	}

	void ParticleSystemContainerProxy::surrenderReferences(GarbageCollector* collector)
	{
		
	}

	class ParticleSystemProxy : public game_logic::FormulaCallable
	{
	public:
		explicit ParticleSystemProxy(KRE::Particles::ParticleSystemPtr obj) : obj_(obj)
		{}
	private:
		DECLARE_CALLABLE(ParticleSystemProxy);
		KRE::Particles::ParticleSystemPtr obj_;
	};

	class ParticleTechniqueProxy : public game_logic::FormulaCallable
	{
	public:
		explicit ParticleTechniqueProxy(KRE::Particles::TechniquePtr obj) : obj_(obj)
		{}
	private:
		DECLARE_CALLABLE(ParticleTechniqueProxy);
		KRE::Particles::TechniquePtr obj_;
	};

	class ParticleEmitterProxy : public game_logic::FormulaCallable
	{
	public:
		explicit ParticleEmitterProxy(KRE::Particles::EmitterPtr obj) : obj_(obj)
		{}
	private:
		DECLARE_CALLABLE(ParticleEmitterProxy);
		KRE::Particles::EmitterPtr obj_;
	};

	class ParticleAffectorProxy : public game_logic::FormulaCallable
	{
	public:
		explicit ParticleAffectorProxy(KRE::Particles::AffectorPtr obj) : obj_(obj)
		{}
	private:
		DECLARE_CALLABLE(ParticleAffectorProxy);
		KRE::Particles::AffectorPtr obj_;
	};

	BEGIN_DEFINE_CALLABLE_NOBASE(ParticleSystemContainerProxy)
		DEFINE_FIELD(running, "bool")
			return variant::from_bool(obj.running_);
		DEFINE_SET_FIELD
			obj.running_ = value.as_bool();
		DEFINE_FIELD(systems, "[builtin particle_system_proxy]")

			auto v = obj.particle_system_container_->getActiveParticleSystems();
			std::vector<variant> result;
			result.reserve(v.size());
			for(auto p : v) {
				result.push_back(variant(new ParticleSystemProxy(p)));
			}

			return variant(&result);

		DEFINE_FIELD(techniques, "[builtin particle_technique_proxy]")

			auto v = obj.particle_system_container_->getTechniques();
			std::vector<variant> result;
			result.reserve(v.size());
			for(auto p : v) {
				result.push_back(variant(new ParticleTechniqueProxy(p)));
			}

			return variant(&result);

		DEFINE_FIELD(emitters, "[builtin particle_emitter_proxy]")

			auto v = obj.particle_system_container_->getEmitters();
			std::vector<variant> result;
			result.reserve(v.size());
			for(auto p : v) {
				result.push_back(variant(new ParticleEmitterProxy(p)));
			}

			return variant(&result);

		DEFINE_FIELD(affectors, "[builtin particle_affector_proxy]")

			auto v = obj.particle_system_container_->getAffectors();
			std::vector<variant> result;
			result.reserve(v.size());
			for(auto p : v) {
				result.push_back(variant(new ParticleAffectorProxy(p)));
			}

			return variant(&result);


	END_DEFINE_CALLABLE(ParticleSystemContainerProxy)

	BEGIN_DEFINE_CALLABLE_NOBASE(ParticleSystemProxy)
	BEGIN_DEFINE_FN(add_technique, "(map) ->commands")
		variant arg = FN_ARG(0);
		return variant(new game_logic::FnCommandCallable([=]() {
			obj.obj_->addTechnique(KRE::Particles::TechniquePtr(new KRE::Particles::Technique(obj.obj_->getParentContainer(), arg)));
		}));
	END_DEFINE_FN
	END_DEFINE_CALLABLE(ParticleSystemProxy)

	BEGIN_DEFINE_CALLABLE_NOBASE(ParticleTechniqueProxy)
	END_DEFINE_CALLABLE(ParticleTechniqueProxy)


	BEGIN_DEFINE_CALLABLE_NOBASE(ParticleEmitterProxy)
	END_DEFINE_CALLABLE(ParticleEmitterProxy)

	BEGIN_DEFINE_CALLABLE_NOBASE(ParticleAffectorProxy)
	END_DEFINE_CALLABLE(ParticleAffectorProxy)

}

