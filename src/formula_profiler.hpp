/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#include <string>

#ifdef DISABLE_FORMULA_PROFILER

namespace formula_profiler
{
	void dump_instrumentation() {}

	class Instrument
	{
	public:
		explicit Instrument(const char* id) {}
		~Instrument() {}
	};

	//should be called every cycle while the profiler is running.
	void pump();

	class Manager
	{
	public:
		explicit Manager(const char* output_file) {}
		~Manager() {}
	};

	class SuspendScope
	{
	};

	inline std::string get_profile_summary() { return ""; }
}

#else

#include <vector>

#if defined(_MSC_VER)
#include "utils.hpp"
#else
#include <sys/time.h>
#endif

class CustomObjectType;

namespace formula_profiler
{
	//instruments inside a given scope.
	class Instrument
	{
	public:
		explicit Instrument(const char* id);
		~Instrument();
	private:
		const char* id_;
		struct timeval tv_;
	};

	void dump_instrumentation();

	//should be called every cycle while the profiler is running.
	void pump();

	struct CustomObjectEventFrame 
	{
		const CustomObjectType* type;
		int event_id;
		bool executing_commands;

		bool operator<(const CustomObjectEventFrame& f) const;
	};

	typedef std::vector<CustomObjectEventFrame> EventCallStackType;
	extern EventCallStackType event_call_stack;

	class Manager
	{
	public:
		explicit Manager(const char* output_file);
		~Manager();
	};

	void end_profiling();

	class SuspendScope
	{
	public:
		SuspendScope() { event_call_stack.swap(backup_); }
		~SuspendScope() { event_call_stack.swap(backup_); }
	private:
		EventCallStackType backup_;
	};

	std::string get_profile_summary();
}

#endif
