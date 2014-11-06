/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include <map>
#include <vector>

#include "background_task_pool.hpp"
#include "foreach.hpp"
#include "thread.hpp"

namespace background_task_pool
{

namespace {

int next_task_id = 0;

struct task {
	boost::function<void()> job, on_complete;
	boost::shared_ptr<threading::thread> thread;
};

threading::mutex* completed_tasks_mutex = NULL;
std::vector<int> completed_tasks;

std::map<int, task> task_map;

void run_task(boost::function<void()> job, int task_id)
{
	job();
	threading::lock(*completed_tasks_mutex);
	completed_tasks.push_back(task_id);
}

}

manager::manager()
{
	completed_tasks_mutex = new threading::mutex;
}

manager::~manager()
{
	while(task_map.empty() == false) {
		pump();
	}
}

void submit(boost::function<void()> job, boost::function<void()> on_complete)
{
	task t = { job, on_complete, boost::shared_ptr<threading::thread>(new threading::thread("background_task", boost::bind(run_task, job, next_task_id))) };
	task_map[next_task_id] = t;
	++next_task_id;
}

void pump()
{
	std::vector<int> completed;
	{
		threading::lock(*completed_tasks_mutex);
		completed.swap(completed_tasks);
	}

	foreach(int t, completed) {
		task_map[t].on_complete();
		task_map.erase(t);
	}
}

}
