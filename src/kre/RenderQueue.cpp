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

#include "asserts.hpp"
#include "Renderable.hpp"
#include "RenderQueue.hpp"
#include "WindowManager.hpp"

namespace KRE
{
	RenderQueue::RenderQueue(const std::string& name) 
		: name_(name)
	{
	}

	RenderQueuePtr RenderQueue::create(const std::string& name)
	{
		return std::make_shared<RenderQueue>(name);
	}

	void RenderQueue::enQueue(uint64_t order, RenderablePtr p)
	{
		renderables_[order] = p;
	}

	void RenderQueue::deQueue(uint64_t order)
	{
		auto it = renderables_.find(order);
		ASSERT_LOG(it != renderables_.end(), "RenderQueue(" << name() << ") nothing to dequeue at order: " << order);
		renderables_.erase(it);
	}

	void RenderQueue::preRender(const WindowPtr& wm)
	{
		for(auto r : renderables_) {
			r.second->preRender(wm);
		}
	}

	void RenderQueue::render(const WindowPtr& wm) const 
	{
		for(auto r : renderables_) {
			wm->render(r.second.get());
		}
	}

	void RenderQueue::postRender(const WindowPtr& wm)
	{
		for(auto r : renderables_) {
			r.second->postRender(wm);
		}
		renderables_.clear();
	}
}
