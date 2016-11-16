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

#include <functional>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>

#include "logger.hpp"

void report_assert_msg(const std::string& m );

//An exception we intend to recover from.
struct validation_failure_exception 
{
	explicit validation_failure_exception(const std::string& m);
	std::string msg;
};

//If we should try to recover on asserts
bool throw_validation_failure_on_assert();

void output_backtrace();

class assert_edit_and_continue_fn_scope 
{
	std::function<void()> fn_;
public:
	assert_edit_and_continue_fn_scope(std::function<void()> fn);
	~assert_edit_and_continue_fn_scope();
};

enum AssertOptions { SilenceAsserts = 1 };

//Scope to make us recover
class assert_recover_scope 
{
	int options_;
	int fatal_;
public:
	assert_recover_scope(int options=0);
	~assert_recover_scope();
};

//An exception we intend to die from, but at a location we'll have better
//error reporting from.
struct fatal_assert_failure_exception 
{
	explicit fatal_assert_failure_exception(const std::string& m);
	std::string msg;
};

//If we should try to recover on asserts
bool throw_fatal_error_on_assert();

//Scope to make us throw a fatal error on assert (as opposed to throwing
//a recoverable error, or just dying on the spot).
class fatal_assert_scope 
{
public:
	fatal_assert_scope();
	~fatal_assert_scope();
};

#define ABORT()		do { exit(1); } while(0)

//various asserts of standard "equality" tests, such as "equals", "not equals", "greater than", etc.  Example usage:
//ASSERT_NE(x, y);

#define ASSERT_EQ(a,b) if((a) != (b)) do { std::ostringstream _s; _s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " ASSERT EQ FAILED: " << #a << " != " << #b << ": " << (a) << " != " << (b) << "\n"; if(throw_validation_failure_on_assert()) { throw validation_failure_exception(_s.str()); } else if(throw_fatal_error_on_assert()) { throw fatal_assert_failure_exception(_s.str()); } else { log_internal(SDL_LOG_PRIORITY_CRITICAL, _s.str()); output_backtrace(); report_assert_msg(_s.str()); ABORT(); } } while(0)

#define ASSERT_NE(a,b) if((a) == (b)) do { std::ostringstream _s; _s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " ASSERT NE FAILED: " << #a << " == " << #b << ": " << (a) << " == " << (b) << "\n"; if(throw_validation_failure_on_assert()) { throw validation_failure_exception(_s.str()); } else if(throw_fatal_error_on_assert()) { throw fatal_assert_failure_exception(_s.str()); } else { log_internal(SDL_LOG_PRIORITY_CRITICAL, _s.str()); output_backtrace(); report_assert_msg(_s.str()); ABORT(); } } while(0)

#define ASSERT_GE(a,b) if((a) < (b)) do { std::ostringstream _s; _s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " ASSERT GE FAILED: " << #a << " < " << #b << ": " << (a) << " < " << (b) << "\n"; if(throw_validation_failure_on_assert()) { throw validation_failure_exception(_s.str()); } else if(throw_fatal_error_on_assert()) { throw fatal_assert_failure_exception(_s.str()); } else { log_internal(SDL_LOG_PRIORITY_CRITICAL, _s.str()); output_backtrace(); report_assert_msg(_s.str()); ABORT(); } } while(0)

#define ASSERT_LE(a,b) if((a) > (b)) do { std::ostringstream _s; _s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " ASSERT LE FAILED: " << #a << " > " << #b << ": " << (a) << " > " << (b) << "\n"; if(throw_validation_failure_on_assert()) { throw validation_failure_exception(_s.str()); } else if(throw_fatal_error_on_assert()) { throw fatal_assert_failure_exception(_s.str()); } else { log_internal(SDL_LOG_PRIORITY_CRITICAL, _s.str()); output_backtrace(); report_assert_msg(_s.str()); ABORT(); } } while(0)

#define ASSERT_GT(a,b) if((a) <= (b)) do { std::ostringstream _s; _s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " ASSERT GT FAILED: " << #a << " <= " << #b << ": " << (a) << " <= " << (b) << "\n"; if(throw_validation_failure_on_assert()) { throw validation_failure_exception(_s.str()); } else if(throw_fatal_error_on_assert()) { throw fatal_assert_failure_exception(_s.str()); } else { log_internal(SDL_LOG_PRIORITY_CRITICAL, _s.str()); output_backtrace(); report_assert_msg(_s.str()); ABORT(); } } while(0)

#define ASSERT_LT(a,b) if((a) >= (b)) do { std::ostringstream _s; _s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " ASSERT LT FAILED: " << #a << " >= " << #b << ": " << (a) << " >= " << (b) << "\n"; if(throw_validation_failure_on_assert()) { throw validation_failure_exception(_s.str()); } else if(throw_fatal_error_on_assert()) { throw fatal_assert_failure_exception(_s.str()); } else { log_internal(SDL_LOG_PRIORITY_CRITICAL, _s.str()); output_backtrace(); report_assert_msg(_s.str()); ABORT(); } } while(0)

#define ASSERT_INDEX_INTO_VECTOR(a,b) do { if((a) < 0 || size_t(a) >= (b).size()) { std::ostringstream _s; _s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " ASSERT INDEX INTO VECTOR FAILED: " << #a << " (" << (a) << " indexes " << #b << " (" << (b).size() << ")\n"; if(throw_validation_failure_on_assert()) { throw validation_failure_exception(_s.str()); } else if(throw_fatal_error_on_assert()) { throw fatal_assert_failure_exception(_s.str()); } else { log_internal(SDL_LOG_PRIORITY_CRITICAL, _s.str()); output_backtrace(); report_assert_msg(_s.str()); ABORT(); } } } while(0)

//for custom logging.  Example usage:
//ASSERT_LOG(x != y, "x not equal to y. Value of x: " << x << ", y: " << y);

#define ASSERT_LOG(_a,_b)										\
	do { if( !(_a) ) {											\
		std::ostringstream _s;									\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " ASSERTION FAILED: " << _b << "\n";	\
		if(throw_validation_failure_on_assert()) {				\
			throw validation_failure_exception(_s.str());		\
		} else if(throw_fatal_error_on_assert()) {				\
			throw fatal_assert_failure_exception(_s.str());		\
		} else {												\
			log_internal(SDL_LOG_PRIORITY_CRITICAL, _s.str());	\
			output_backtrace();									\
			report_assert_msg(_s.str());						\
			ABORT();											\
		}														\
	} } while(0)

#define ASSERT_FATAL(_b)										\
	do {														\
		std::ostringstream _s;									\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " ASSERTION FAILED: " << _b << "\n"; \
		if(throw_validation_failure_on_assert()) {				\
			throw validation_failure_exception(_s.str());		\
		} else {												\
			log_internal(SDL_LOG_PRIORITY_CRITICAL, _s.str());	\
			output_backtrace();									\
			report_assert_msg(_s.str());						\
			ABORT();											\
		}														\
	} while(0)

#define VALIDATE_LOG(a,b)										\
	do{ if(!(a)) {												\
		std::ostringstream s;									\
		s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " VALIDATION FAILED: " << b << "\n"; \
		throw validation_failure_exception(s.str());			\
	} } while(0)
