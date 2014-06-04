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

#include <iostream>
#include <sstream>
#include <cstring>

enum LogLevel
{
	LOG_LEVEL_DEBUG, 
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARN,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_FATAL,
};

const char* get_log_level_as_string(LogLevel l);

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

#define LOG_MSG(_ll, _msg)													\
	do {																	\
			std::ostringstream _s;											\
			_s  << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " "			\
				<< get_log_level_as_string(_ll) << ": " << _msg << "\n";	\
			std::cerr << _s.str();											\
	} while(0)

#define LOG_DEBUG(_msg)	LOG_MSG(LOG_LEVEL_DEBUG, _msg)
#define LOG_INFO(_msg)	LOG_MSG(LOG_LEVEL_INFO, _msg)
#define LOG_WARN(_msg)	LOG_MSG(LOG_LEVEL_WARN, _msg)
#define LOG_ERROR(_msg) LOG_MSG(LOG_LEVEL_ERROR, _msg)
