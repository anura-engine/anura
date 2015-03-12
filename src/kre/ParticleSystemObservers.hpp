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

#include "ParticleSystemFwd.hpp"
#include "variant.hpp"

namespace KRE
{
	namespace Particles
	{
		class Action;
		typedef std::shared_ptr<Action> ActionPtr;

		class EventHandler;
		typedef std::shared_ptr<EventHandler> EventHandlerPtr;

		class Action
		{
		public:
			explicit Action(const variant& node);
			virtual ~Action();
			const std::string& getName() const { return name_; }
			void execute(TechniquePtr tech, float t);
			virtual ActionPtr clone() const = 0;
			static ActionPtr create(const variant& node);
		private:
			std::string name_;
		};

		class EventHandler
		{
		public:
			explicit EventHandler(const variant& node);
			virtual ~EventHandler();
			void process(TechniquePtr tech, float t);
			void processActions(TechniquePtr tech, float t);
			void addAction(ActionPtr evt);
			const std::string& name() const { return name_; }
			bool isEnabled() const { return enabled_; }
			void enable(bool en=true) { enabled_ = en; }
			void disable() { enabled_ = false; }
			virtual EventHandlerPtr clone() const = 0;
			static EventHandlerPtr create(const variant& node);
		private:
			virtual bool handleProcess(TechniquePtr tech, float t) = 0;
			std::string name_;
			bool enabled_;
			bool observe_till_event_;
			bool actions_executed_;
			std::vector<ActionPtr> actions_;
		};
	}
}
