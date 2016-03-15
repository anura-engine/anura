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

#include <iostream>
#include <map>
#include <set>

#include "asserts.hpp"
#include "preferences.hpp"
#include "profile_timer.hpp"
#include "unit_test.hpp"

namespace test 
{
	namespace 
	{
		typedef std::map<std::string, UnitTest> TestMap;
		TestMap& get_test_map()
		{
			static TestMap map;
			return map;
		}

		typedef std::map<std::string, BenchmarkTest> BenchmarkMap;
		BenchmarkMap& get_benchmark_map()
		{
			static BenchmarkMap map;
			return map;
		}

		typedef std::map<std::string, CommandLineBenchmarkTest> CommandLineBenchmarkMap;
		CommandLineBenchmarkMap& get_cl_benchmark_map()
		{
			static CommandLineBenchmarkMap map;
			return map;
		}

		typedef std::map<std::string, UtilityProgram> UtilityMap;
		UtilityMap& get_utility_map()
		{
			static UtilityMap map;
			return map;
		}

		std::set<std::string>& get_command_line_utilities() {
			static std::set<std::string> map;
			return map;
		}
	}

	int register_test(const std::string& name, UnitTest test)
	{
		get_test_map()[name] = test;
		return 0;
	}

	int register_utility(const std::string& name, UtilityProgram utility, bool needs_video)
	{
		get_utility_map()[name] = utility;
		if(!needs_video) {
			get_command_line_utilities().insert(name);
		}
		return 0;
	}

	bool utility_needs_video(const std::string& name)
	{
		return get_command_line_utilities().count(name) == 0;
	}

	bool run_tests(const std::vector<std::string>* tests)
	{
		const int start_time = profile::get_tick_time();
		std::vector<std::string> all_tests;
		if(!tests) {
			for(TestMap::const_iterator i = get_test_map().begin(); i != get_test_map().end(); ++i) {
				all_tests.push_back(i->first);
			}

			tests = &all_tests;
		}

		int npass = 0, nfail = 0;
		for(const std::string& test : *tests) {
			if(preferences::run_failing_unit_tests() == false && test.size() > 5 && std::string(test.end()-5, test.end()) == "FAILS") {
				continue;
			}

			try {
				get_test_map().at(test)();
				LOG_INFO("TEST " << test << " PASSED");
				++npass;
			} catch(FailureException&) {
				LOG_ERROR("TEST " << test << " FAILED!!");
				++nfail;
			} catch(std::out_of_range&) {
				LOG_ERROR("TEST " << test << " NOT FOUND.");
				++nfail;
			}
		}

		if(nfail) {
			LOG_INFO(npass << " TESTS PASSED, " << nfail << " TESTS FAILED");
			return false;
		} else {
			LOG_INFO("ALL " << npass << " TESTS PASSED IN " << (profile::get_tick_time() - start_time) << "ms");
			return true;
		}
	}

	int register_benchmark(const std::string& name, BenchmarkTest test)
	{
		get_benchmark_map()[name] = test;
		return 0;
	}

	int register_benchmark_cl(const std::string& name, CommandLineBenchmarkTest test)
	{
		get_cl_benchmark_map()[name] = test;
		return 0;
	}

	std::string run_benchmark(const std::string& name, BenchmarkTest fn)
	{
		//run it once without counting it to let any initialization code be run.
		fn(1);

		LOG_INFO("RUNNING BENCHMARK " << name << "...");
		const int MinTicks = 1000;
		for(int64_t nruns = 10; ; nruns *= 10) {
			const unsigned start_time = profile::get_tick_time();
			fn(static_cast<int>(nruns));
			const int64_t time_taken_ms = profile::get_tick_time() - start_time;
			if(time_taken_ms >= MinTicks || nruns > 1000000000) {
				int64_t time_taken = time_taken_ms*1000000LL;
				int time_taken_units = 0;
				int64_t time_taken_per_iter = time_taken/nruns;
				int time_taken_per_iter_units = 0;
				while(time_taken > 10000 && time_taken_units < 3) {
					time_taken /= 1000;
					time_taken_units++;
				}

				while(time_taken_per_iter > 10000 && time_taken_per_iter_units < 3) {
					time_taken_per_iter /= 1000;
					time_taken_per_iter_units++;
				}

				const char* units[] = {"ns", "us", "ms", "s"};
				std::ostringstream s;
				s << "BENCH " << name << ": " << nruns << " iterations, " << time_taken_per_iter << units[time_taken_per_iter_units] << "/iteration; total, " << time_taken << units[time_taken_units];
				std::string res = s.str();
				LOG_INFO(res);
				return res;
			}
		}

		return "";
	}

	void run_benchmarks(const std::vector<std::string>* benchmarks)
	{
		std::vector<std::string> all_benchmarks;
		if(!benchmarks) {
			for(BenchmarkMap::const_iterator i = get_benchmark_map().begin(); i != get_benchmark_map().end(); ++i) {
				all_benchmarks.push_back(i->first);
			}

			benchmarks = &all_benchmarks;
		}

		for(const std::string& benchmark : *benchmarks) {
			std::string::const_iterator colon = std::find(benchmark.begin(), benchmark.end(), ':');
			if(colon != benchmark.end()) {
				//this benchmark has a user-supplied argument
				const std::string bench_name(benchmark.begin(), colon);
				const std::string arg(colon+1, benchmark.end());
				run_command_line_benchmark(bench_name, arg);
			} else {
				try {
					run_benchmark(benchmark, get_benchmark_map().at(benchmark));
				} catch (std::out_of_range &) {
					LOG_INFO("BENCHMARK " << benchmark << " NOT FOUND.");
				}
			}
		}
	}

	void run_command_line_benchmark(const std::string& benchmark_name, const std::string& arg)
	{
		try {
			run_benchmark(benchmark_name, std::bind(get_cl_benchmark_map().at(benchmark_name), std::placeholders::_1, arg));
		} catch (std::out_of_range&) {
			LOG_INFO("COMMAND-LINE BENCHMARK " << benchmark_name << " NOT FOUND.");
		}
	}

	void run_utility(const std::string& utility_name, const std::vector<std::string>& arg)
	{
		UtilityProgram util = get_utility_map()[utility_name];
		if(!util) {
			std::string known;
			for(UtilityMap::const_iterator i = get_utility_map().begin(); i != get_utility_map().end(); ++i) {
				if(i->second) {
					known += i->first + " ";
				}
			}
			ASSERT_LOG(false, "Unknown utility: '" << utility_name << "'; known utilities: " << known);
		}
		util(arg);
	}
}
