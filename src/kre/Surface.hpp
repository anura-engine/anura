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

#include <cstdint>
#include <functional>
#include <memory>
#include <tuple>

#include "Geometry.hpp"
#include "WindowManagerFwd.hpp"

namespace KRE
{
	class PixelFormat
	{
	public:
		PixelFormat();
		virtual ~PixelFormat();

		virtual uint8_t BitsPerPixel() const = 0;
		virtual uint8_t BytesPerPixel() const = 0;

		virtual bool IsYuvPlanar() const = 0;
		virtual bool IsYuvPacked() const = 0;
		virtual bool YuvHeightReversed() const = 0;
		virtual bool IsInterlaced() const = 0;
		
		virtual bool IsRGB() const = 0;
		virtual bool HasRedChannel() const = 0;
		virtual bool HasGreenChannel() const = 0;
		virtual bool HasBlueChannel() const = 0;
		virtual bool HasAlphaChannel() const = 0;
		virtual bool HasLuminance() const = 0;

		virtual uint32_t RedMask() const = 0;
		virtual uint32_t GreenMask() const = 0;
		virtual uint32_t BlueMask() const = 0;
		virtual uint32_t AlphaMask() const = 0;
		virtual uint32_t LuminanceMask() const = 0;

		virtual uint32_t RedShift() const = 0;
		virtual uint32_t GreenShift() const = 0;
		virtual uint32_t BlueShift() const = 0;
		virtual uint32_t AlphaShift() const = 0;
		virtual uint32_t LuminanceShift() const = 0;

		virtual uint8_t RedBits() const = 0;
		virtual uint8_t GreenBits() const = 0;
		virtual uint8_t BlueBits() const = 0;
		virtual uint8_t AlphaBits() const = 0;
		virtual uint8_t LuminanceBits() const = 0;

		virtual bool HasPalette() const = 0;

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
		};
		virtual PF GetFormat() const = 0;

		virtual std::tuple<int,int> ExtractRGBA(const void* pixels, int ndx, uint32_t& red, uint32_t& green, uint32_t& blue, uint32_t& alpha) = 0;
		virtual void EncodeRGBA(void* pixels, uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha) = 0;
	private:
		PixelFormat(const PixelFormat&);
	};

	typedef std::shared_ptr<PixelFormat> PixelFormatPtr;

	typedef std::function<void(uint32_t&,uint32_t&,uint32_t&,uint32_t&)> SurfaceConvertFn;

	typedef std::function<SurfacePtr(const std::string&, PixelFormat::PF, SurfaceConvertFn)> SurfaceCreatorFn;

	class Surface
	{
	public:
		virtual ~Surface();
		virtual const void* Pixels() const = 0;
		virtual unsigned width() = 0;
		virtual unsigned height() = 0;
		virtual unsigned row_pitch() = 0;

		virtual void WritePixels(unsigned bpp, 
			uint32_t rmask, 
			uint32_t gmask, 
			uint32_t bmask, 
			uint32_t amask,
			const void* pixels) = 0;
		virtual void WritePixels(const void* pixels) = 0;

		PixelFormatPtr GetPixelFormat();

		virtual void Lock() = 0;
		virtual void Unlock() = 0;

		virtual bool HasData() const = 0;

		enum BlendMode {
			BLEND_MODE_NONE,
			BLEND_MODE_BLEND,
			BLEND_MODE_ADD,
			BLEND_MODE_MODULATE,
		};
		virtual void SetBlendMode(BlendMode bm) = 0;
		virtual BlendMode GetBlendMode() const = 0;

		virtual bool SetClipRect(int x, int y, unsigned width, unsigned height) = 0;
		virtual void GetClipRect(int& x, int& y, unsigned& width, unsigned& height) = 0;
		virtual bool SetClipRect(const rect& r) = 0;
		virtual const rect GetClipRect() = 0;
		SurfacePtr Convert(PixelFormat::PF fmt, SurfaceConvertFn convert=nullptr);

		static bool RegisterSurfaceCreator(const std::string& name, SurfaceCreatorFn fn);
		static void UnRegisterSurfaceCreator(const std::string& name);
		static SurfacePtr Create(const std::string& filename, bool no_cache=false, PixelFormat::PF fmt=PixelFormat::PF::PIXELFORMAT_UNKNOWN, SurfaceConvertFn convert=nullptr);
		static void ResetSurfaceCache();
	protected:
		Surface();
		void SetPixelFormat(PixelFormatPtr pf);
	private:
		virtual SurfacePtr HandleConvert(PixelFormat::PF fmt, SurfaceConvertFn convert) = 0;
		PixelFormatPtr pf_;
	};

	class SurfaceLock
	{
	public:
		SurfaceLock(const SurfacePtr& surface);
		~SurfaceLock();
	private:
		SurfacePtr surface_;
	};
}
