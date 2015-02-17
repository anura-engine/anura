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
#include <functional>
#include <memory>
#include <tuple>

#include "geometry.hpp"
#include "PixelFormat.hpp"
#include "WindowManagerFwd.hpp"

namespace KRE
{
	struct ImageLoadError
	{
	};

	typedef std::function<void(uint32_t&,uint32_t&,uint32_t&,uint32_t&)> SurfaceConvertFn;

	typedef std::function<SurfacePtr(const std::string&, PixelFormat::PF, SurfaceConvertFn)> SurfaceCreatorFileFn;
	typedef std::function<SurfacePtr(int, int, int, int, uint32_t, uint32_t, uint32_t, uint32_t, const void*)> SurfaceCreatorPixelsFn;
	typedef std::function<SurfacePtr(int, int, int, uint32_t, uint32_t, uint32_t, uint32_t)> SurfaceCreatorMaskFn;
	typedef std::function<SurfacePtr(int, int, PixelFormat::PF)> SurfaceCreatorFormatFn;

	enum class ColorCountFlags {
		NONE						= 0,
		IGNORE_ALPHA_VARIATIONS		= 1,
	};

	inline bool operator&(ColorCountFlags lhs, ColorCountFlags rhs)
	{
		return (static_cast<int>(lhs) & static_cast<int>(rhs)) != 0;
	}

	typedef std::function<std::string(const std::string&)> file_filter;

	enum class FileFilterType {
		LOAD,
		SAVE,
	};

	class Surface
	{
	public:
		virtual ~Surface();
		virtual const void* pixels() const = 0;
		// This is a potentially dangerous function and significant care must
		// be taken when processing the pixel data to respect correct row pitch
		// and pixel format.
		virtual void* pixelsWriteable() = 0;
		virtual int width() const = 0;
		virtual int height() const = 0;
		virtual int rowPitch() const = 0;

		virtual void blit(SurfacePtr src, const rect& src_rect) = 0;
		virtual void blitTo(SurfacePtr src, const rect& src_rect, const rect& dst_rect) = 0;
		virtual void blitTo(SurfacePtr src, const rect& dst_rect) = 0;
		virtual void blitToScaled(SurfacePtr src, const rect& src_rect, const rect& dst_rect) = 0;

		virtual void writePixels(int bpp, 
			uint32_t rmask, 
			uint32_t gmask, 
			uint32_t bmask, 
			uint32_t amask,
			const void* pixels) = 0;
		virtual void writePixels(const void* pixels) = 0;

		virtual void fillRect(const rect& dst_rect, const Color& color);

		PixelFormatPtr getPixelFormat();

		virtual void lock() = 0;
		virtual void unlock() = 0;

		virtual bool hasData() const = 0;

		static void clearSurfaceCache();

		virtual void savePng(const std::string& filename) = 0;

		enum BlendMode {
			BLEND_MODE_NONE,
			BLEND_MODE_BLEND,
			BLEND_MODE_ADD,
			BLEND_MODE_MODULATE,
		};
		virtual void setBlendMode(BlendMode bm) = 0;
		virtual BlendMode getBlendMode() const = 0;

		virtual bool setClipRect(int x, int y, unsigned width, unsigned height) = 0;
		virtual void getClipRect(int& x, int& y, unsigned& width, unsigned& height) = 0;
		virtual bool setClipRect(const rect& r) = 0;
		virtual const rect getClipRect() = 0;
		SurfacePtr convert(PixelFormat::PF fmt, SurfaceConvertFn convert=nullptr);
		void convertInPlace(PixelFormat::PF fmt, SurfaceConvertFn convert=nullptr);

		unsigned getColorCount(ColorCountFlags flags=ColorCountFlags::NONE);

		static bool registerSurfaceCreator(const std::string& name, 
			SurfaceCreatorFileFn file_fn, 
			SurfaceCreatorPixelsFn pixels_fn, 
			SurfaceCreatorMaskFn mask_fn,
			SurfaceCreatorFormatFn format_fn);
		static void unRegisterSurfaceCreator(const std::string& name);
		static SurfacePtr create(const std::string& filename, bool no_cache=false, PixelFormat::PF fmt=PixelFormat::PF::PIXELFORMAT_UNKNOWN, SurfaceConvertFn convert=nullptr);
		static SurfacePtr create(int width, 
			int height, 
			int bpp, 
			int row_pitch, 
			uint32_t rmask, 
			uint32_t gmask, 
			uint32_t bmask, 
			uint32_t amask, 
			const void* pixels);
		static SurfacePtr create(int width, 
			int height, 
			int bpp, 
			uint32_t rmask, 
			uint32_t gmask, 
			uint32_t bmask, 
			uint32_t amask);
		static SurfacePtr create(int width, int height, PixelFormat::PF fmt);

		static void resetSurfaceCache();

		static void setFileFilter(FileFilterType type, file_filter fn);
		static file_filter getFileFilter(FileFilterType type);
	protected:
		Surface();
		void setPixelFormat(PixelFormatPtr pf);
	private:
		virtual SurfacePtr handleConvert(PixelFormat::PF fmt, SurfaceConvertFn convert) = 0;
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
