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
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include <map>
#include <cstdint>

#include "RenderFwd.hpp"
#include "WindowManagerFwd.hpp"

namespace KRE
{
	class RenderQueue
	{
	public:
		RenderQueue(const std::string& name);
		const std::string& name() const { return name_; }

		void enQueue(uint64_t order, RenderablePtr p);	
		void deQueue(uint64_t order);

		void preRender(const WindowPtr& wm);
		void render(const WindowPtr& wm) const;
		void postRender(const WindowPtr& wm);

		static RenderQueuePtr create(const std::string& name);
	private:
		std::map<uint64_t, RenderablePtr> renderables_;
		std::string name_;
		RenderQueue();
		RenderQueue(const RenderQueue&);
	};
}
