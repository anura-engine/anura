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
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include "profile_timer.hpp"

#include "SurfaceBlur.hpp"

namespace KRE
{
	//
	// Copyright (c) 2009-2013 Mikko Mononen memon@inside.org
	//
	// This software is provided 'as-is', without any express or implied
	// warranty.  In no event will the authors be held liable for any damages
	// arising from the use of this software.
	// Permission is granted to anyone to use this software for any purpose,
	// including commercial applications, and to alter it and redistribute it
	// freely, subject to the following restrictions:
	// 1. The origin of this software must not be misrepresented; you must not
	//    claim that you wrote the original software. If you use this software
	//    in a product, an acknowledgment in the product documentation would be
	//    appreciated but is not required.
	// 2. Altered source versions must be plainly marked as such, and must not be
	//    misrepresented as being the original software.
	// 3. This notice may not be removed or altered from any source distribution.
	//
	
	// Based on Exponential blur, Jani Huhtanen, 2006
	
	namespace 
	{
		#define APREC 16
		#define ZPREC 7

		void blur_cols(unsigned char* dst, int w, int h, int stride, int alpha, int aoffs, int Bpp)
		{
			int x, y;
			for (y = 0; y < h; y++) {
				int z = 0; // force zero border
				for (x = 1; x < w; x++) {
					z += (alpha * (((int)(dst[x*Bpp+aoffs]) << ZPREC) - z)) >> APREC;
					dst[x*Bpp+aoffs] = (unsigned char)(z >> ZPREC);
				}
				dst[(w-1)*Bpp+aoffs] = 0; // force zero border
				z = 0;
				for (x = w-2; x >= 0; x--) {
					z += (alpha * (((int)(dst[x*Bpp+aoffs]) << ZPREC) - z)) >> APREC;
					dst[x*Bpp+aoffs] = (unsigned char)(z >> ZPREC);
				}
				dst[0+aoffs] = 0; // force zero border
				dst += stride;
			}
		}

		static void blur_rows(unsigned char* dst, int w, int h, int stride, int alpha, int aoffs, int Bpp)
		{
			int x, y;
			for (x = 0; x < w; x++) {
				int z = 0; // force zero border
				for (y = stride; y < h*stride; y += stride) {
					z += (alpha * (((int)(dst[y+aoffs]) << ZPREC) - z)) >> APREC;
					dst[y+aoffs] = (unsigned char)(z >> ZPREC);
				}
				dst[(h-1)*stride+aoffs] = 0; // force zero border
				z = 0;
				for (y = (h-2)*stride; y >= 0; y -= stride) {
					z += (alpha * (((int)(dst[y+aoffs]) << ZPREC) - z)) >> APREC;
					dst[y+aoffs] = (unsigned char)(z >> ZPREC);
				}
				dst[0+aoffs] = 0; // force zero border
				dst += Bpp;
			}
		}

	}

	void pixels_alpha_blur(void* pixels, int w, int h, int stride, float blur)
	{
		profile::manager pman("pixels_alpha_blur");
		if(blur < 1.0f || blur > 128.0f) {
			return;
		}
		const float sigma = blur * 0.57735f; // 1 / sqrt(3)
		const int alpha = static_cast<int>((1<<APREC) * (1.0f - expf(-2.3f / (sigma+1.0f))));
		uint8_t* dst = reinterpret_cast<uint8_t*>(pixels);
		
		blur_rows(dst, w, h, stride, alpha, 0, 1);
		blur_cols(dst, w, h, stride, alpha, 0, 1);
		blur_rows(dst, w, h, stride, alpha, 0, 1);
		blur_cols(dst, w, h, stride, alpha, 0, 1);
	}

	void surface_alpha_blur(const SurfacePtr& surface, float blur)
	{
		profile::manager pman("surface_alpha_blur");
		if(blur < 1.0f || blur > 128.0f) {
			return;
		}
		const float sigma = blur * 0.57735f; // 1 / sqrt(3)
		const int alpha = static_cast<int>((1<<APREC) * (1.0f - expf(-2.3f / (sigma+1.0f))));
		const int w = surface->width();
		const int h = surface->height();
		const int stride = surface->rowPitch();
		const int alpha_offset = surface->getPixelFormat()->getAlphaShift() / 8;
		const int Bpp = surface->getPixelFormat()->bytesPerPixel();
		uint8_t* dst = reinterpret_cast<uint8_t*>(surface->pixelsWriteable());
		
		blur_rows(dst, w, h, stride, alpha, alpha_offset, Bpp);
		blur_cols(dst, w, h, stride, alpha, alpha_offset, Bpp);
		blur_rows(dst, w, h, stride, alpha, alpha_offset, Bpp);
		blur_cols(dst, w, h, stride, alpha, alpha_offset, Bpp);
	}
}
