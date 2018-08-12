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

#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "logger.hpp"

namespace test 
{
	struct FailureException 
	{
	};

	typedef std::function<void ()> UnitTest;
	typedef std::function<void (int)> BenchmarkTest;
	typedef std::function<void (int, const std::string&)> CommandLineBenchmarkTest;
	typedef std::function<void (const std::vector<std::string>&)> UtilityProgram;

	int register_test(const std::string& name, UnitTest test);
	int register_benchmark(const std::string& name, BenchmarkTest test);
	int register_benchmark_cl(const std::string& name, CommandLineBenchmarkTest test);
	int register_utility(const std::string& name, UtilityProgram utility, bool needs_video);
	bool utility_needs_video(const std::string& name);
	bool run_tests(const std::vector<std::string>* tests=nullptr);
	void run_benchmarks(const std::vector<std::string>* benchmarks=nullptr);
	void run_command_line_benchmark(const std::string& benchmark_name, const std::string& arg);
	void run_utility(const std::string& utility_name, const std::vector<std::string>& arg);

	std::string run_benchmark(const std::string& name, BenchmarkTest fn);
}

#define CHECK(cond, msg) do { if(!(cond)) { std::ostringstream _s; _s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << ": TEST CHECK FAILED:\nCONDITION:\n\t→ " << #cond << ":\nRESULTS:\n\t" << msg; log_internal(SDL_LOG_PRIORITY_CRITICAL, _s.str()); throw test::FailureException(); } } while(0)
/**  CHECK_H(cond, msg, heading) is like CHECK(cond, msg), but receiving an
 * additional heading message. */
#define CHECK_H(cond, msg, heading) if (!(cond)) {            \
	std::ostringstream _s;                                 \
	_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ <<      \
			": TEST CHECK FAILED:\n" <<              \
			heading << '\n' <<                        \
			"CONDITION:\n\t→ " << #cond << '\n' <<     \
			"RESULTS:\n\t" << msg;                      \
	log_internal(SDL_LOG_PRIORITY_CRITICAL, _s.str());           \
	throw test::FailureException(); }

#define CHECK_CMP(a, b, cmp) CHECK((a) cmp (b), #a << ":\n\t→ " << (a) << ";\n\t" << #b << ":\n\t→ " << (b))
/**  CHECK_CMP_M(a, b, cmp, m) is like CHECK_CMP(a, b, cmp), but receiving an
 * additional [heading] message. */
#define CHECK_CMP_M(a, b, cmp, m) CHECK_H(                         \
	(a) cmp (b),                                                \
	#a << ":\n\t→ " << (a) << ";\n\t" << #b << ":\n\t→ " << (b), \
	m)

#define CHECK_EQ(a, b) CHECK_CMP(a, b, ==)
/**  CHECK_EQ_M is like CHECK_EQ, but receiving an additional [heading] message
 * to be printed in case of error. */
#define CHECK_EQ_M(a, b, m) CHECK_CMP_M(a, b, ==, m)
#define CHECK_NE(a, b) CHECK_CMP(a, b, !=)
#define CHECK_LE(a, b) CHECK_CMP(a, b, <=)
#define CHECK_GE(a, b) CHECK_CMP(a, b, >=)
#define CHECK_LT(a, b) CHECK_CMP(a, b, <)
#define CHECK_GT(a, b) CHECK_CMP(a, b, >)

//on mobile phones we don't do unit tests or benchmarks.
#if defined(MOBILE_BUILD)

#define UNIT_TEST(name) \
	void TEST_##name()

#define BENCHMARK(name) \
	void BENCHMARK_##name(int benchmark_iterations)

#define BENCHMARK_LOOP

#define BENCHMARK_ARG(name, arg) \
	void BENCHMARK_ARG_##name(int benchmark_iterations, arg)

#define BENCHMARK_ARG_CALL(name, id, arg)

#define BENCHMARK_ARG_CALL_COMMAND_LINE(name)

#define UTILITY(name) void UTILITY_##name(const std::vector<std::string>& args)

#define COMMAND_LINE_UTILITY(name) \
	void UTILITY_##name(const std::vector<std::string>& args)

#else

#define UNIT_TEST(name) \
	void TEST_##name(); \
	static int TEST_VAR_##name = test::register_test(#name, TEST_##name); \
	void TEST_##name()

#define BENCHMARK(name) \
	void BENCHMARK_##name(int benchmark_iterations); \
	static int BENCHMARK_VAR_##name = test::register_benchmark(#name, BENCHMARK_##name); \
	void BENCHMARK_##name(int benchmark_iterations)

#define BENCHMARK_LOOP while(benchmark_iterations--)

#define BENCHMARK_ARG(name, arg) \
	void BENCHMARK_ARG_##name(int benchmark_iterations, arg)

#define BENCHMARK_ARG_CALL(name, id, arg) \
	void BENCHMARK_ARG_CALL_##name_##id(int benchmark_iterations) { \
		BENCHMARK_ARG_##name(benchmark_iterations, arg); \
	} \
	static int BENCHMARK_ARG_VAR_##name_##id = test::register_benchmark(#name " " #id, BENCHMARK_ARG_CALL_##name_##id);

#define BENCHMARK_ARG_CALL_COMMAND_LINE(name) \
	void BENCHMARK_ARG_CALL_##name(int benchmark_iterations, const std::string& arg) { \
		BENCHMARK_ARG_##name(benchmark_iterations, arg); \
	} \
	static int BENCHMARK_ARG_VAR_##name = test::register_benchmark_cl(#name, BENCHMARK_ARG_CALL_##name);

#define UTILITY(name) \
    void UTILITY_##name(const std::vector<std::string>& args); \
	static int UTILITY_VAR_##name = test::register_utility(#name, UTILITY_##name, true); \
	void UTILITY_##name(const std::vector<std::string>& args)

#define COMMAND_LINE_UTILITY(name) \
    void UTILITY_##name(const std::vector<std::string>& args); \
	static int UTILITY_VAR_##name = test::register_utility(#name, UTILITY_##name, false); \
	void UTILITY_##name(const std::vector<std::string>& args)

#endif
