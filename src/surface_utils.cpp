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

#include "surface_cache.hpp"
#include "surface_utils.hpp"

namespace graphics
{
	const unsigned char* get_alpha_pixel_colors()
	{
		using namespace KRE;
		auto s = SurfaceCache::get("alpha-colors.png", false);
		ASSERT_LOG(s != NULL, "COULD NOT LOAD alpha.png");

		const int npixels = s->width() * s->height();
		ASSERT_LOG(npixels == 2, "UNEXPECTED SIZE FOR alpha.png");

		static unsigned char color[6];

		const unsigned char* pixel = reinterpret_cast<const unsigned char*>(s->pixels());
		memcpy(color, pixel, 3);
		pixel += 4;
		memcpy(color+3, pixel, 3);
		return color;
	}

	void set_alpha_for_transparent_colors_in_rgba_surface(KRE::SurfacePtr s, SpritesheetOptions options)
	{
		const bool strip_red_rects = !(options & SpritesheetOptions::NO_STRIP_ANNOTATIONS);

		const int npixels = s->width() * s->height();
		static const unsigned char* AlphaColors = get_alpha_pixel_colors();
		KRE::SurfaceLock lck(s);
		for(int n = 0; n != npixels; ++n) {
			//we use a color in our sprite sheets to indicate transparency, rather than an alpha channel
			static const unsigned char* AlphaPixel = AlphaColors; //the background color, brown
			static const unsigned char* AlphaPixel2 = AlphaColors+3; //the border color, red
			unsigned char* pixel = reinterpret_cast<unsigned char*>(s->pixelsWriteable()) + n*4;

			if(pixel[0] == AlphaPixel[0] && pixel[1] == AlphaPixel[1] && pixel[2] == AlphaPixel[2] ||
			   strip_red_rects &&
			   pixel[0] == AlphaPixel2[0] && pixel[1] == AlphaPixel2[1] && pixel[2] == AlphaPixel2[2]) {
				pixel[3] = 0;
			}
		}
	}

	namespace
	{
		const unsigned char table_8bits_to_4bits[256] = {
			 0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  
			 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  
			 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  
			 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  
			 4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  5,  5,  5,  
			 5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  6,  6,  
			 6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  7,  
			 7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  
			 8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  
			 8,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  
			 9,  9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 
			10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 
			11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 
			12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 
			13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
			14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
		};
	}

	unsigned long map_color_to_16bpp(unsigned long color)
	{
		return table_8bits_to_4bits[(color >> 24)&0xFF] << 28 |
			   table_8bits_to_4bits[(color >> 24)&0xFF] << 24 |
			   table_8bits_to_4bits[(color >> 16)&0xFF] << 20 |
			   table_8bits_to_4bits[(color >> 16)&0xFF] << 16 |
			   table_8bits_to_4bits[(color >>  8)&0xFF] << 12 |
			   table_8bits_to_4bits[(color >>  8)&0xFF] << 8 |
			   table_8bits_to_4bits[(color >>  0)&0xFF] << 4 |
			   table_8bits_to_4bits[(color >>  0)&0xFF] << 0;	
	}
}
