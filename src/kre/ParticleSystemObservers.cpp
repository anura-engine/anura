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

#include "ParticleSystemObservers.hpp"
#include "ParticleSystem.hpp"

namespace KRE
{
	namespace Particles
	{
		namespace 
		{
			class ClearEventHandler : public EventHandler
			{
			public:
				ClearEventHandler(const variant& node) 
					: EventHandler(node), 
					  seen_particles_(false) 
				{
				}
				EventHandlerPtr clone() const override {
					return std::make_shared<ClearEventHandler>(*this);
				}
			private:
				bool handleProcess(TechniquePtr tech, float t) override {
					ASSERT_LOG(tech != nullptr, "technique was null pointer.");
					if(!seen_particles_) {
						if(tech->getActiveParticles().size() > 0) {
							seen_particles_ = true;
						}
					} else {
						// no particles left
						if(tech->getActiveParticles().size() == 0) {
							return true;
						}
					}
					return false;
				}
				bool seen_particles_;
			};
		}

		EventHandler::EventHandler(const variant& node)
			: name_(node["name"].as_string()),
			  enabled_(node["enabled"].as_bool(true)),
			  observe_till_event_(node["observe_till_event"].as_bool(false)),
			  actions_executed_(false)
		{
		}

		EventHandler::~EventHandler()
		{
		}

		void EventHandler::process(TechniquePtr tech, float t)
		{
			if(enabled_) {
				if(observe_till_event_ && actions_executed_) {
					return;
				}
				if(handleProcess(tech, t)) {
					processActions(tech, t);
				}
			}
		}

		void EventHandler::addAction(ActionPtr evt)
		{
			actions_.emplace_back(evt);
		}

		void EventHandler::processActions(TechniquePtr tech, float t)
		{
			for(auto a : actions_) {
				a->execute(tech, t);
			}
			actions_executed_ = true;
		}

		EventHandlerPtr EventHandler::create(const variant& node)
		{
			const std::string& type = node["type"].as_string();
			if(type == "on_clear") {
				return std::make_shared<ClearEventHandler>(node);
			}

			ASSERT_LOG(false, "No handler found of type: " << type);
			return EventHandlerPtr();
		}

		Action::Action(const variant& node)
			: name_(node["name"].as_string())
		{
			// XXX
		}

		Action::~Action()
		{
		}
		
		void Action::execute(TechniquePtr tech, float t)
		{
			// XXX
		}

		ActionPtr Action::create(const variant& node)
		{
			const std::string& type = node["type"].as_string();
			// XXX
			//if(type == "stop_system") {
			//	return std::make_shared<StopSystemAction>(node);
			//}
			ASSERT_LOG(false, "No handler found of type: " << type);
			return ActionPtr();
		}
	}
}
