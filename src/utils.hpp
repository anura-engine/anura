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

#if defined(_MSC_VER)
#include "winsock2.h"
#endif

std::string get_http_datetime();
int truncate_to_char(int value);
void write_autosave();
void toggle_fullscreen();

#if defined(_MSC_VER)
int gettimeofday(struct timeval *tv, struct timezone2 *tz);
#endif

namespace util {
template<typename T>
T clamp(T value, T minval, T maxval)
{
	return std::min<T>(maxval, std::max(value, minval));
}

template<typename T, typename R>
T mix(T a, T b, R ratio)
{
	R inv = R(1.0) - ratio;
	return a*inv + b*ratio;
}

}
