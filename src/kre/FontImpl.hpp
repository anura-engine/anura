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

#pragma once

#include "FontDriver.hpp"

namespace KRE
{
	class FontHandle::Impl
	{
	public:
		Impl(const std::string& fnt_name, const std::string& fnt_path, float size, const Color& color, bool init_texture)
			: fnt_(fnt_name),
			  fnt_path_(fnt_path),
			  size_(size),
			  color_(color),
			  has_kerning_(false),
			  x_height_(0),
			  glyph_path_cache_()
		{
		}
		virtual ~Impl() {}
		virtual int getDescender() = 0;
		virtual void getBoundingBox(const std::string& str, long* w, long* h) = 0;
		virtual std::vector<unsigned> getGlyphs(const std::string& text) = 0;
		virtual const std::vector<point>& getGlyphPath(const std::string& text) = 0;
		virtual FontRenderablePtr createRenderableFromPath(FontRenderablePtr font_renderable, const std::string& text, const std::vector<point>& path) = 0;
		virtual ColoredFontRenderablePtr createColoredRenderableFromPath(ColoredFontRenderablePtr r, const std::string& text, const std::vector<point>& path, const std::vector<KRE::Color>& colors) = 0;
		virtual long calculateCharAdvance(char32_t cp) = 0;
		virtual void addGlyphsToTexture(const std::vector<char32_t>& glyphs) = 0;
		virtual void* getRawFontHandle() = 0;
		virtual float getLineGap() const = 0;
	protected:
		std::string fnt_;
		std::string fnt_path_;
		float size_;
		Color color_;
		bool has_kerning_;
		float x_height_;
		std::map<std::string, std::vector<point>> glyph_path_cache_;
		friend class FontHandle;
	};
}
