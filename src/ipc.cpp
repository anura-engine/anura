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
#include <sys/types.h>
#include <fcntl.h>
#include "asserts.hpp"
#include "ipc.hpp"

#if defined(UTILITY_IN_PROC)

namespace
{
	shared_sem_type shared_sem = NULL;
}

namespace ipc
{
	namespace semaphore
	{
		bool in_use()
		{
			return shared_sem != NULL;
		}

		void post()
		{
#if defined(_MSC_VER)
			ASSERT_LOG(ReleaseSemaphore(shared_sem, 1, NULL) != NULL, 
				"Tried to release a semaphore which is alredy signaled.");
#else
			ASSERT_LOG(sem_post(shared_sem) == 0, 
				"Error calling sem_post: " << errno);
#endif
		}

		bool trywait()
		{
#if defined(_MSC_VER)
			if(WaitForSingleObject(shared_sem, 20) != WAIT_OBJECT_0) {
				return false;
			}
#else
			if(sem_trywait(shared_sem) < 0 && errno == EAGAIN) {
				return false;
			}
#endif
			return true;
		}

		bool open(const std::string& sem_name)
		{
#if defined(_MSC_VER)
			shared_sem = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, false, sem_name.c_str());
			if(shared_sem == INVALID_HANDLE_VALUE) {
#else
			shared_sem = sem_open(sem_name.c_str(), 0);
			if(shared_sem == NULL) {
#endif
				return false;
			}
			return true;
		}

		bool create(const std::string& sem_name, int initial_count)
		{
#if defined(_MSC_VER)
			shared_sem = CreateSemaphoreA(NULL, initial_count, 1, sem_name.c_str());
			if(shared_sem == INVALID_HANDLE_VALUE) {
#else
			shared_sem = sem_open(sem_name.c_str(), O_CREAT, S_IRUSR|S_IWUSR, initial_count);
			if(shared_sem == NULL) {
#endif
				return false;
			}
			return true;
		}
	}
}

#endif
