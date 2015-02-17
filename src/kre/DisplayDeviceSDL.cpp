/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "DisplayDeviceSDL.hpp"

namespace KRE
{
	namespace
	{
		//DisplayDeviceRegistrar<DisplayDeviceSDL> ogl_register("SDL");
	}
}

/*
	XXX this code goes in the call to init().

			Uint32 rnd_flags = SDL_RENDERER_ACCELERATED;
			if(vsync()) {
				rnd_flags |= SDL_RENDERER_PRESENTVSYNC;
			}
			if(renderer_hint_.size() > 4 && renderer_hint_.substr(0,4) == "sdl:") {
				SDL_SetHint(SDL_HINT_RENDER_DRIVER, renderer_hint_.substr(5).c_str());
			}
			renderer_ = SDL_CreateRenderer(window_.get(), -1, rnd_flags);
			ASSERT_LOG(renderer_ != nullptr, "Failed to create renderer: " << SDL_GetError());

	XXX in swap()
		SDL_RenderPresent(renderer_);

	XXX in ~DisplayDeviceSDL()
		SDL_DestroyRenderer(renderer_);
*/