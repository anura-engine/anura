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

#include <tuple>

#include "Surface.hpp"
#include "SDL.h"

namespace KRE
{
	class SDLPixelFormat : public PixelFormat
	{
	public:
		SDLPixelFormat(Uint32 pf);
		virtual ~SDLPixelFormat();

		uint8_t bitsPerPixel() const override;
		uint8_t bytesPerPixel() const override;

		bool isYuvPlanar() const override;
		bool isYuvPacked() const override;
		bool isYuvHeightReversed() const override;
		bool isInterlaced() const override;
		
		bool isRGB() const override;
		bool hasRedChannel() const override;
		bool hasGreenChannel() const override;
		bool hasBlueChannel() const override;
		bool hasAlphaChannel() const override;
		bool hasLuminance() const override;

		uint32_t getRedMask() const override;
		uint32_t getGreenMask() const override;
		uint32_t getBlueMask() const override;
		uint32_t getAlphaMask() const override;
		uint32_t getLuminanceMask() const override;

		uint8_t getRedBits() const override;
		uint8_t getGreenBits() const override;
		uint8_t getBlueBits() const override;
		uint8_t getAlphaBits() const override;
		uint8_t getLuminanceBits() const override;

		uint32_t getRedShift() const override;
		uint32_t getGreenShift() const override;
		uint32_t getBlueShift() const override;
		uint32_t getAlphaShift() const override;
		uint32_t getLuminanceShift() const override;

		uint32_t getRedLoss() const override;
		uint32_t getGreenLoss() const override;
		uint32_t getBlueLoss() const override;
		uint32_t getAlphaLoss() const override;
		uint32_t getLuminanceLoss() const override;

		PixelFormat::PF getFormat() const override;

		Color mapRGB(int r, int g, int b) override;
		Color mapRGB(float r, float g, float b) override;
		Color mapRGBA(int r, int g, int b, int a) override;
		Color mapRGBA(float r, float g, float b, float a) override;

		void getRGBA(uint32_t pix, int& r, int& g, int& b, int& a) override;

		void extractRGBA(const void* pixels, int ndx, int& red, int& green, int& blue, int& alpha) override;
		void encodeRGBA(void* pixels, int red, int green, int blue, int alpha) override; 

		bool hasPalette() const override;
		SDL_PixelFormat* get() { return pf_; }
	private:
		SDL_PixelFormat* pf_;
		SDLPixelFormat(const SDLPixelFormat&);
	};


	class SurfaceSDL : public Surface
	{
	public:
		SurfaceSDL(int width, 
			int height, 
			int bpp, 
			int row_pitch, 
			uint32_t rmask, 
			uint32_t gmask, 
			uint32_t bmask, 
			uint32_t amask, 
			const void* pixels);
		SurfaceSDL(int width, 
			int height, 
			int bpp, 
			uint32_t rmask, 
			uint32_t gmask, 
			uint32_t bmask, 
			uint32_t amask);
		SurfaceSDL(const std::string& filename);
		SurfaceSDL(SDL_Surface* surface);
		SurfaceSDL(int width, int height, PixelFormat::PF format);
		virtual ~SurfaceSDL();
		const void* pixels() const override;
		void* pixelsWriteable() override;
		void writePixels(int bpp, 
			uint32_t rmask, 
			uint32_t gmask, 
			uint32_t bmask, 
			uint32_t amask,
			const void* pixels) override;
		void writePixels(const void* pixels, int size) override;
		void fillRect(const rect& dst_rect, const Color& color) override;
		int width() const override {
			ASSERT_LOG(surface_ != nullptr, "surface_ is null");
			return surface_->w;
		}
		int height() const override {
			ASSERT_LOG(surface_ != nullptr, "surface_ is null");
			return surface_->h;
		}
		int rowPitch() const override {
			ASSERT_LOG(surface_ != nullptr, "surface_ is null");
			return surface_->pitch;
		}
		int bytesPerPixel() const override {
			ASSERT_LOG(surface_ != nullptr, "surface_ is null");
			ASSERT_LOG(surface_->format != nullptr, "surface_->format is null");
			return surface_->format->BytesPerPixel;
		}
		int bitsPerPixel() const override {
			ASSERT_LOG(surface_ != nullptr, "surface_ is null");
			ASSERT_LOG(surface_->format != nullptr, "surface_->format is null");
			return surface_->format->BitsPerPixel;
		}

		virtual bool hasData() const override {
			if(surface_ == nullptr) {
				return false;
			}
			return has_data_;
		}

		void blit(SurfacePtr src, const rect& src_rect) override;
		void blitTo(SurfacePtr src, const rect& src_rect, const rect& dst_rect) override;
		void blitTo(SurfacePtr src, const rect& dst_rect) override;
		void blitToScaled(SurfacePtr src, const rect& src_rect, const rect& dst_rect) override;

		const std::vector<Color>& getPalette() override { return palette_; }

		void setBlendMode(BlendMode bm) override;
		BlendMode getBlendMode() const override;

		bool setClipRect(int x, int y, unsigned width, unsigned height) override;
		void getClipRect(int& x, int& y, unsigned& width, unsigned& height) override;
		bool setClipRect(const rect& r) override;
		const rect getClipRect() override;

		static SurfacePtr createFromFile(const std::string&, PixelFormat::PF fmt, SurfaceFlags flags, SurfaceConvertFn fn);
		static SurfacePtr createFromPixels(int width, 
			int height, 
			int bpp, 
			int row_pitch, 
			uint32_t rmask, 
			uint32_t gmask, 
			uint32_t bmask, 
			uint32_t amask, 
			const void* pixels);
		static SurfacePtr createFromMask(int width, 
			int height, 
			int bpp, 
			uint32_t rmask, 
			uint32_t gmask, 
			uint32_t bmask, 
			uint32_t amask);
		static SurfacePtr createFromFormat(int width,
			int height,
			PixelFormat::PF fmt);

		void lock() override;
		void unlock() override;

		std::string savePng(const std::string& filename) override;

		CursorPtr createCursorFromSurface(int hot_x, int hot_y) override;

		SDL_Surface* get() { return surface_; }
		const SDL_Surface* get() const { return surface_; }
	private:
		SurfacePtr handleConvert(PixelFormat::PF fmt, SurfaceConvertFn convert) override;
		SurfacePtr runGlobalAlphaFilter() override;
		void createPalette();

		SDL_Surface* surface_;
		bool has_data_;
		std::vector<Color> palette_;
		SurfaceSDL();
	};
}
