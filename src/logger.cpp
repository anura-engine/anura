/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "logger.hpp"

namespace
{
	const int max_log_packet_length = 3072;
}

void log_internal(SDL_LogPriority priority, const std::string& str)
{
	std::string s(str);
	// break up long strings into something about max_log_packet_length in size.
	while(s.size() > max_log_packet_length) {
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority, "%s\n", s.substr(0, max_log_packet_length).c_str());
		s = s.substr(max_log_packet_length+1);
	}
	if(!s.empty()) {
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority, "%s\n", s.c_str());
	}
}

/**
 * Helps the fact that a very large message can not be logged using
 * `SDL_LogMessage(4)`.
 */
void log_internal_wo_SDL(
		const SDL_LogPriority priority, const std::string & str)
{
	//   XXX Change for something more worked, such as `https://github.com/xeekworx/logger`.
	std::cout << str; /* XXX */
}
