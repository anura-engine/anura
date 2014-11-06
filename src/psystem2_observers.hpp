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

#pragma once

#include "psystem2.hpp"
#include "variant.hpp"

namespace graphics
{
	namespace particles
	{
		class action;
		typedef std::shared_ptr<action> action_ptr;

		class event_handler;
		typedef std::shared_ptr<event_handler> event_handler_ptr;

		class action
		{
		public:
			explicit action(const variant& node);
			virtual ~action();
			const std::string& name() const { return name_; }
			void execute(technique* tech, float t);
			virtual action* clone() = 0;
			static action_ptr create(const variant& node);
		private:
			std::string name_;
		};

		class event_handler
		{
		public:
			explicit event_handler(const variant& node);
			virtual ~event_handler();
			void process(technique* tech, float t);
			void process_actions(technique* tech, float t);
			void add_action(action_ptr evt);
			const std::string& name() const { return name_; }
			bool is_enabled() const { return enabled_; }
			void enable(bool en=true) { enabled_ = en; }
			void disable() { enabled_ = false; }
			virtual event_handler* clone() = 0;
			static event_handler_ptr create(const variant& node);
		private:
			virtual bool handle_process(technique* tech, float t) = 0;
			std::string name_;
			bool enabled_;
			bool observe_till_event_;
			bool actions_executed_;
			std::vector<action_ptr> actions_;
		};
	}
}
