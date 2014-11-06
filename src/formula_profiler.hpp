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
#ifndef FORMULA_PROFILER_HPP_INCLUDED
#define FORMULA_PROFILER_HPP_INCLUDED

#include <string>

#ifdef DISABLE_FORMULA_PROFILER

namespace formula_profiler
{

void dump_instrumentation() {}

class instrument
{
public:
	explicit instrument(const char* id) {}
	~instrument() {}
};

//should be called every cycle while the profiler is running.
void pump();

class manager
{
public:
	explicit manager(const char* output_file) {}
	~manager() {}
};

class suspend_scope
{
};

inline std::string get_profile_summary() { return ""; }

}

#else

#include <vector>

#if defined(_WINDOWS)
#include "utils.hpp"
#else
#include <sys/time.h>
#endif

class custom_object_type;

namespace formula_profiler
{

//instruments inside a given scope.
class instrument
{
public:
	explicit instrument(const char* id);
	~instrument();
private:
	const char* id_;
	struct timeval tv_;
};

void dump_instrumentation();

//should be called every cycle while the profiler is running.
void pump();

struct custom_object_event_frame {
	const custom_object_type* type;
	int event_id;
	bool executing_commands;

	bool operator<(const custom_object_event_frame& f) const;
};

typedef std::vector<custom_object_event_frame> event_call_stack_type;
extern event_call_stack_type event_call_stack;

class manager
{
public:
	explicit manager(const char* output_file);
	~manager();
};

void end_profiling();

class suspend_scope
{
public:
	suspend_scope() { event_call_stack.swap(backup_); }
	~suspend_scope() { event_call_stack.swap(backup_); }
private:
	event_call_stack_type backup_;
};

std::string get_profile_summary();

}

#endif

#endif
