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

#include <cstdio>

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "StackWalker.h"
#else
#include <csignal>
#include "stacktrace.hpp"
#endif

#ifndef NO_EDITOR
#include "editor.hpp"
#endif

#include "asserts.hpp"
#include "code_editor_dialog.hpp"
#include "level.hpp"
#include "preferences.hpp"
#include "stats.hpp"
#include "string_utils.hpp"
#include "variant.hpp"

namespace 
{
	PREF_BOOL_PERSISTENT(error_message_box, true, "Show a message dialog when an error occurs");
	PREF_INT_PERSISTENT(error_message_box_max_rows, 30, "Maximum rows in error message dialog");
	PREF_INT_PERSISTENT(error_message_box_max_cols, 180, "Maximum columns in error message dialog");

	std::string trim_error_message(std::string msg) {
		return util::word_wrap(
			msg, 
			g_error_message_box_max_cols, "", 
			g_error_message_box_max_rows, "(error message truncated. See console for more)"
		);
	}

	std::function<void()> g_edit_and_continue_fn;
}

void set_assert_edit_and_continue_fn(std::function<void()> fn)
{
	g_edit_and_continue_fn = fn;
}

assert_edit_and_continue_fn_scope::assert_edit_and_continue_fn_scope(std::function<void()> fn)
  : fn_(g_edit_and_continue_fn)
{
	g_edit_and_continue_fn = fn;
}

assert_edit_and_continue_fn_scope::~assert_edit_and_continue_fn_scope()
{
	g_edit_and_continue_fn = fn_;
}

extern std::map<std::string, variant> g_user_info_registry;

void report_assert_msg(const std::string& m)
{
	if(Level::getCurrentPtr()) {
		LOG_INFO("ATTEMPTING TO SEND CRASH REPORT...");
		std::map<variant,variant> obj;
		for(auto p : g_user_info_registry) {
			obj[variant(p.first)] = p.second;
		}
		obj[variant("type")] = variant("crash");
		obj[variant("msg")] = variant(m);
#ifndef NO_EDITOR
		obj[variant("editor")] = variant(editor::last_edited_level().empty() == false);
#else
		obj[variant("editor")] = variant(false);
#endif

		if(preferences::edit_and_continue()) {
			if(g_edit_and_continue_fn) {
				edit_and_continue_assert(m, g_edit_and_continue_fn);
				throw validation_failure_exception("edit and continue recover");
			} else {
				edit_and_continue_assert(m);
			}
		}

		stats::record(variant(&obj), Level::getCurrentPtr()->id());
	} else {
		std::map<variant,variant> obj;
		obj[variant("type")] = variant("crash");
		obj[variant("msg")] = variant(m);
		obj[variant("editor")] = variant(false);
		stats::record(variant(&obj), "nolevel");
	}

	stats::flush_and_quit();

#if defined(__ANDROID__)
	__android_log_print(ANDROID_LOG_INFO, "Frogatto", m.c_str());
#endif
	
	if(g_error_message_box) {
		std::stringstream ss;
		ss << "Assertion failed\n\n" << m;
		std::string assert_str = ss.str();
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Assertion Failed", trim_error_message(assert_str).c_str(), nullptr);
	}


#if defined(WIN32)
	if(IsDebuggerPresent()) {
		DebugBreak();
	}
#elif defined(__APPLE__)
    *((volatile int *) NULL) = 0;	/* To continue from here in GDB: "return" then "continue". */
    raise(SIGABRT);			/* In case above statement gets nixed by the optimizer. */
#else
    raise(SIGABRT);			/* To continue from here in GDB: "signal 0". */
#endif
}

namespace {
	int silence_on_assert = 0;
	int throw_validation_failure = 0;
	int throw_fatal = 0;
}

validation_failure_exception::validation_failure_exception(const std::string& m)
  : msg(m)
{
	if(!silence_on_assert) {
		LOG_ERROR("ASSERT FAIL: " << m);
		output_backtrace();
	}
}

fatal_assert_failure_exception::fatal_assert_failure_exception(const std::string& m)
  : msg(m)
{
	LOG_ERROR("ASSERT FAIL: " << m);
	output_backtrace();
}

bool throw_validation_failure_on_assert()
{
	return throw_validation_failure != 0 && !preferences::die_on_assert();
}

assert_recover_scope::assert_recover_scope(int options) : options_(options), fatal_(throw_fatal)
{
	throw_fatal = 0;
	if(options_&static_cast<int>(SilenceAsserts)) {
		silence_on_assert++;
	}
	throw_validation_failure++;
}

assert_recover_scope::~assert_recover_scope()
{
	throw_fatal = fatal_;
	if(options_&SilenceAsserts) {
		silence_on_assert--;
	}
	throw_validation_failure--;
}

#ifdef _MSC_VER
class StderrStackWalker : public StackWalker
{
public:
	StderrStackWalker() : StackWalker(RetrieveVerbose) {}
protected:
	virtual void OnOutput(LPCSTR szText)
	{
		SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s\n", szText);
		//StackWalker::OnOutput(szText);
	}
};
#endif

void output_backtrace()
{
	std::string call_stack = get_call_stack();
	SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s\n---\n", call_stack.c_str());
#ifdef _MSC_VER
	StderrStackWalker sw; 
	sw.ShowCallstack();
#else
// disable C++ stack traces for now.
//	const int nframes = 256;
//	void* trace_buffer[nframes];
//	const int nsymbols = backtrace(trace_buffer, nframes);
//	backtrace_symbols_fd(trace_buffer, nsymbols, 2);
	print_stacktrace(256);
#endif
	SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s\n", "---");
}

bool throw_fatal_error_on_assert()
{
	return throw_fatal != 0 && !preferences::die_on_assert();
}

fatal_assert_scope::fatal_assert_scope()
{
	throw_fatal++;
}

fatal_assert_scope::~fatal_assert_scope()
{
	throw_fatal--;
}
