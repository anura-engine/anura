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

#include "SDL_image.h"

#include "../asserts.hpp"
#include "SurfaceSDL.hpp"

namespace KRE
{
	namespace
	{
		bool can_create_surfaces = Surface::RegisterSurfaceCreator("sdl", SurfaceSDL::CreateFromFile);

		Uint32 get_sdl_pixel_format(PixelFormat::PF fmt)
		{
			switch(fmt) {
				case PixelFormat::PF::PIXELFORMAT_INDEX1LSB:	    return SDL_PIXELFORMAT_INDEX1LSB;
				case PixelFormat::PF::PIXELFORMAT_INDEX1MSB:	    return SDL_PIXELFORMAT_INDEX1MSB;
				case PixelFormat::PF::PIXELFORMAT_INDEX4LSB:	    return SDL_PIXELFORMAT_INDEX4LSB;
				case PixelFormat::PF::PIXELFORMAT_INDEX4MSB:	    return SDL_PIXELFORMAT_INDEX4MSB;
				case PixelFormat::PF::PIXELFORMAT_INDEX8:	    return SDL_PIXELFORMAT_INDEX8;
				case PixelFormat::PF::PIXELFORMAT_RGB332:	    return SDL_PIXELFORMAT_RGB332;
				case PixelFormat::PF::PIXELFORMAT_RGB444:	    return SDL_PIXELFORMAT_RGB444;
				case PixelFormat::PF::PIXELFORMAT_RGB555:	    return SDL_PIXELFORMAT_RGB555;
				case PixelFormat::PF::PIXELFORMAT_BGR555:	    return SDL_PIXELFORMAT_BGR555;
				case PixelFormat::PF::PIXELFORMAT_ARGB4444:	    return SDL_PIXELFORMAT_ARGB4444;
				case PixelFormat::PF::PIXELFORMAT_RGBA4444:	    return SDL_PIXELFORMAT_RGBA4444;
				case PixelFormat::PF::PIXELFORMAT_ABGR4444:	    return SDL_PIXELFORMAT_ABGR4444;
				case PixelFormat::PF::PIXELFORMAT_BGRA4444:	    return SDL_PIXELFORMAT_BGRA4444;
				case PixelFormat::PF::PIXELFORMAT_ARGB1555:	    return SDL_PIXELFORMAT_ARGB1555;
				case PixelFormat::PF::PIXELFORMAT_RGBA5551:	    return SDL_PIXELFORMAT_RGBA5551;
				case PixelFormat::PF::PIXELFORMAT_ABGR1555:	    return SDL_PIXELFORMAT_ABGR1555;
				case PixelFormat::PF::PIXELFORMAT_BGRA5551:	    return SDL_PIXELFORMAT_BGRA5551;
				case PixelFormat::PF::PIXELFORMAT_RGB565:	    return SDL_PIXELFORMAT_RGB565;
				case PixelFormat::PF::PIXELFORMAT_BGR565:	    return SDL_PIXELFORMAT_BGR565;
				case PixelFormat::PF::PIXELFORMAT_RGB24:	        return SDL_PIXELFORMAT_RGB24;
				case PixelFormat::PF::PIXELFORMAT_BGR24:	        return SDL_PIXELFORMAT_BGR24;
				case PixelFormat::PF::PIXELFORMAT_RGB888:	    return SDL_PIXELFORMAT_RGB888;
				case PixelFormat::PF::PIXELFORMAT_RGBX8888:	    return SDL_PIXELFORMAT_RGBX8888;
				case PixelFormat::PF::PIXELFORMAT_BGR888:	    return SDL_PIXELFORMAT_BGR888;
				case PixelFormat::PF::PIXELFORMAT_BGRX8888:	    return SDL_PIXELFORMAT_BGRX8888;
				case PixelFormat::PF::PIXELFORMAT_ARGB8888:	    return SDL_PIXELFORMAT_ARGB8888;
				case PixelFormat::PF::PIXELFORMAT_RGBA8888:	    return SDL_PIXELFORMAT_RGBA8888;
				case PixelFormat::PF::PIXELFORMAT_ABGR8888:	    return SDL_PIXELFORMAT_ABGR8888;
				case PixelFormat::PF::PIXELFORMAT_BGRA8888:	    return SDL_PIXELFORMAT_BGRA8888;
				case PixelFormat::PF::PIXELFORMAT_ARGB2101010:	return SDL_PIXELFORMAT_ARGB2101010;
				case PixelFormat::PF::PIXELFORMAT_YV12:	        return SDL_PIXELFORMAT_YV12;
				case PixelFormat::PF::PIXELFORMAT_IYUV:	        return SDL_PIXELFORMAT_IYUV;
				case PixelFormat::PF::PIXELFORMAT_YUY2:	        return SDL_PIXELFORMAT_YUY2;
				case PixelFormat::PF::PIXELFORMAT_UYVY:	        return SDL_PIXELFORMAT_UYVY;
				case PixelFormat::PF::PIXELFORMAT_YVYU:	        return SDL_PIXELFORMAT_YVYU;
				default:
					ASSERT_LOG(false, "Unknown pixel format given");
			}
			return SDL_PIXELFORMAT_ABGR8888;
		}
	}

	SurfaceSDL::SurfaceSDL(unsigned width, 
		unsigned height, 
		unsigned bpp, 
		uint32_t rmask, 
		uint32_t gmask, 
		uint32_t bmask, 
		uint32_t amask) : has_data_(false)
	{
		surface_ = SDL_CreateRGBSurface(0, width, height, bpp, rmask, gmask, bmask, amask);
		ASSERT_LOG(surface_ != NULL, "Error creating surface: " << SDL_GetError());
		SetPixelFormat(PixelFormatPtr(new SDLPixelFormat(surface_->format)));
	}

	SurfaceSDL::SurfaceSDL(unsigned width, 
		unsigned height, 
		unsigned bpp, 
		unsigned row_pitch,
		uint32_t rmask, 
		uint32_t gmask, 
		uint32_t bmask, 
		uint32_t amask, 
		void* pixels) : has_data_(true)
	{
		ASSERT_LOG(pixels != NULL, "NULL value for pixels while creating surface.");
		surface_ = SDL_CreateRGBSurfaceFrom(pixels, width, height, bpp, row_pitch, rmask, gmask, bmask, amask);
		ASSERT_LOG(surface_ != NULL, "Error creating surface: " << SDL_GetError());
		SetPixelFormat(PixelFormatPtr(new SDLPixelFormat(surface_->format)));
	}

	SurfaceSDL::SurfaceSDL(SDL_Surface* surface)
		: surface_(surface)
	{
		ASSERT_LOG(surface_ != NULL, "Error creating surface: " << SDL_GetError());
		SetPixelFormat(PixelFormatPtr(new SDLPixelFormat(surface_->format)));
	}

	SurfaceSDL::SurfaceSDL(size_t width, size_t height, PixelFormat::PF format)
	{
		// XXX todo
	}

	SurfaceSDL::~SurfaceSDL()
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null in destructor");
		SDL_FreeSurface(surface_);
	}

	const void* SurfaceSDL::Pixels() const
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null");
		// technically surface_->locked is an internal implementation detail.
		// but we'll live with using it.
		if(SDL_MUSTLOCK(surface_) && !surface_->locked) {
			ASSERT_LOG(false, "Surface is marked as needing to be locked but is not locked on Pixels access.");
		}
		return surface_->pixels;
	}

	bool SurfaceSDL::SetClipRect(int x, int y, unsigned width, unsigned height)
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null");
		SDL_Rect r = {x,y,width,height};
		return SDL_SetClipRect(surface_, &r) != SDL_TRUE;
	}

	void SurfaceSDL::GetClipRect(int& x, int& y, unsigned& width, unsigned& height)
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null");
		SDL_Rect r;
		SDL_GetClipRect(surface_, &r);
		x = r.x;
		y = r.y;
		width = r.w;
		height = r.h;

	}

	bool SurfaceSDL::SetClipRect(const rect& r)
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null");
		SDL_Rect sr = r.sdl_rect();
		return SDL_SetClipRect(surface_, &sr) == SDL_TRUE;
	}

	const rect SurfaceSDL::GetClipRect()
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null");
		SDL_Rect sr;
		SDL_GetClipRect(surface_, &sr);
		return rect(sr);
	}

	void SurfaceSDL::Lock() 
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null");
		if(SDL_MUSTLOCK(surface_)) {
			auto res = SDL_LockSurface(surface_);
			ASSERT_LOG(res == 0, "Error calling SDL_LockSurface(): " << SDL_GetError());
		}
	}

	void SurfaceSDL::Unlock() 
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null");
		if(SDL_MUSTLOCK(surface_)) {
			SDL_UnlockSurface(surface_);
		}
	}

	void SurfaceSDL::WritePixels(unsigned bpp, 
		uint32_t rmask, 
		uint32_t gmask, 
		uint32_t bmask, 
		uint32_t amask,
		const void* pixels)
	{
		SDL_FreeSurface(surface_);
		ASSERT_LOG(pixels != NULL, "NULL value for pixels while creating surface.");
		surface_ = SDL_CreateRGBSurfaceFrom(const_cast<void*>(pixels), width(), height(), bpp, row_pitch(), rmask, gmask, bmask, amask);
		ASSERT_LOG(surface_ != NULL, "Error creating surface: " << SDL_GetError());
		SetPixelFormat(PixelFormatPtr(new SDLPixelFormat(surface_->format)));
	}

	void SurfaceSDL::WritePixels(const void* pixels) 
	{
		SurfaceLock lock(SurfacePtr(this));
		memcpy(surface_->pixels, pixels, row_pitch() * width());
	}

	void SurfaceSDL::SetBlendMode(Surface::BlendMode bm) 
	{
		SDL_BlendMode sdl_bm;
		switch(bm) {
			case BLEND_MODE_NONE:	sdl_bm = SDL_BLENDMODE_NONE; break;
			case BLEND_MODE_BLEND:	sdl_bm = SDL_BLENDMODE_BLEND; break;
			case BLEND_MODE_ADD:	sdl_bm = SDL_BLENDMODE_ADD; break;
			case BLEND_MODE_MODULATE:	sdl_bm = SDL_BLENDMODE_MOD; break;
		}
		SDL_SetSurfaceBlendMode(surface_, sdl_bm);
	}

	Surface::BlendMode SurfaceSDL::GetBlendMode() const 
	{
		SDL_BlendMode sdl_bm;
		SDL_GetSurfaceBlendMode(surface_, &sdl_bm);
		switch(sdl_bm) {
		case SDL_BLENDMODE_NONE:	return BLEND_MODE_NONE;
		case SDL_BLENDMODE_BLEND:	return BLEND_MODE_BLEND;
		case SDL_BLENDMODE_ADD:		return BLEND_MODE_ADD;
		case SDL_BLENDMODE_MOD:		return BLEND_MODE_MODULATE;
		}
		ASSERT_LOG(false, "Unrecognised SDL blend mode: " << sdl_bm);
		return BLEND_MODE_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	// SDLPixelFormat
	namespace 
	{
		uint8_t count_bits_set(uint32_t v)
		{
			v = v - ((v >> 1) & 0x55555555);
			v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
			return uint8_t(((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24);
		}
	}

	SDLPixelFormat::SDLPixelFormat(const SDL_PixelFormat* pf)
		: pf_(pf)
	{
		ASSERT_LOG(pf != NULL, "SDLPixelFormat constructor passeda null pixel format.");
	}

	SDLPixelFormat::~SDLPixelFormat()
	{
	}

	uint8_t SDLPixelFormat::BitsPerPixel() const 
	{
		return pf_->BitsPerPixel;
	}

	uint8_t SDLPixelFormat::BytesPerPixel() const 
	{
		return pf_->BytesPerPixel;
	}

	bool SDLPixelFormat::IsYuvPlanar() const 
	{
		return pf_->format == SDL_PIXELFORMAT_YV12 || pf_->format == SDL_PIXELFORMAT_IYUV;
	}

	bool SDLPixelFormat::IsYuvPacked() const 
	{
		return pf_->format == SDL_PIXELFORMAT_YUY2 
			|| pf_->format == SDL_PIXELFORMAT_UYVY
			|| pf_->format == SDL_PIXELFORMAT_YVYU;
	}

	bool SDLPixelFormat::YuvHeightReversed() const 
	{
		return false;
	}

	bool SDLPixelFormat::IsInterlaced() const 
	{
		return false;
	}
		
	bool SDLPixelFormat::IsRGB() const 
	{
		return !SDL_ISPIXELFORMAT_FOURCC(pf_->format);
	}

	bool SDLPixelFormat::HasRedChannel() const 
	{
		return IsRGB() && pf_->Rmask != 0;
	}

	bool SDLPixelFormat::HasGreenChannel() const 
	{
		return IsRGB() && pf_->Gmask != 0;
	}

	bool SDLPixelFormat::HasBlueChannel() const 
	{
		return IsRGB() && pf_->Bmask != 0;
	}

	bool SDLPixelFormat::HasAlphaChannel() const 
	{
		return IsRGB() && pf_->Amask != 0;
	}

	bool SDLPixelFormat::HasLuminance() const 
	{
		return IsRGB() && pf_->Rmask != 0;
	}

	uint32_t SDLPixelFormat::RedMask() const 
	{
		ASSERT_LOG(IsRGB(), "Asked for RedMask of non-RGB surface.");
		return pf_->Rmask;
	}

	uint32_t SDLPixelFormat::GreenMask() const 
	{
		ASSERT_LOG(IsRGB(), "Asked for GreenMask of non-RGB surface.");
		return pf_->Gmask;
	}

	uint32_t SDLPixelFormat::BlueMask() const 
	{
		ASSERT_LOG(IsRGB(), "Asked for BlueMask of non-RGB surface.");
		return pf_->Bmask;
	}

	uint32_t SDLPixelFormat::AlphaMask() const 
	{
		return pf_->Amask;
	}

	uint32_t SDLPixelFormat::LuminanceMask() const 
	{
		ASSERT_LOG(IsRGB(), "Asked for LuminanceMask of non-RGB surface.");
		return pf_->Rmask;
	}

	uint8_t SDLPixelFormat::RedBits() const 
	{
		ASSERT_LOG(IsRGB(), "Asked for RedBits() of non-RGB surface.");
		return count_bits_set(pf_->Rmask);
	}

	uint8_t SDLPixelFormat::GreenBits() const 
	{
		ASSERT_LOG(IsRGB(), "Asked for GreenBits() of non-RGB surface.");
		return count_bits_set(pf_->Gmask);
	}

	uint8_t SDLPixelFormat::BlueBits() const 
	{
		ASSERT_LOG(IsRGB(), "Asked for BlueBits() of non-RGB surface.");
		return count_bits_set(pf_->Bmask);
	}

	uint8_t SDLPixelFormat::AlphaBits() const 
	{
		ASSERT_LOG(IsRGB(), "Asked for AlphaBits() of non-RGB surface.");
		return count_bits_set(pf_->Amask);
	}

	uint8_t SDLPixelFormat::LuminanceBits() const 
	{
		ASSERT_LOG(IsRGB(), "Asked for LuminanceBits() of non-RGB surface.");
		return count_bits_set(pf_->Rmask);
	}

	uint32_t SDLPixelFormat::RedShift() const 
	{
		ASSERT_LOG(IsRGB(), "Asked for RedShift() of non-RGB surface.");
		return pf_->Rshift;
	}

	uint32_t SDLPixelFormat::GreenShift() const 
	{
		ASSERT_LOG(IsRGB(), "Asked for GreenShift() of non-RGB surface.");
		return pf_->Gshift;
	}

	uint32_t SDLPixelFormat::BlueShift() const 
	{
		ASSERT_LOG(IsRGB(), "Asked for BlueShift() of non-RGB surface.");
		return pf_->Bshift;
	}

	uint32_t SDLPixelFormat::AlphaShift() const 
	{
		ASSERT_LOG(IsRGB(), "Asked for AlphaShift() of non-RGB surface.");
		return pf_->Ashift;
	}

	uint32_t SDLPixelFormat::LuminanceShift() const 
	{
		ASSERT_LOG(IsRGB(), "Asked for LuminanceShift() of non-RGB surface.");
		return pf_->Rshift;
	}

	bool SDLPixelFormat::HasPalette() const 
	{
		return pf_->palette != NULL;
	}

	PixelFormat::PF SDLPixelFormat::GetFormat() const
	{
		switch(pf_->format) {
			case SDL_PIXELFORMAT_INDEX1LSB:	    return PF::PIXELFORMAT_INDEX1LSB;
			case SDL_PIXELFORMAT_INDEX1MSB:	    return PF::PIXELFORMAT_INDEX1MSB;
			case SDL_PIXELFORMAT_INDEX4LSB:	    return PF::PIXELFORMAT_INDEX4LSB;
			case SDL_PIXELFORMAT_INDEX4MSB:	    return PF::PIXELFORMAT_INDEX4MSB;
			case SDL_PIXELFORMAT_INDEX8:	    return PF::PIXELFORMAT_INDEX8;
			case SDL_PIXELFORMAT_RGB332:	    return PF::PIXELFORMAT_RGB332;
			case SDL_PIXELFORMAT_RGB444:	    return PF::PIXELFORMAT_RGB444;
			case SDL_PIXELFORMAT_RGB555:	    return PF::PIXELFORMAT_RGB555;
			case SDL_PIXELFORMAT_BGR555:	    return PF::PIXELFORMAT_BGR555;
			case SDL_PIXELFORMAT_ARGB4444:	    return PF::PIXELFORMAT_ARGB4444;
			case SDL_PIXELFORMAT_RGBA4444:	    return PF::PIXELFORMAT_RGBA4444;
			case SDL_PIXELFORMAT_ABGR4444:	    return PF::PIXELFORMAT_ABGR4444;
			case SDL_PIXELFORMAT_BGRA4444:	    return PF::PIXELFORMAT_BGRA4444;
			case SDL_PIXELFORMAT_ARGB1555:	    return PF::PIXELFORMAT_ARGB1555;
			case SDL_PIXELFORMAT_RGBA5551:	    return PF::PIXELFORMAT_RGBA5551;
			case SDL_PIXELFORMAT_ABGR1555:	    return PF::PIXELFORMAT_ABGR1555;
			case SDL_PIXELFORMAT_BGRA5551:	    return PF::PIXELFORMAT_BGRA5551;
			case SDL_PIXELFORMAT_RGB565:	    return PF::PIXELFORMAT_RGB565;
			case SDL_PIXELFORMAT_BGR565:	    return PF::PIXELFORMAT_BGR565;
			case SDL_PIXELFORMAT_RGB24:	        return PF::PIXELFORMAT_RGB24;
			case SDL_PIXELFORMAT_BGR24:	        return PF::PIXELFORMAT_BGR24;
			case SDL_PIXELFORMAT_RGB888:	    return PF::PIXELFORMAT_RGB888;
			case SDL_PIXELFORMAT_RGBX8888:	    return PF::PIXELFORMAT_RGBX8888;
			case SDL_PIXELFORMAT_BGR888:	    return PF::PIXELFORMAT_BGR888;
			case SDL_PIXELFORMAT_BGRX8888:	    return PF::PIXELFORMAT_BGRX8888;
			case SDL_PIXELFORMAT_ARGB8888:	    return PF::PIXELFORMAT_ARGB8888;
			case SDL_PIXELFORMAT_RGBA8888:	    return PF::PIXELFORMAT_RGBA8888;
			case SDL_PIXELFORMAT_ABGR8888:	    return PF::PIXELFORMAT_ABGR8888;
			case SDL_PIXELFORMAT_BGRA8888:	    return PF::PIXELFORMAT_BGRA8888;
			case SDL_PIXELFORMAT_ARGB2101010:	return PF::PIXELFORMAT_ARGB2101010;
			case SDL_PIXELFORMAT_YV12:	        return PF::PIXELFORMAT_YV12;
			case SDL_PIXELFORMAT_IYUV:	        return PF::PIXELFORMAT_IYUV;
			case SDL_PIXELFORMAT_YUY2:	        return PF::PIXELFORMAT_YUY2;
			case SDL_PIXELFORMAT_UYVY:	        return PF::PIXELFORMAT_UYVY;
			case SDL_PIXELFORMAT_YVYU:	        return PF::PIXELFORMAT_YVYU;
		}
		return PF::PIXELFORMAT_UNKNOWN;
	}

	SurfacePtr SurfaceSDL::CreateFromFile(const std::string& filename, PixelFormat::PF fmt, SurfaceConvertFn fn)
	{
		auto surface = new SurfaceSDL(IMG_Load(filename.c_str()));
		// format means don't convert the surface from the loaded format.
		if(fmt != PixelFormat::PF::PIXELFORMAT_UNKNOWN) {
			return surface->Convert(fmt, fn);
		}
		return SurfacePtr(surface);
	}

	std::tuple<int,int> SDLPixelFormat::ExtractRGBA(const void* pixels, int ndx, uint32_t& red, uint32_t& green, uint32_t& blue, uint32_t& alpha)
	{
		auto fmt = GetFormat();
		int pixel_shift_return = BytesPerPixel();
		red = 0;
		green = 0;
		blue = 0;
		alpha = 255;
		switch(fmt) {
            case PixelFormat::PF::PIXELFORMAT_INDEX1LSB: {
				ASSERT_LOG(pf_->palette != NULL, "Index type has no palette.");
				uint8_t px = *static_cast<const uint8_t*>(pixels) & (1 << ndx) >> ndx;
				ASSERT_LOG(px < pf_->palette->ncolors, "Index into palette invalid. " << px << " >= " << pf_->palette->ncolors);
				auto color = pf_->palette->colors[px];
				red = color.r;
				green = color.g;
				blue = color.b;
				alpha = color.a;
				if(ndx == 7) {
					ndx = 0;
				} else {
					pixel_shift_return = 0;
					++ndx;
				}
				break;
			}
            case PixelFormat::PF::PIXELFORMAT_INDEX1MSB: {
				ASSERT_LOG(pf_->palette != NULL, "Index type has no palette.");
				uint8_t px = (*static_cast<const uint8_t*>(pixels) & (1 << (7-ndx))) >> (7-ndx);
				ASSERT_LOG(px < pf_->palette->ncolors, "Index into palette invalid. " << px << " >= " << pf_->palette->ncolors);
				auto color = pf_->palette->colors[px];
				red = color.r;
				green = color.g;
				blue = color.b;
				alpha = color.a;
				if(ndx == 7) {
					ndx = 0;
				} else {
					pixel_shift_return = 0;
					++ndx;
				}
				break;
			}
            case PixelFormat::PF::PIXELFORMAT_INDEX4LSB: {
				ASSERT_LOG(pf_->palette != NULL, "Index type has no palette.");
				uint8_t px = (*static_cast<const uint8_t*>(pixels) & (0xf << ndx)) >> ndx;
				ASSERT_LOG(px < pf_->palette->ncolors, "Index into palette invalid. " << px << " >= " << pf_->palette->ncolors);
				auto color = pf_->palette->colors[px];
				red = color.r;
				green = color.g;
				blue = color.b;
				alpha = color.a;
				if(ndx == 4) {
					ndx = 0;
				} else {
					pixel_shift_return = 0;
					ndx = 4;
				}
				break;
			}
			case PixelFormat::PF::PIXELFORMAT_INDEX4MSB: {
				ASSERT_LOG(pf_->palette != NULL, "Index type has no palette.");
				uint8_t px = (*static_cast<const uint8_t*>(pixels) & (0xf << (4-ndx))) >> (4-ndx);
				ASSERT_LOG(px < pf_->palette->ncolors, "Index into palette invalid. " << px << " >= " << pf_->palette->ncolors);
				auto color = pf_->palette->colors[px];
				red = color.r;
				green = color.g;
				blue = color.b;
				alpha = color.a;
				if(ndx == 4) {
					ndx = 0;
				} else {
					pixel_shift_return = 0;
					ndx = 4;
				}
				break;
			}
            case PixelFormat::PF::PIXELFORMAT_INDEX8: {
				ASSERT_LOG(pf_->palette != NULL, "Index type has no palette.");
				uint8_t px = *static_cast<const uint8_t*>(pixels);
				ASSERT_LOG(px < pf_->palette->ncolors, "Index into palette invalid. " << px << " >= " << pf_->palette->ncolors);
				auto color = pf_->palette->colors[px];
				red = color.r;
				green = color.g;
				blue = color.b;
				alpha = color.a;
				pixel_shift_return = BytesPerPixel();
				break;
			}

            case PixelFormat::PF::PIXELFORMAT_RGB332:
            case PixelFormat::PF::PIXELFORMAT_RGB444:
            case PixelFormat::PF::PIXELFORMAT_RGB555:
            case PixelFormat::PF::PIXELFORMAT_BGR555:
            case PixelFormat::PF::PIXELFORMAT_ARGB4444:
            case PixelFormat::PF::PIXELFORMAT_RGBA4444:
            case PixelFormat::PF::PIXELFORMAT_ABGR4444:
            case PixelFormat::PF::PIXELFORMAT_BGRA4444:
            case PixelFormat::PF::PIXELFORMAT_ARGB1555:
            case PixelFormat::PF::PIXELFORMAT_RGBA5551:
            case PixelFormat::PF::PIXELFORMAT_ABGR1555:
            case PixelFormat::PF::PIXELFORMAT_BGRA5551:
            case PixelFormat::PF::PIXELFORMAT_RGB565:
            case PixelFormat::PF::PIXELFORMAT_BGR565:
            case PixelFormat::PF::PIXELFORMAT_RGB24:
            case PixelFormat::PF::PIXELFORMAT_BGR24:
            case PixelFormat::PF::PIXELFORMAT_RGB888:
            case PixelFormat::PF::PIXELFORMAT_RGBX8888:
            case PixelFormat::PF::PIXELFORMAT_BGR888:
            case PixelFormat::PF::PIXELFORMAT_BGRX8888:
            case PixelFormat::PF::PIXELFORMAT_ARGB8888:
            case PixelFormat::PF::PIXELFORMAT_RGBA8888:
            case PixelFormat::PF::PIXELFORMAT_ABGR8888:
            case PixelFormat::PF::PIXELFORMAT_BGRA8888:
            case PixelFormat::PF::PIXELFORMAT_ARGB2101010: {
				const uint32_t* px = static_cast<const uint32_t*>(pixels);
				if(HasRedChannel()) {
					red = (*px) & RedMask() >> RedShift();
				}
				if(HasGreenChannel()) {
					green = (*px) & GreenMask() >> GreenShift();
				}
				if(HasBlueChannel()) {
					blue = (*px) & BlueMask() >> BlueShift();
				}
				if(HasAlphaChannel()) {
					alpha = (*px) & AlphaMask() >> AlphaShift();
				}
				break;
			}

            case PixelFormat::PF::PIXELFORMAT_YV12:
            case PixelFormat::PF::PIXELFORMAT_IYUV:
            case PixelFormat::PF::PIXELFORMAT_YUY2:
            case PixelFormat::PF::PIXELFORMAT_UYVY:
            case PixelFormat::PF::PIXELFORMAT_YVYU:				
			default:
				ASSERT_LOG(false, "unsupported pixel format value for conversion.");
		}
		return std::make_tuple(pixel_shift_return, ndx);
	}

	void SDLPixelFormat::EncodeRGBA(void* pixels, uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha)
	{
		auto fmt = GetFormat();
		switch(fmt) {
            case PixelFormat::PF::PIXELFORMAT_RGB332:
            case PixelFormat::PF::PIXELFORMAT_RGB444:
            case PixelFormat::PF::PIXELFORMAT_RGB555:
            case PixelFormat::PF::PIXELFORMAT_BGR555:
            case PixelFormat::PF::PIXELFORMAT_ARGB4444:
            case PixelFormat::PF::PIXELFORMAT_RGBA4444:
            case PixelFormat::PF::PIXELFORMAT_ABGR4444:
            case PixelFormat::PF::PIXELFORMAT_BGRA4444:
            case PixelFormat::PF::PIXELFORMAT_ARGB1555:
            case PixelFormat::PF::PIXELFORMAT_RGBA5551:
            case PixelFormat::PF::PIXELFORMAT_ABGR1555:
            case PixelFormat::PF::PIXELFORMAT_BGRA5551:
            case PixelFormat::PF::PIXELFORMAT_RGB565:
            case PixelFormat::PF::PIXELFORMAT_BGR565:
            case PixelFormat::PF::PIXELFORMAT_RGB24:
            case PixelFormat::PF::PIXELFORMAT_BGR24:
            case PixelFormat::PF::PIXELFORMAT_RGB888:
            case PixelFormat::PF::PIXELFORMAT_RGBX8888:
            case PixelFormat::PF::PIXELFORMAT_BGR888:
            case PixelFormat::PF::PIXELFORMAT_BGRX8888:
            case PixelFormat::PF::PIXELFORMAT_ARGB8888:
            case PixelFormat::PF::PIXELFORMAT_RGBA8888:
            case PixelFormat::PF::PIXELFORMAT_ABGR8888:
            case PixelFormat::PF::PIXELFORMAT_BGRA8888:
            case PixelFormat::PF::PIXELFORMAT_ARGB2101010: {
				uint32_t pixel = 0;
				if(HasRedChannel()) {
					pixel = (red << RedShift()) & RedMask();
				}
				if(HasGreenChannel()) {
					pixel = (green << GreenShift()) & GreenMask();
				}
				if(HasBlueChannel()) {
					pixel = (blue << BlueShift()) & BlueMask();
				}
				if(HasAlphaChannel()) {
					pixel = (alpha << AlphaShift()) & AlphaMask();
				}
				uint32_t* px = static_cast<uint32_t*>(pixels);
				*px = pixel;
				break;
			}

            case PixelFormat::PF::PIXELFORMAT_INDEX1LSB: 
            case PixelFormat::PF::PIXELFORMAT_INDEX1MSB:
            case PixelFormat::PF::PIXELFORMAT_INDEX4LSB:
            case PixelFormat::PF::PIXELFORMAT_INDEX4MSB:
            case PixelFormat::PF::PIXELFORMAT_INDEX8:
				ASSERT_LOG(false, "converting format to an indexed type not supported.");
				break;
            case PixelFormat::PF::PIXELFORMAT_YV12:
            case PixelFormat::PF::PIXELFORMAT_IYUV:
            case PixelFormat::PF::PIXELFORMAT_YUY2:
            case PixelFormat::PF::PIXELFORMAT_UYVY:
            case PixelFormat::PF::PIXELFORMAT_YVYU:				
			default:
				ASSERT_LOG(false, "unsupported pixel format value for conversion.");
		}
	}

	SurfacePtr SurfaceSDL::HandleConvert(PixelFormat::PF fmt, SurfaceConvertFn convert)
	{
		ASSERT_LOG(fmt != PixelFormat::PF::PIXELFORMAT_UNKNOWN, "unknown pixel format to convert to.");
		if(convert == nullptr) {
			std::shared_ptr<SDL_PixelFormat> pf = std::shared_ptr<SDL_PixelFormat>(SDL_AllocFormat(get_sdl_pixel_format(fmt)), [](SDL_PixelFormat* fmt) {
				SDL_FreeFormat(fmt);
			});
			auto surface = new SurfaceSDL(SDL_ConvertSurface(surface_, pf.get(), 0));
			return SurfacePtr(surface);
		}

		SurfaceLock lock(SurfacePtr(this));
		uint32_t red;
		uint32_t green;
		uint32_t blue;
		uint32_t alpha;

		// Create a destination surface
		auto dst = new SurfaceSDL(width(), height(), fmt);
		size_t dst_size = dst->row_pitch() * dst->height();
		void* dst_pixels = new uint8_t[dst_size];

		for(size_t h = 0; h != height(); ++h) {
			unsigned offs = 0;
			int ndx = 0;
			uint8_t* pixel_ptr = static_cast<uint8_t*>(surface_->pixels) + h*row_pitch();
			uint8_t* dst_pixel_ptr = static_cast<uint8_t*>(dst_pixels) + h*row_pitch();
			while(offs < width()) {
				std::tie(offs, ndx) = GetPixelFormat()->ExtractRGBA(pixel_ptr + offs, ndx, red, green, blue, alpha);
				convert(red, green, blue, alpha);
				dst->GetPixelFormat()->EncodeRGBA(dst_pixels, red, green, blue, alpha);
			}
		}
		dst->WritePixels(dst_pixels);
		delete[] dst_pixels;
		return SurfacePtr(dst);
	}
}
