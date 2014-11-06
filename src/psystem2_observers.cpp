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
     in a product, an acknowledgement in the product documentation would be
     appreciated but is not required.

     2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.

     3. This notice may not be removed or altered from any source
     distribution.
*/

#include "psystem2_observers.hpp"

namespace graphics
{
	namespace particles
	{
		namespace 
		{
			class clear_event_handler : public event_handler
			{
			public:
				clear_event_handler(const variant& node) : event_handler(node), seen_particles_(false) {
				}
				event_handler* clone() override {
					return new clear_event_handler(*this);
				}
			private:
				bool handle_process(technique* tech, float t) override {
					ASSERT_LOG(tech != nullptr, "technique was null pointer.");
					if(!seen_particles_) {
						if(tech->active_particles().size() > 0) {
							seen_particles_ = true;
						}
					} else {
						// no particles left
						if(tech->active_particles().size() == 0) {
							return true;
						}
					}
					return false;
				}
				bool seen_particles_;
			};
		}

		event_handler::event_handler(const variant& node)
			: name_(node["name"].as_string_default()),
			  enabled_(node["enabled"].as_bool(true)),
			  observe_till_event_(node["observe_till_event"].as_bool(false)),
			  actions_executed_(false)
		{
		}

		event_handler::~event_handler()
		{
		}

		void event_handler::process(technique* tech, float t)
		{
			if(enabled_) {
				if(observe_till_event_ && actions_executed_) {
					return;
				}
				if(handle_process(tech, t)) {
					process_actions(tech, t);
				}
			}
		}

		void event_handler::add_action(action_ptr evt)
		{
			actions_.emplace_back(evt);
		}

		void event_handler::process_actions(technique* tech, float t)
		{
			for(auto a : actions_) {
				a->execute(tech, t);
			}
			actions_executed_ = true;
		}

		event_handler_ptr event_handler::create(const variant& node)
		{
			const std::string& type = node["type"].as_string();
			if(type == "on_clear") {
				return std::make_shared<clear_event_handler>(node);
			}

			ASSERT_LOG(false, "No handler found of type: " << type);
			return event_handler_ptr();
		}

		action::action(const variant& node)
			: name_(node["name"].as_string_default())
		{
			// XXX
		}

		action::~action()
		{
		}
		
		void action::execute(technique* tech, float t)
		{
			// XXX
		}

		action_ptr action::create(const variant& node)
		{
			const std::string& type = node["type"].as_string();
			// XXX
			//if(type == "stop_system") {
			//	return std::make_shared<stop_system_action>(node);
			//}
			ASSERT_LOG(false, "No handler found of type: " << type);
			return action_ptr();
		}
	}
}
