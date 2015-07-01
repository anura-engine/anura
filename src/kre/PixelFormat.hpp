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

#pragma once

#include <cstdint>
#include "Color.hpp"

namespace KRE
{
	class PixelFormat
	{
	public:
		PixelFormat();
		virtual ~PixelFormat();

		virtual uint8_t bitsPerPixel() const = 0;
		virtual uint8_t bytesPerPixel() const = 0;

		virtual bool isYuvPlanar() const = 0;
		virtual bool isYuvPacked() const = 0;
		virtual bool isYuvHeightReversed() const = 0;
		virtual bool isInterlaced() const = 0;
		
		virtual bool isRGB() const = 0;
		virtual bool hasRedChannel() const = 0;
		virtual bool hasGreenChannel() const = 0;
		virtual bool hasBlueChannel() const = 0;
		virtual bool hasAlphaChannel() const = 0;
		virtual bool hasLuminance() const = 0;

		virtual uint32_t getRedMask() const = 0;
		virtual uint32_t getGreenMask() const = 0;
		virtual uint32_t getBlueMask() const = 0;
		virtual uint32_t getAlphaMask() const = 0;
		virtual uint32_t getLuminanceMask() const = 0;

		virtual uint32_t getRedShift() const = 0;
		virtual uint32_t getGreenShift() const = 0;
		virtual uint32_t getBlueShift() const = 0;
		virtual uint32_t getAlphaShift() const = 0;
		virtual uint32_t getLuminanceShift() const = 0;

		virtual uint32_t getRedLoss() const = 0;
		virtual uint32_t getGreenLoss() const = 0;
		virtual uint32_t getBlueLoss() const = 0;
		virtual uint32_t getAlphaLoss() const = 0;
		virtual uint32_t getLuminanceLoss() const = 0;

		virtual uint8_t getRedBits() const = 0;
		virtual uint8_t getGreenBits() const = 0;
		virtual uint8_t getBlueBits() const = 0;
		virtual uint8_t getAlphaBits() const = 0;
		virtual uint8_t getLuminanceBits() const = 0;

		virtual bool hasPalette() const = 0;

		enum class PF {
			PIXELFORMAT_UNKNOWN,
			PIXELFORMAT_INDEX1LSB,
			PIXELFORMAT_INDEX1MSB,
			PIXELFORMAT_INDEX4LSB,
			PIXELFORMAT_INDEX4MSB,
			PIXELFORMAT_INDEX8,
			PIXELFORMAT_RGB332,
			PIXELFORMAT_RGB444,
			PIXELFORMAT_RGB555,
			PIXELFORMAT_BGR555,
			PIXELFORMAT_ARGB4444,
			PIXELFORMAT_RGBA4444,
			PIXELFORMAT_ABGR4444,
			PIXELFORMAT_BGRA4444,
			PIXELFORMAT_ARGB1555,
			PIXELFORMAT_RGBA5551,
			PIXELFORMAT_ABGR1555,
			PIXELFORMAT_BGRA5551,
			PIXELFORMAT_RGB565,
			PIXELFORMAT_BGR565,
			PIXELFORMAT_RGB24,
			PIXELFORMAT_BGR24,
			PIXELFORMAT_RGB888,
			PIXELFORMAT_RGBX8888,
			PIXELFORMAT_BGR888,
			PIXELFORMAT_BGRX8888,
			PIXELFORMAT_ARGB8888,
			PIXELFORMAT_XRGB8888,
			PIXELFORMAT_RGBA8888,
			PIXELFORMAT_ABGR8888,
			PIXELFORMAT_BGRA8888,
			PIXELFORMAT_RGB101010,
			PIXELFORMAT_ARGB2101010,
			PIXELFORMAT_YV12,
			PIXELFORMAT_IYUV,
			PIXELFORMAT_YUY2,
			PIXELFORMAT_UYVY,
			PIXELFORMAT_YVYU,
			PIXELFORMAT_R8,
		};
		virtual PF getFormat() const = 0;

		virtual Color mapRGB(int r, int g, int b) = 0;
		virtual Color mapRGB(float r, float g, float b) = 0;
		virtual Color mapRGBA(int r, int g, int b, int a) = 0;
		virtual Color mapRGBA(float r, float g, float b, float a) = 0;

		virtual void getRGBA(uint32_t pix, int& r, int& g, int& b, int& a) = 0; 

		virtual void extractRGBA(const void* pixels, int ndx, int& red, int& green, int& blue, int& alpha) = 0;
		virtual void encodeRGBA(void* pixels, int red, int green, int blue, int alpha) = 0;
		
		static bool isIndexedFormat(PixelFormat::PF pf);
	private:
		PixelFormat(const PixelFormat&);
	};

	typedef std::shared_ptr<PixelFormat> PixelFormatPtr;
}
