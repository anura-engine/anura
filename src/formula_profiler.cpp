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

#ifndef DISABLE_FORMULA_PROFILER

#include <SDL_thread.h>
#include <SDL_timer.h>

#include <assert.h>
#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include <signal.h>
#include <stdio.h>
#include <string.h>
#if !defined(_MSC_VER)
#include <sys/time.h>
#else
#include <time.h>
#endif

#include "custom_object_type.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "formula_profiler.hpp"
#include "object_events.hpp"
#include "variant.hpp"

void init_call_stack(int min_size);

namespace formula_profiler
{
	namespace 
	{
		bool profiler_on = false;

		struct InstrumentationRecord 
		{
			InstrumentationRecord() : time_us(0), nsamples(0)
			{}
			int time_us, nsamples;
		};

		std::map<const char*, InstrumentationRecord> g_instrumentation;
	}

	Instrument::Instrument(const char* id) : id_(id)
	{
		if(profiler_on) {
			gettimeofday(&tv_, nullptr);
		}
	}

	Instrument::~Instrument()
	{
		if(profiler_on) {
			struct timeval end_tv;
			gettimeofday(&end_tv, nullptr);
			InstrumentationRecord& r = g_instrumentation[id_];
			r.time_us += (end_tv.tv_sec - tv_.tv_sec)*1000000 + (end_tv.tv_usec - tv_.tv_usec);
			r.nsamples++;
		}
	}

	void dump_instrumentation()
	{
		static struct timeval prev_call;
		static bool first_call = true;

		struct timeval tv;
		gettimeofday(&tv, nullptr);

		if(!first_call && g_instrumentation.empty() == false) {
			const int time_us = (tv.tv_sec - prev_call.tv_sec)*1000000 + (tv.tv_usec - prev_call.tv_usec);
			if(time_us) {
				std::ostringstream ss;
				ss << "FRAME INSTRUMENTATION TOTAL TIME: " << time_us << "us. INSTRUMENTS: ";
				for(std::map<const char*,InstrumentationRecord>::const_iterator i = g_instrumentation.begin(); i != g_instrumentation.end(); ++i) {
					const int percent = (i->second.time_us*100)/time_us;
					ss << i->first << ": " << i->second.time_us << "us (" << percent << "%) in " << i->second.nsamples << " calls; ";
				}
				LOG_INFO(ss.str());
			}

			g_instrumentation.clear();
		}

		first_call = false;
		prev_call = tv;
	}

	EventCallStackType event_call_stack;

	namespace 
	{
		bool handler_disabled = false;
		std::string output_fname;
		SDL_threadID main_thread;

		int empty_samples = 0;

		std::map<std::vector<CallStackEntry>, int> expression_call_stack_samples;
		std::vector<CallStackEntry> current_expression_call_stack;

		std::vector<CustomObjectEventFrame> event_call_stack_samples;
		int num_samples = 0;
		const size_t max_samples = 10000;

		int nframes_profiled = 0;

#if defined(_MSC_VER) || MOBILE_BUILD
		SDL_TimerID sdl_profile_timer;
#endif

#if defined(_MSC_VER) || MOBILE_BUILD
		Uint32 sdl_timer_callback(Uint32 interval, void *param)
#else
		void sigprof_handler(int sig)
#endif
		{
			//NOTE: Nothing in this function should allocate memory, since
			//we might be called while allocating memory.
#if defined(_MSC_VER) || MOBILE_BUILD
			if(handler_disabled) {
				return interval;
			}
#else
			if(handler_disabled || main_thread != SDL_ThreadID()) {
				return;
			}
#endif

			if(current_expression_call_stack.empty() && current_expression_call_stack.capacity() >= get_expression_call_stack().size()) {
				bool valid = true;

				//Very important that this does not allocate memory.
				if(valid) {
					current_expression_call_stack = get_expression_call_stack();

					for(int n = 0; n != current_expression_call_stack.size(); ++n) {
						if(current_expression_call_stack[n].expression == nullptr) {
							valid = false;
							break;
						}

						intrusive_ptr_add_ref(current_expression_call_stack[n].expression);
					}

					if(!valid) {
						current_expression_call_stack.clear();
					}
				}
			}

			if(num_samples == max_samples) {
#if defined(_MSC_VER) || MOBILE_BUILD
				return interval;
#else
				return;
#endif
			}

			if(event_call_stack.empty()) {
				++empty_samples;
			} else {
				event_call_stack_samples[num_samples++] = event_call_stack.back();
			}
	#if defined(_MSC_VER) || MOBILE_BUILD
			return interval;
	#endif
		}
	}

	Manager::Manager(const char* output_file)
	{
		if(output_file) {
			current_expression_call_stack.reserve(10000);
			event_call_stack_samples.resize(max_samples);

			main_thread = SDL_ThreadID();

			LOG_INFO("SETTING UP PROFILING: " << output_file);
			profiler_on = true;
			output_fname = output_file;

			init_call_stack(65536);

#if defined(_MSC_VER) || MOBILE_BUILD
			// Crappy windows approximation.
			sdl_profile_timer = SDL_AddTimer(10, sdl_timer_callback, 0);
			if(sdl_profile_timer == 0) {
				LOG_WARN("Couldn't create a profiling timer!");
			}
#else
			signal(SIGPROF, sigprof_handler);

			struct itimerval timer;
			timer.it_interval.tv_sec = 0;
			timer.it_interval.tv_usec = 10000;
			timer.it_value = timer.it_interval;
			setitimer(ITIMER_PROF, &timer, 0);
#endif
		}
	}

	Manager::~Manager()
	{
		end_profiling();
	}

	void end_profiling()
	{
		LOG_INFO("END PROFILING: " << (int)profiler_on);
		if(profiler_on) {
#if defined(_MSC_VER) || MOBILE_BUILD
			SDL_RemoveTimer(sdl_profile_timer);
#else
			struct itimerval timer;
			memset(&timer, 0, sizeof(timer));
			setitimer(ITIMER_PROF, &timer, 0);
#endif

			std::map<std::string, int> samples_map;

			for(int n = 0; n != num_samples; ++n) {
				const CustomObjectEventFrame& frame = event_call_stack_samples[n];

				std::string str = formatter() << frame.type->id() << ":" << get_object_event_str(frame.event_id) << ":" << (frame.executing_commands ? "CMD" : "FFL");

				samples_map[str]++;
			}

			std::vector<std::pair<int, std::string> > sorted_samples, cum_sorted_samples;
			for(std::map<std::string, int>::const_iterator i = samples_map.begin(); i != samples_map.end(); ++i) {
				sorted_samples.push_back(std::pair<int, std::string>(i->second, i->first));
			}

			std::sort(sorted_samples.begin(), sorted_samples.end());
			std::reverse(sorted_samples.begin(), sorted_samples.end());

			const int total_samples = empty_samples + num_samples;
			if(!total_samples) {
				return;
			}

			std::ostringstream s;
			s << "TOTAL SAMPLES: " << total_samples << "\n";
			s << (100*empty_samples)/total_samples << "% (" << empty_samples << ") CORE ENGINE (non-FFL processing)\n";

			for(int n = 0; n != sorted_samples.size(); ++n) {
				s << (100*sorted_samples[n].first)/total_samples << "% (" << sorted_samples[n].first << ") " << sorted_samples[n].second << "\n";
			}

			sorted_samples.clear();

			std::map<const game_logic::FormulaExpression*, int> expr_samples, cum_expr_samples;

			int total_expr_samples = 0;

			for(std::map<std::vector<CallStackEntry>, int>::const_iterator i = expression_call_stack_samples.begin(); i != expression_call_stack_samples.end(); ++i) {
				const std::vector<CallStackEntry>& sample = i->first;
				const int nsamples = i->second;
				if(sample.empty()) {
					continue;
				}

				for(const CallStackEntry& entry : sample) {
					cum_expr_samples[entry.expression] += nsamples;
				}

				expr_samples[sample.back().expression] += nsamples;

				total_expr_samples += nsamples;
			}

			for(std::map<const game_logic::FormulaExpression*, int>::const_iterator i = expr_samples.begin(); i != expr_samples.end(); ++i) {
				sorted_samples.push_back(std::pair<int, std::string>(i->second, formatter() << i->first->debugPinpointLocation() << " (called " << double(i->first->getNTimesCalled())/double(nframes_profiled) << " times per frame)"));
			}

			for(std::map<const game_logic::FormulaExpression*, int>::const_iterator i = cum_expr_samples.begin(); i != cum_expr_samples.end(); ++i) {
				cum_sorted_samples.push_back(std::pair<int, std::string>(i->second, formatter() << i->first->debugPinpointLocation() << " (called " << double(i->first->getNTimesCalled())/double(nframes_profiled) << " times per frame)"));
			}

			std::sort(sorted_samples.begin(), sorted_samples.end());
			std::reverse(sorted_samples.begin(), sorted_samples.end());

			std::sort(cum_sorted_samples.begin(), cum_sorted_samples.end());
			std::reverse(cum_sorted_samples.begin(), cum_sorted_samples.end());

			s << "\n\nPROFILE BROKEN DOWN INTO FFL EXPRESSIONS:\n\nTOTAL SAMPLES: " << total_expr_samples << "\n OVER " << nframes_profiled << " FRAMES\nSELF TIME:\n";

			for(int n = 0; n != sorted_samples.size(); ++n) {
				s << (100*sorted_samples[n].first)/total_expr_samples << "% (" << sorted_samples[n].first << ") " << sorted_samples[n].second << "\n";
			}

			s << "\n\nCUMULATIVE TIME:\n";
			for(int n = 0; n != cum_sorted_samples.size(); ++n) {
				s << (100*cum_sorted_samples[n].first)/total_expr_samples << "% (" << cum_sorted_samples[n].first << ") " << cum_sorted_samples[n].second << "\n";
			}

			if(!output_fname.empty()) {
				sys::write_file(output_fname, s.str());
				LOG_INFO("WROTE PROFILE TO " << output_fname);
			} else {
				LOG_INFO("===\n=== PROFILE REPORT ===");
				LOG_INFO(s.str());
				LOG_INFO("=== END PROFILE REPORT ===");
			}

			profiler_on = false;
		}
	}

	void pump()
	{
		static int instr_count = 0;
		if(++instr_count%50 == 0) {
			dump_instrumentation();
		}

		if(current_expression_call_stack.empty() == false) {
			expression_call_stack_samples[current_expression_call_stack]++;
			current_expression_call_stack.clear();
		}

		++nframes_profiled;
	}

	bool CustomObjectEventFrame::operator<(const CustomObjectEventFrame& f) const
	{
		return type < f.type || (type == f.type && event_id < f.event_id) ||
			   (type == f.type && event_id == f.event_id && executing_commands < f.executing_commands);
	}

	std::string get_profile_summary()
	{
		if(!profiler_on) {
			return "";
		}

		handler_disabled = true;

		static int last_empty_samples = 0;
		static int last_num_samples = 0;

		const int nsamples = num_samples - last_num_samples;
		const int nempty = empty_samples - last_empty_samples;

		std::sort(event_call_stack_samples.end() - nsamples, event_call_stack_samples.end());

		std::ostringstream s;

		s << "PROFILE: " << (nsamples + nempty) << " CPU. " << nsamples << " IN FFL ";


		std::vector<std::pair<int, std::string> > samples;
		int count = 0;
		for(int n = last_num_samples; n < num_samples; ++n) {
			if(n+1 == num_samples || event_call_stack_samples[n].type != event_call_stack_samples[n+1].type) {
				samples.push_back(std::pair<int, std::string>(count + 1, event_call_stack_samples[n].type->id()));
				count = 0;
			} else {
				++count;
			}
		}

		std::sort(samples.begin(), samples.end());
		std::reverse(samples.begin(), samples.end());
		for(int n = 0; n != samples.size(); ++n) {
			s << samples[n].second << " " << samples[n].first << " ";
		}

		last_empty_samples = empty_samples;
		last_num_samples = num_samples;

		handler_disabled = false;

		return s.str();
	}

}

#endif
