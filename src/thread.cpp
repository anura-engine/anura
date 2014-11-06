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
#include <boost/scoped_ptr.hpp>

#include <iostream>
#include <vector>

#include "thread.hpp"

namespace {

std::vector<SDL_Thread*> detached_threads;

}

namespace threading {

manager::~manager()
{
	for(std::vector<SDL_Thread*>::iterator i = detached_threads.begin(); i != detached_threads.end(); ++i) {
		SDL_WaitThread(*i,NULL);
	}
}

namespace {

int call_boost_function(void* arg)
{
	boost::scoped_ptr<boost::function<void()> > fn((boost::function<void()>*)arg);
	(*fn)();
	return 0;
}

}

thread::thread(const std::string& name, boost::function<void()> fn) 
	: fn_(fn), thread_(SDL_CreateThread(call_boost_function, name.c_str(), new boost::function<void()>(fn_)))
{}

thread::~thread()
{
	join();
}

void thread::join()
{
	if(thread_ != NULL) {
		SDL_WaitThread(thread_,NULL);
		thread_ = NULL;
	}
}

void thread::detach()
{
	detached_threads.push_back(thread_);
	thread_ = NULL;
}

mutex::mutex() : m_(SDL_CreateMutex())
{}

mutex::~mutex()
{
	SDL_DestroyMutex(m_);
}

mutex::mutex(const mutex&) : m_(SDL_CreateMutex())
{}

const mutex& mutex::operator=(const mutex&)
{
	return *this;
}

lock::lock(const mutex& m) : m_(m)
{
	SDL_mutexP(m_.m_);
}

lock::~lock()
{
	SDL_mutexV(m_.m_);
}

condition::condition() : cond_(SDL_CreateCond())
{}

condition::~condition()
{
	SDL_DestroyCond(cond_);
}

bool condition::wait(const mutex& m)
{
	return SDL_CondWait(cond_,m.m_) == 0;
}

condition::WAIT_TIMEOUT_RESULT condition::wait_timeout(const mutex& m, unsigned int timeout)
{
	const int res = SDL_CondWaitTimeout(cond_,m.m_,timeout);
	switch(res) {
		case 0: return THREAD_WAIT_OK;
		case SDL_MUTEX_TIMEDOUT: return THREAD_WAIT_TIMEOUT;
		default:
			 std::cerr << "SDL_CondWaitTimeout: " << SDL_GetError() << "\n";
			return THREAD_WAIT_ERROR;
	}
}

bool condition::notify_one()
{
	if(SDL_CondSignal(cond_) < 0) {
		std::cerr << "SDL_CondSignal: " << SDL_GetError() << "\n";
		return false;
	}

	return true;
}

bool condition::notify_all()
{
	if(SDL_CondBroadcast(cond_) < 0) {
		std::cerr << "SDL_CondBroadcast: " << SDL_GetError() << "\n";
		return false;
	}
	return true;
}

}
