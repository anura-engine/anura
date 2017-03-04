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

#include "SDL.h"

#include "formatter.hpp"
#include "formula.hpp"

#define PROFILE_INSTRUMENT(id, info) \
	formula_profiler::Instrument id_##instrument; \
	if(formula_profiler::profiler_on) { \
		variant v(formatter() << info); \
		id_##instrument.init(#id, v); \
	}

#ifdef DISABLE_FORMULA_PROFILER

namespace formula_profiler
{
	static bool profiler_on = false;

	void dump_instrumentation() {}

	class Instrument
	{
	public:
		static const char* generate_id(const char* id, int num) { return id; }
		explicit Instrument(const char* id, const game_logic::Formula* formula=nullptr) {}
		~Instrument() {}
		void init(const char* id, variant info);
		uint64_t get_ns() const { return 0; }
		void finish() {}
	};

	//should be called every cycle while the profiler is running.
	void pump();

	void draw();
	bool handle_sdl_event(const SDL_Event& event, bool claimed);

	class Manager
	{
	public:
		explicit Manager(const char* output_file) {}
		~Manager() {}
		static Manager* get();
		void init(const char* output_file, bool memory_profiler=false) {}
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
	extern bool profiler_on;

	//instruments inside a given scope.
	class Instrument
	{
	public:
		static const char* generate_id(const char* id, int num);

		Instrument();
		void init(const char* id, variant info);
		explicit Instrument(const char* id, const game_logic::Formula* formula=nullptr);
		Instrument(const char* id, variant info);
		~Instrument();

		void finish();

		uint64_t get_ns() const;
	private:
		const char* id_;
		uint64_t t_;
	};

	void dump_instrumentation();

	//should be called every cycle while the profiler is running.
	void pump();

	void draw();
	bool handle_sdl_event(const SDL_Event& event, bool claimed);

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
		static Manager* get();
		explicit Manager(const char* output_file);
		~Manager();

		bool is_profiling() const;
		void init(const char* output_file, bool memory_profiler=false);
		void halt();
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
