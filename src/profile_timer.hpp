/*
   Copyright 2014 Kristina Simpson <sweet.kristas@gmail.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#pragma once

#include "SDL.h"
#include <iostream>
#include <string>

namespace profile 
{
	struct manager
	{
		Uint64 frequency;
		Uint64 t1, t2;
		double elapsedTime;
		const char* name;

		manager(const char* const str) : elapsedTime(0.0), name(str)
		{
			frequency = SDL_GetPerformanceFrequency();
			t1 = SDL_GetPerformanceCounter();
		}

		~manager()
		{
			t2 = SDL_GetPerformanceCounter();
			elapsedTime = (t2 - t1) * 1000.0 / frequency;
			std::cerr << name << ": " << elapsedTime << " milliseconds" << std::endl;
		}
	};

	struct timer
	{
		Uint64 frequency;
		Uint64 t1, t2;

		timer()
		{
			frequency = SDL_GetPerformanceFrequency();
			t1 = SDL_GetPerformanceCounter();
		}

		// Elapsed time in milliseconds
		double get_time()
		{
			t2 = SDL_GetPerformanceCounter();
			return (t2 - t1) * 1000000.0 / frequency;
		}
	};

	inline void sleep(unsigned long t) 
	{
		SDL_Delay(t);
	}

	inline void delay(unsigned long t) 
	{
		sleep(t);
	}

	inline int get_tick_time()
	{
		return static_cast<int>(SDL_GetTicks());
	}
}
