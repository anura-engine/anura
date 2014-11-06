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
#ifndef SURFACE_HPP_INCLUDED
#define SURFACE_HPP_INCLUDED

#include <iostream>

#include "graphics.hpp"
#include "scoped_resource.hpp"

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define SURFACE_MASK 0xFF,0xFF00,0xFF0000,0xFF000000
#define SURFACE_MASK_RGB 0xFF,0xFF00,0xFF0000,0x0
#else
#define SURFACE_MASK 0xFF000000,0xFF0000,0xFF00,0xFF
#define SURFACE_MASK_RGB 0xFF0000,0xFF00,0xFF,0x0
#endif

namespace graphics
{

struct surface
{
private:
	static void sdl_add_ref(SDL_Surface *surf)
	{
		if (surf != NULL)
			++surf->refcount;
	}

	struct free_sdl_surface {
		void operator()(SDL_Surface *surf) const
		{
			if (surf != NULL) {
				 SDL_FreeSurface(surf);
			}
		}
	};

	typedef util::scoped_resource<SDL_Surface*,free_sdl_surface> scoped_sdl_surface;
public:
	surface() : surface_(NULL)
	{}

	surface(SDL_Surface *surf) : surface_(surf)
	{
	}

	surface(const surface& o) : surface_(o.surface_.get())
	{
		sdl_add_ref(surface_.get());
	}

	static surface create(int w, int h);

	void assign(const surface& o)
	{
		SDL_Surface *surf = o.surface_.get();
		sdl_add_ref(surf); // need to be done before assign to avoid corruption on "a=a;"
		surface_.assign(surf);
	}

	surface& operator=(const surface& o)
	{
		assign(o);
		return *this;
	}

	operator SDL_Surface*() const { return surface_.get(); }

	SDL_Surface* get() const { return surface_.get(); }

	SDL_Surface* operator->() const { return surface_.get(); }

	void assign(SDL_Surface* surf) { surface_.assign(surf); }

	bool null() const { return surface_.get() == NULL; }

	surface convert_opengl_format() const;
	surface clone() const;

private:
	scoped_sdl_surface surface_;
};

inline bool operator==(const surface& a, const surface& b)
{
	return a.get() == b.get();
}

inline bool operator<(const surface& a, const surface& b)
{
	return a.get() < b.get();
}

}

#endif
