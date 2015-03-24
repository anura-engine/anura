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

#define LOG_VERBOSE(_a)																\
	do {																			\
		std::ostringstream _s;														\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _a;				\
		std::string _ss = _s.str();													\
		SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "%s\n", _ss.c_str());			\
	} while(0)

#define LOG_INFO(_a)																\
	do {																			\
		std::ostringstream _s;														\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _a;				\
		std::string _ss = _s.str();													\
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s\n", _ss.c_str());				\
	} while(0)

#define LOG_DEBUG(_a)																\
	do {																			\
		std::ostringstream _s;														\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _a;				\
		std::string _ss = _s.str();													\
		SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s\n", _ss.c_str());			\
	} while(0)

#define LOG_WARN(_a)																\
	do {																			\
		std::ostringstream _s;														\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _a;				\
		std::string _ss = _s.str();													\
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s\n", _ss.c_str());				\
	} while(0)

#define LOG_ERROR(_a)																\
	do {																			\
		std::ostringstream _s;														\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _a;				\
		std::string _ss = _s.str();													\
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s\n", _ss.c_str());			\
	} while(0)


