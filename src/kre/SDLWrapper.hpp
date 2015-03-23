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

#include <exception>
#include <memory>
#include <string>
#include <sstream>

#include "SDL.h"

namespace SDL
{
	class init_error : public std::exception
	{
	public:
		init_error() : exception(), msg_(SDL_GetError())
		{}
		init_error(const std::string& msg) : exception(), msg_(msg)
		{}
		virtual ~init_error() throw()
		{}
		virtual const char* what() const throw() { return msg_.c_str(); }
	private:
		std::string msg_;
	};

	class SDL
	{
	public:
		SDL(Uint32 flags = SDL_INIT_VIDEO)
		{
			if (SDL_Init(flags) < 0) {
				std::stringstream ss;
				ss << "Unable to initialize SDL: " << SDL_GetError() << std::endl;
				throw init_error(ss.str());
			}
		}

		virtual ~SDL()
		{
			SDL_Quit();
		}
	};
	typedef std::shared_ptr<SDL> SDL_ptr;
}