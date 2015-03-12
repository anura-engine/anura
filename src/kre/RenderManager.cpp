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
#include "RenderManager.hpp"
#include "RenderQueue.hpp"

namespace KRE
{
	RenderManager::RenderManager()
	{
	}

	RenderManagerPtr RenderManager::getInstance()
	{
		static RenderManagerPtr res = std::make_shared<RenderManager>();
		return res;
	}

	RenderQueuePtr RenderManager::addQueue(int priority, const std::string& queue_name)
	{
		RenderQueuePtr queue = RenderQueue::create(queue_name);
		auto it = render_queues_.find(priority);
		if(it != render_queues_.end()) {
			LOG_WARN("Replacing queue " << it->second->name() << " at priority " << priority << " with queue " << queue->name());
		}
		render_queues_[priority] = queue;
		return queue;
	}

	void RenderManager::removeQueue(int priority)
	{
		auto it = render_queues_.find(priority);
		ASSERT_LOG(it != render_queues_.end(), "Tried to remove non-existant render queue at priority: " << priority);
		render_queues_.erase(it);
	}

	void RenderManager::render(const WindowPtr& wm) const
	{
		for(auto& q : render_queues_) {
			q.second->preRender(wm);
		}
		for(auto& q : render_queues_) {
			q.second->render(wm);
		}
		for(auto& q : render_queues_) {
			q.second->postRender(wm);
		}
	}

	void RenderManager::addRenderableToQueue(size_t q, size_t order, const RenderablePtr& r)
	{
		auto it = render_queues_.find(q);
		ASSERT_LOG(it != render_queues_.end(), "Tried to add renderable to non-existant render queue at priority: " << q);
		it->second->enQueue(order, r);
	}
}
