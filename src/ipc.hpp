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
#pragma once
#ifndef IPC_HPP_INCLUDED
#define IPC_HPP_INCLUDED

#if defined(UTILITY_IN_PROC)

#include <string>

#if defined(_MSC_VER)
#include <windows.h>
#else
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <signal.h>
#endif

#if defined(_MSC_VER)
typedef HANDLE shared_sem_type;
#else
typedef sem_t* shared_sem_type;
#endif

namespace ipc
{
	namespace semaphore
	{
		bool in_use();
		void post();
		bool trywait();
		bool open(const std::string& sem_name);
		bool create(const std::string& sem_name, int initial_count);
	}
}

#endif

#endif
