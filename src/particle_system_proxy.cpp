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

#include "ParticleSystem.hpp"
#include "SceneGraph.hpp"
#include "SceneNode.hpp"
#include "RenderManager.hpp"
#include "WindowManager.hpp"

namespace graphics
{
	using namespace KRE;

	ParticleSystemProxy::ParticleSystemProxy(const variant& node)
		: scene_(SceneGraph::create("ParticleSystemProxy")),
		  root_(scene_->getRootNode()),
		  rmanager_(),
		  particle_system_container_(),
		  running_(true)
	{
		root_->setNodeName("root_node");

		rmanager_ = std::make_shared<RenderManager>();
		rmanager_->addQueue(0, "PS");

		particle_system_container_ = Particles::ParticleSystemContainer::create(scene_, node);
		root_->attachNode(particle_system_container_);
	}

	void ParticleSystemProxy::draw(const WindowPtr& wnd) const
	{
		if(running_) {
			scene_->renderScene(rmanager_);
			rmanager_->render(wnd);
		}
	}

	void ParticleSystemProxy::process()
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

	void ParticleSystemProxy::surrenderReferences(GarbageCollector* collector)
	{
		
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(ParticleSystemProxy)
		DEFINE_FIELD(running, "bool")
			return variant::from_bool(obj.running_);
		DEFINE_SET_FIELD
			obj.running_ = value.as_bool();
	END_DEFINE_CALLABLE(ParticleSystemProxy)
}

