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

#ifndef _USE_MATH_DEFINES
#	define _USE_MATH_DEFINES	1
#endif 

#include "SDL_image.h"

#include "asserts.hpp"
#include "SurfaceSDL.hpp"

enum {
	SDL_PIXELFORMAT_XRGB8888 = 
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_XRGB,
                               SDL_PACKEDLAYOUT_8888, 32, 4),
};

namespace KRE
{
	namespace
	{
		bool can_create_surfaces = Surface::registerSurfaceCreator("sdl", 
			SurfaceSDL::createFromFile,
			SurfaceSDL::createFromPixels,
			SurfaceSDL::createFromMask,
			SurfaceSDL::createFromFormat);

		Uint32 get_sdl_pixel_format(PixelFormat::PF fmt)
		{
			switch(fmt) {
				case PixelFormat::PF::PIXELFORMAT_INDEX1LSB:	return SDL_PIXELFORMAT_INDEX1LSB;
				case PixelFormat::PF::PIXELFORMAT_INDEX1MSB:	return SDL_PIXELFORMAT_INDEX1MSB;
				case PixelFormat::PF::PIXELFORMAT_INDEX4LSB:	return SDL_PIXELFORMAT_INDEX4LSB;
				case PixelFormat::PF::PIXELFORMAT_INDEX4MSB:	return SDL_PIXELFORMAT_INDEX4MSB;
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
				case PixelFormat::PF::PIXELFORMAT_RGB24:	    return SDL_PIXELFORMAT_RGB24;
				case PixelFormat::PF::PIXELFORMAT_BGR24:	    return SDL_PIXELFORMAT_BGR24;
				case PixelFormat::PF::PIXELFORMAT_RGB888:	    return SDL_PIXELFORMAT_RGB888;
				case PixelFormat::PF::PIXELFORMAT_RGBX8888:	    return SDL_PIXELFORMAT_RGBX8888;
				case PixelFormat::PF::PIXELFORMAT_BGR888:	    return SDL_PIXELFORMAT_BGR888;
				case PixelFormat::PF::PIXELFORMAT_BGRX8888:	    return SDL_PIXELFORMAT_BGRX8888;
				case PixelFormat::PF::PIXELFORMAT_ARGB8888:	    return SDL_PIXELFORMAT_ARGB8888;
				case PixelFormat::PF::PIXELFORMAT_XRGB8888:	    return SDL_PIXELFORMAT_XRGB8888;
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

	SurfaceSDL::SurfaceSDL(int width, 
		int height, 
		int bpp, 
		uint32_t rmask, 
		uint32_t gmask, 
		uint32_t bmask, 
		uint32_t amask) : has_data_(false)
	{
		surface_ = SDL_CreateRGBSurface(0, width, height, bpp, rmask, gmask, bmask, amask);
		ASSERT_LOG(surface_ != NULL, "Error creating surface: " << SDL_GetError());
		
		auto pf = std::make_shared<SDLPixelFormat>(surface_->format->format);
		setPixelFormat(PixelFormatPtr(pf));
	}

	SurfaceSDL::SurfaceSDL(int width, 
		int height, 
		int bpp, 
		int row_pitch,
		uint32_t rmask, 
		uint32_t gmask, 
		uint32_t bmask, 
		uint32_t amask, 
		const void* pixels) : has_data_(true)
	{
		ASSERT_LOG(pixels != NULL, "NULL value for pixels while creating surface.");
		surface_ = SDL_CreateRGBSurfaceFrom(const_cast<void*>(pixels), width, height, bpp, row_pitch, rmask, gmask, bmask, amask);
		ASSERT_LOG(surface_ != NULL, "Error creating surface: " << SDL_GetError());
		auto pf = std::make_shared<SDLPixelFormat>(surface_->format->format);
		setPixelFormat(PixelFormatPtr(pf));
	}

	SurfaceSDL::SurfaceSDL(const std::string& filename)
	{
		surface_ = IMG_Load(filename.c_str());
		if(surface_ == NULL) {
			LOG_ERROR("Failed to load image file: '" << filename << "' : " << IMG_GetError());
			throw ImageLoadError();
		}
		auto pf = std::make_shared<SDLPixelFormat>(surface_->format->format);
		setPixelFormat(PixelFormatPtr(pf));
	}

	SurfaceSDL::SurfaceSDL(SDL_Surface* surface)
		: surface_(surface)
	{
		ASSERT_LOG(surface_ != NULL, "Error creating surface: " << SDL_GetError());
		auto pf = std::make_shared<SDLPixelFormat>(surface_->format->format);
		setPixelFormat(PixelFormatPtr(pf));
	}

	SurfaceSDL::SurfaceSDL(int width, int height, PixelFormat::PF format)
	{
		int bpp;
		uint32_t rmask, gmask, bmask, amask;
		SDL_bool ret = SDL_PixelFormatEnumToMasks(get_sdl_pixel_format(format), &bpp, &rmask, &gmask, &bmask, &amask);
		ASSERT_LOG(ret != SDL_FALSE, "Unable to convert pixel format to masks: " << SDL_GetError());

		surface_ = SDL_CreateRGBSurface(0, width, height, bpp, rmask, gmask, bmask, amask);
		ASSERT_LOG(surface_ != NULL, "Error creating surface: " << SDL_GetError());
		auto pf = std::make_shared<SDLPixelFormat>(surface_->format->format);
		setPixelFormat(PixelFormatPtr(pf));
	}

	SurfaceSDL::~SurfaceSDL()
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null in destructor");
		SDL_FreeSurface(surface_);
	}

	SurfacePtr SurfaceSDL::createFromPixels(int width, 
		int height, 
		int bpp, 
		int row_pitch, 
		uint32_t rmask, 
		uint32_t gmask, 
		uint32_t bmask, 
		uint32_t amask, 
		const void* pixels)
	{
		auto s = std::make_shared<SurfaceSDL>(width, height, bpp, row_pitch, rmask, gmask, bmask, amask, pixels);
		return SurfacePtr(s);
	}

	SurfacePtr SurfaceSDL::createFromMask(int width, 
		int height, 
		int bpp, 
		uint32_t rmask, 
		uint32_t gmask, 
		uint32_t bmask, 
		uint32_t amask)
	{
		auto s = std::make_shared<SurfaceSDL>(width, height, bpp, rmask, gmask, bmask, amask);
		return SurfacePtr(s);
	}

	SurfacePtr SurfaceSDL::createFromFormat(int width,
		int height,
		PixelFormat::PF fmt)
	{
		auto s = std::make_shared<SurfaceSDL>(width, height, fmt);
		return SurfacePtr(s);
	}

	const void* SurfaceSDL::pixels() const
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null");
		// technically surface_->locked is an internal implementation detail.
		// but we'll live with using it.
		if(SDL_MUSTLOCK(surface_) && !surface_->locked) {
			ASSERT_LOG(false, "Surface is marked as needing to be locked but is not locked on Pixels access.");
		}
		return surface_->pixels;
	}

	void* SurfaceSDL::pixelsWriteable()
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null");
		// technically surface_->locked is an internal implementation detail.
		// but we'll live with using it.
		if(SDL_MUSTLOCK(surface_) && !surface_->locked) {
			ASSERT_LOG(false, "Surface is marked as needing to be locked but is not locked on Pixels access.");
		}
		return surface_->pixels;
	}

	bool SurfaceSDL::setClipRect(int x, int y, unsigned width, unsigned height)
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null");
		SDL_Rect r = {x,y,width,height};
		return SDL_SetClipRect(surface_, &r) != SDL_TRUE;
	}

	void SurfaceSDL::getClipRect(int& x, int& y, unsigned& width, unsigned& height)
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null");
		SDL_Rect r;
		SDL_GetClipRect(surface_, &r);
		x = r.x;
		y = r.y;
		width = r.w;
		height = r.h;

	}

	bool SurfaceSDL::setClipRect(const rect& r)
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null");
		SDL_Rect sr = {r.x(), r.y(), r.w(), r.h()};
		return SDL_SetClipRect(surface_, &sr) == SDL_TRUE;
	}

	const rect SurfaceSDL::getClipRect()
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null");
		SDL_Rect sr;
		SDL_GetClipRect(surface_, &sr);
		return rect(sr.x, sr.y, sr.w, sr.h);
	}

	void SurfaceSDL::lock() 
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null");
		if(SDL_MUSTLOCK(surface_)) {
			auto res = SDL_LockSurface(surface_);
			ASSERT_LOG(res == 0, "Error calling SDL_LockSurface(): " << SDL_GetError());
		}
	}

	void SurfaceSDL::unlock() 
	{
		ASSERT_LOG(surface_ != NULL, "surface_ is null");
		if(SDL_MUSTLOCK(surface_)) {
			SDL_UnlockSurface(surface_);
		}
	}

	void SurfaceSDL::writePixels(int bpp, 
		uint32_t rmask, 
		uint32_t gmask, 
		uint32_t bmask, 
		uint32_t amask,
		const void* pixels)
	{
		SDL_FreeSurface(surface_);
		ASSERT_LOG(pixels != NULL, "NULL value for pixels while creating surface.");
		surface_ = SDL_CreateRGBSurfaceFrom(const_cast<void*>(pixels), width(), height(), bpp, rowPitch(), rmask, gmask, bmask, amask);
		ASSERT_LOG(surface_ != NULL, "Error creating surface: " << SDL_GetError());
		setPixelFormat(PixelFormatPtr(new SDLPixelFormat(surface_->format->format)));
	}

	void SurfaceSDL::writePixels(const void* pixels) 
	{
		SurfaceLock lock(SurfacePtr(this));
		memcpy(surface_->pixels, pixels, rowPitch() * width());
	}

	void SurfaceSDL::setBlendMode(Surface::BlendMode bm) 
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

	Surface::BlendMode SurfaceSDL::getBlendMode() const 
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

	void SurfaceSDL::fillRect(const rect& dst_rect, const Color& color)
	{
		SDL_Rect r = {dst_rect.x(), dst_rect.y(), dst_rect.w(), dst_rect.h() };
		SDL_FillRect(surface_, &r, color.asARGB());
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

	SDLPixelFormat::SDLPixelFormat(Uint32 pf)
	{
		pf_ = SDL_AllocFormat(pf);
		ASSERT_LOG(pf_ != NULL, "SDLPixelFormat constructor passed a null pixel format: " << SDL_GetError());
	}

	SDLPixelFormat::~SDLPixelFormat()
	{
		SDL_FreeFormat(pf_);
	}

	uint8_t SDLPixelFormat::bitsPerPixel() const 
	{
		return pf_->BitsPerPixel;
	}

	uint8_t SDLPixelFormat::bytesPerPixel() const 
	{
		return pf_->BytesPerPixel;
	}

	bool SDLPixelFormat::isYuvPlanar() const 
	{
		return pf_->format == SDL_PIXELFORMAT_YV12 || pf_->format == SDL_PIXELFORMAT_IYUV;
	}

	bool SDLPixelFormat::isYuvPacked() const 
	{
		return pf_->format == SDL_PIXELFORMAT_YUY2 
			|| pf_->format == SDL_PIXELFORMAT_UYVY
			|| pf_->format == SDL_PIXELFORMAT_YVYU;
	}

	bool SDLPixelFormat::isYuvHeightReversed() const 
	{
		return false;
	}

	bool SDLPixelFormat::isInterlaced() const 
	{
		return false;
	}
		
	bool SDLPixelFormat::isRGB() const 
	{
		return !SDL_ISPIXELFORMAT_FOURCC(pf_->format);
	}

	bool SDLPixelFormat::hasRedChannel() const 
	{
		return isRGB() && pf_->Rmask != 0;
	}

	bool SDLPixelFormat::hasGreenChannel() const 
	{
		return isRGB() && pf_->Gmask != 0;
	}

	bool SDLPixelFormat::hasBlueChannel() const 
	{
		return isRGB() && pf_->Bmask != 0;
	}

	bool SDLPixelFormat::hasAlphaChannel() const 
	{
		return isRGB() && pf_->Amask != 0;
	}

	bool SDLPixelFormat::hasLuminance() const 
	{
		return isRGB() && pf_->Rmask != 0;
	}

	uint32_t SDLPixelFormat::getRedMask() const 
	{
		ASSERT_LOG(isRGB(), "Asked for RedMask of non-RGB surface.");
		return pf_->Rmask;
	}

	uint32_t SDLPixelFormat::getGreenMask() const 
	{
		ASSERT_LOG(isRGB(), "Asked for GreenMask of non-RGB surface.");
		return pf_->Gmask;
	}

	uint32_t SDLPixelFormat::getBlueMask() const 
	{
		ASSERT_LOG(isRGB(), "Asked for BlueMask of non-RGB surface.");
		return pf_->Bmask;
	}

	uint32_t SDLPixelFormat::getAlphaMask() const 
	{
		return pf_->Amask;
	}

	uint32_t SDLPixelFormat::getLuminanceMask() const 
	{
		ASSERT_LOG(isRGB(), "Asked for LuminanceMask of non-RGB surface.");
		return pf_->Rmask;
	}

	uint8_t SDLPixelFormat::getRedBits() const 
	{
		ASSERT_LOG(isRGB(), "Asked for RedBits() of non-RGB surface.");
		return count_bits_set(pf_->Rmask);
	}

	uint8_t SDLPixelFormat::getGreenBits() const 
	{
		ASSERT_LOG(isRGB(), "Asked for GreenBits() of non-RGB surface.");
		return count_bits_set(pf_->Gmask);
	}

	uint8_t SDLPixelFormat::getBlueBits() const 
	{
		ASSERT_LOG(isRGB(), "Asked for BlueBits() of non-RGB surface.");
		return count_bits_set(pf_->Bmask);
	}

	uint8_t SDLPixelFormat::getAlphaBits() const 
	{
		ASSERT_LOG(isRGB(), "Asked for AlphaBits() of non-RGB surface.");
		return count_bits_set(pf_->Amask);
	}

	uint8_t SDLPixelFormat::getLuminanceBits() const 
	{
		ASSERT_LOG(isRGB(), "Asked for LuminanceBits() of non-RGB surface.");
		return count_bits_set(pf_->Rmask);
	}

	uint32_t SDLPixelFormat::getRedShift() const 
	{
		ASSERT_LOG(isRGB(), "Asked for RedShift() of non-RGB surface.");
		return pf_->Rshift;
	}

	uint32_t SDLPixelFormat::getGreenShift() const 
	{
		ASSERT_LOG(isRGB(), "Asked for GreenShift() of non-RGB surface.");
		return pf_->Gshift;
	}

	uint32_t SDLPixelFormat::getBlueShift() const 
	{
		ASSERT_LOG(isRGB(), "Asked for BlueShift() of non-RGB surface.");
		return pf_->Bshift;
	}

	uint32_t SDLPixelFormat::getAlphaShift() const 
	{
		ASSERT_LOG(isRGB(), "Asked for AlphaShift() of non-RGB surface.");
		return pf_->Ashift;
	}

	uint32_t SDLPixelFormat::getLuminanceShift() const 
	{
		ASSERT_LOG(isRGB(), "Asked for LuminanceShift() of non-RGB surface.");
		return pf_->Rshift;
	}

	uint32_t SDLPixelFormat::getRedLoss() const 
	{
		ASSERT_LOG(isRGB(), "Asked for RedLoss() of non-RGB surface.");
		return pf_->Rloss;
	}

	uint32_t SDLPixelFormat::getGreenLoss() const 
	{
		ASSERT_LOG(isRGB(), "Asked for GreenLoss() of non-RGB surface.");
		return pf_->Gloss;
	}

	uint32_t SDLPixelFormat::getBlueLoss() const 
	{
		ASSERT_LOG(isRGB(), "Asked for BlueLoss() of non-RGB surface.");
		return pf_->Bloss;
	}

	uint32_t SDLPixelFormat::getAlphaLoss() const 
	{
		ASSERT_LOG(isRGB(), "Asked for AlphaLoss() of non-RGB surface.");
		return pf_->Aloss;
	}

	uint32_t SDLPixelFormat::getLuminanceLoss() const 
	{
		ASSERT_LOG(isRGB(), "Asked for LuminanceLoss() of non-RGB surface.");
		return pf_->Rloss;
	}

	bool SDLPixelFormat::hasPalette() const 
	{
		return pf_->palette != NULL;
	}

	Color SDLPixelFormat::mapRGB(int r, int g, int b)
	{
		return Color(SDL_MapRGB(pf_, r, g, b));
	}

	Color SDLPixelFormat::mapRGB(double r, double g, double b)
	{
		return Color(SDL_MapRGB(pf_, static_cast<uint8_t>(r*255.0), static_cast<uint8_t>(g*255.0), static_cast<uint8_t>(b*255.0)));
	}

	Color SDLPixelFormat::mapRGBA(int r, int g, int b, int a)
	{
		return Color(SDL_MapRGBA(pf_, r, g, b, a));
	}

	Color SDLPixelFormat::mapRGBA(double r, double g, double b, double a)
	{
		return Color(SDL_MapRGBA(pf_, static_cast<uint8_t>(r*255.0), static_cast<uint8_t>(g*255.0), static_cast<uint8_t>(b*255.0), static_cast<uint8_t>(a*255.0)));
	}

	void SDLPixelFormat::getRGBA(uint32_t pix, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a)
	{
		SDL_GetRGBA(pix, pf_, &r, &g, &b, &a);
	}

	PixelFormat::PF SDLPixelFormat::getFormat() const
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

	SurfacePtr SurfaceSDL::createFromFile(const std::string& filename, PixelFormat::PF fmt, SurfaceConvertFn fn)
	{
		auto s = IMG_Load(filename.c_str());
		if(s == NULL) {
			LOG_ERROR("Failed to load image file: '" << filename << "' : " << IMG_GetError());
			throw ImageLoadError();
		}
		auto surf = std::make_shared<SurfaceSDL>(s);
		// format means don't convert the surface from the loaded format.
		if(fmt != PixelFormat::PF::PIXELFORMAT_UNKNOWN) {
			return surf->convert(fmt, fn);
		}
		return SurfacePtr(surf);
	}

	std::tuple<int,int> SDLPixelFormat::extractRGBA(const void* pixels, int ndx, uint32_t& red, uint32_t& green, uint32_t& blue, uint32_t& alpha)
	{
		auto fmt = getFormat();
		int pixel_shift_return = bytesPerPixel();
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
				pixel_shift_return = bytesPerPixel();
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
			case PixelFormat::PF::PIXELFORMAT_XRGB8888:
            case PixelFormat::PF::PIXELFORMAT_RGBA8888:
            case PixelFormat::PF::PIXELFORMAT_ABGR8888:
            case PixelFormat::PF::PIXELFORMAT_BGRA8888:
            case PixelFormat::PF::PIXELFORMAT_ARGB2101010: {
				const uint32_t* px = static_cast<const uint32_t*>(pixels);
				if(hasRedChannel()) {
					red = (*px) & getRedMask() >> getRedShift();
				}
				if(hasGreenChannel()) {
					green = (*px) & getGreenMask() >> getGreenShift();
				}
				if(hasBlueChannel()) {
					blue = (*px) & getBlueMask() >> getBlueShift();
				}
				if(hasAlphaChannel()) {
					alpha = (*px) & getAlphaMask() >> getAlphaShift();
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

	void SDLPixelFormat::encodeRGBA(void* pixels, uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha)
	{
		auto fmt = getFormat();
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
			case PixelFormat::PF::PIXELFORMAT_XRGB8888:
            case PixelFormat::PF::PIXELFORMAT_RGBA8888:
            case PixelFormat::PF::PIXELFORMAT_ABGR8888:
            case PixelFormat::PF::PIXELFORMAT_BGRA8888:
            case PixelFormat::PF::PIXELFORMAT_ARGB2101010: {
				uint32_t pixel = 0;
				if(hasRedChannel()) {
					pixel += (red << getRedShift()) & getRedMask();
				}
				if(hasGreenChannel()) {
					pixel += (green << getGreenShift()) & getGreenMask();
				}
				if(hasBlueChannel()) {
					pixel += (blue << getBlueShift()) & getBlueMask();
				}
				if(hasAlphaChannel()) {
					pixel += (alpha << getAlphaShift()) & getAlphaMask();
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

	SurfacePtr SurfaceSDL::handleConvert(PixelFormat::PF fmt, SurfaceConvertFn convert)
	{
		ASSERT_LOG(fmt != PixelFormat::PF::PIXELFORMAT_UNKNOWN, "unknown pixel format to convert to.");
		if(convert == nullptr) {
			SDL_PixelFormat* pf = SDL_AllocFormat(get_sdl_pixel_format(fmt));
			ASSERT_LOG(pf != NULL, "error allocating pixel format: " << SDL_GetError());
			auto surface = new SurfaceSDL(SDL_ConvertSurface(surface_, pf, 0));
			SDL_FreeFormat(pf);
			return SurfacePtr(surface);
		}

		SurfaceLock lock(SurfacePtr(this));
		uint32_t red;
		uint32_t green;
		uint32_t blue;
		uint32_t alpha;

		// Create a destination surface
		auto dst = new SurfaceSDL(width(), height(), fmt);
		size_t dst_size = dst->rowPitch() * dst->height();
		void* dst_pixels = new uint8_t[dst_size];

		for(size_t h = 0; h != height(); ++h) {
			int offs = 0;
			int ndx = 0;
			uint8_t* pixel_ptr = static_cast<uint8_t*>(surface_->pixels) + h*rowPitch();
			uint8_t* dst_pixel_ptr = static_cast<uint8_t*>(dst_pixels) + h*rowPitch();
			while(offs < width()) {
				std::tie(offs, ndx) = getPixelFormat()->extractRGBA(pixel_ptr + offs, ndx, red, green, blue, alpha);
				convert(red, green, blue, alpha);
				dst->getPixelFormat()->encodeRGBA(dst_pixels, red, green, blue, alpha);
			}
		}
		dst->writePixels(dst_pixels);
		delete[] static_cast<uint8_t*>(dst_pixels);
		return SurfacePtr(dst);
	}

	void SurfaceSDL::savePng(const std::string& filename)
	{
		SurfaceLock lock(SurfacePtr(this));
		auto err = IMG_SavePNG(surface_, filename.c_str());
		ASSERT_LOG(err == 0, "Error saving PNG file: " << SDL_GetError());
	}

	void SurfaceSDL::blit(SurfacePtr src, const rect& src_rect) 
	{
		auto src_ptr = std::dynamic_pointer_cast<SurfaceSDL>(src);
		ASSERT_LOG(src_ptr != NULL, "Source pointer was wrong type is not SurfaceSDL");
		SDL_Rect sr = {src_rect.x(), src_rect.y(), src_rect.w(), src_rect.h()};
		SDL_BlitSurface(src_ptr->surface_, &sr, surface_, NULL);
	}

	void SurfaceSDL::blitTo(SurfacePtr src, const rect& src_rect, const rect& dst_rect) 
	{
		auto src_ptr = std::dynamic_pointer_cast<SurfaceSDL>(src);
		ASSERT_LOG(src_ptr != NULL, "Source pointer was wrong type is not SurfaceSDL");
		SDL_Rect sr = {src_rect.x(), src_rect.y(), src_rect.w(), src_rect.h()};
		SDL_Rect dr = {dst_rect.x(), dst_rect.y(), dst_rect.w(), dst_rect.h()};
		SDL_BlitSurface(src_ptr->surface_, &sr, surface_, &dr);
	}

	void SurfaceSDL::blitToScaled(SurfacePtr src, const rect& src_rect, const rect& dst_rect) 
	{
		auto src_ptr = std::dynamic_pointer_cast<SurfaceSDL>(src);
		ASSERT_LOG(src_ptr != NULL, "Source pointer was wrong type is not SurfaceSDL");
		SDL_Rect sr = {src_rect.x(), src_rect.y(), src_rect.w(), src_rect.h()};
		SDL_Rect dr = {dst_rect.x(), dst_rect.y(), dst_rect.w(), dst_rect.h()};
		SDL_BlitScaled(src_ptr->surface_, &sr, surface_, &dr);
	}

	void SurfaceSDL::blitTo(SurfacePtr src, const rect& dst_rect) 
	{
		auto src_ptr = std::dynamic_pointer_cast<SurfaceSDL>(src);
		ASSERT_LOG(src_ptr != NULL, "Source pointer was wrong type is not SurfaceSDL");
		SDL_Rect dr = {dst_rect.x(), dst_rect.y(), dst_rect.w(), dst_rect.h()};
		SDL_BlitScaled(src_ptr->surface_, NULL, surface_, &dr);
	}
}
