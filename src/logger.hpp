/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <sstream>
#include <cstring>

#include "SDL.h"

#if defined(_MSC_VER)
#define __SHORT_FORM_OF_FILE__	\
	(strrchr(__FILE__,'\\')		\
	? strrchr(__FILE__,'\\')+1	\
	: __FILE__					\
	)
#else
#define __SHORT_FORM_OF_FILE__	\
	(strrchr(__FILE__,'/')		\
	? strrchr(__FILE__,'/')+1	\
	: __FILE__					\
	)
#endif

void log_internal(SDL_LogPriority priority, const std::string& s);

/**
 * A variant of `log_internal(2)` that will make more efforts in logging
 * the message in a single atomic logging operation. This might bring
 * greater readability at the costs of less performance, potentially
 * riskier operation, and even potentially less readability.
 *
 * @see `validation_failure_exception::validation_failure_exception`.
 */
void log_internal_single_dispatch(
		SDL_LogPriority priority, const std::string & s);

#define LOG_VERBOSE(_a)																\
	do {																			\
		std::ostringstream _s;														\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _a;				\
		log_internal(SDL_LOG_PRIORITY_VERBOSE, _s.str());							\
	} while(0)

#define LOG_INFO(_a)																\
	do {																			\
		std::ostringstream _s;														\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _a;				\
		log_internal(SDL_LOG_PRIORITY_INFO, _s.str());								\
	} while(0)

#define LOG_DEBUG(_a)																\
	do {																			\
		std::ostringstream _s;														\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _a;				\
		log_internal(SDL_LOG_PRIORITY_DEBUG, _s.str());								\
	} while(0)

#define LOG_WARN(_a)																\
	do {																			\
		std::ostringstream _s;														\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _a;				\
		log_internal(SDL_LOG_PRIORITY_WARN, _s.str());								\
	} while(0)

#define LOG_ERROR(_a)																\
	do {																			\
		std::ostringstream _s;														\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _a;				\
		log_internal(SDL_LOG_PRIORITY_ERROR, _s.str());								\
	} while(0)

/**
 * A variant of `LOG_ERROR(1)` that will make more efforts in logging
 * the message in a single atomic logging operation. This might bring
 * greater readability at the costs of less performance, potentially
 * riskier operation, and even potentially less readability.
 *
 * @see `validation_failure_exception::validation_failure_exception`.
 */
#define LOG_ERROR_SINGLE_DISPATCH(_a) do {                           \
	std::ostringstream _s;                                        \
	_s << __SHORT_FORM_OF_FILE__ << ':' << __LINE__ << " : " << _a;\
	log_internal_single_dispatch(                                   \
			SDL_LOG_PRIORITY_CRITICAL, _s.str());            \
	} while (false)

#define LOG_CRITICAL(_a)															\
	do {																			\
		std::ostringstream _s;														\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _a;				\
		log_internal(SDL_LOG_PRIORITY_CRITICAL, _s.str());							\
	} while(0)

