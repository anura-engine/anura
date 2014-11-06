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
#include <stdio.h>

#ifdef _MSC_VER
#include "StackWalker.h"
#else
#include <execinfo.h> //for backtrace. May not be available on some platforms.
                      //If not available implement output_backtrace() some
					  //other way.
#endif

#ifndef NO_EDITOR
#include "editor.hpp"
#endif

#include "asserts.hpp"
#include "code_editor_dialog.hpp"
#include "level.hpp"
#include "preferences.hpp"
#include "stats.hpp"
#include "variant.hpp"

#if defined(_WINDOWS)
#include "SDL_syswm.h"
#endif

namespace {
boost::function<void()> g_edit_and_continue_fn;
}

void set_assert_edit_and_continue_fn(boost::function<void()> fn)
{
	g_edit_and_continue_fn = fn;
}

assert_edit_and_continue_fn_scope::assert_edit_and_continue_fn_scope(boost::function<void()> fn)
  : fn_(g_edit_and_continue_fn)
{
	g_edit_and_continue_fn = fn;
}

assert_edit_and_continue_fn_scope::~assert_edit_and_continue_fn_scope()
{
	g_edit_and_continue_fn = fn_;
}

void report_assert_msg(const std::string& m)
{
	if(level::current_ptr()) {
		std::cerr << "ATTEMPTING TO SEND CRASH REPORT...\n";
		std::map<variant,variant> obj;
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

		stats::record(variant(&obj), level::current_ptr()->id());
		stats::flush_and_quit();
	}

#if defined(__native_client__)
	std::cerr << m;
#endif

#if defined(__ANDROID__)
	__android_log_print(ANDROID_LOG_INFO, "Frogatto", m.c_str());

#endif
	
#if defined(_WINDOWS)
	::MessageBoxA(NULL, m.c_str(), "Assertion failed", MB_OK|MB_ICONSTOP);
#endif
}

validation_failure_exception::validation_failure_exception(const std::string& m)
  : msg(m)
{
	std::cerr << "ASSERT FAIL: " << m << "\n";
	output_backtrace();
}

fatal_assert_failure_exception::fatal_assert_failure_exception(const std::string& m)
  : msg(m)
{
	std::cerr << "ASSERT FAIL: " << m << "\n";
	output_backtrace();
}

namespace {
	int throw_validation_failure = 0;
	int throw_fatal = 0;
}

bool throw_validation_failure_on_assert()
{
	return throw_validation_failure != 0 && !preferences::die_on_assert();
}

assert_recover_scope::assert_recover_scope()
{
	throw_validation_failure++;
}

assert_recover_scope::~assert_recover_scope()
{
	throw_validation_failure--;
}

#ifdef _MSC_VER
class StderrStackWalker : public StackWalker
{
public:
	StderrStackWalker() : StackWalker() {}
protected:
	virtual void OnOutput(LPCSTR szText)
	{
		fprintf(stderr, "%s", szText);
		StackWalker::OnOutput(szText);
	}
};
#endif

void output_backtrace()
{
	std::cerr << get_call_stack() << "\n";

	std::cerr << "---\n";
#ifdef _MSC_VER
	StderrStackWalker sw; 
	sw.ShowCallstack();
#else
// disable C++ stack traces for now.
//	const int nframes = 256;
//	void* trace_buffer[nframes];
//	const int nsymbols = backtrace(trace_buffer, nframes);
//	backtrace_symbols_fd(trace_buffer, nsymbols, 2);
#endif
	std::cerr << "---\n";
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
