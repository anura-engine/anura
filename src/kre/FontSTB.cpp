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

#include "filesystem.hpp"

#include "FontDriver.hpp"
#include "FontImpl.hpp"
#include "utf8_to_codepoint.hpp"

#define STBTT_STATIC
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace KRE
{
	class stb_impl;
	namespace
	{
		const int default_dpi = 96;
		const int surface_width = 512;
		const int surface_height = 512;
	}

	// Implication of non-overlapping ranges.
	struct UnicodeRange
	{
		UnicodeRange() : first(0), last(0) {}
		explicit UnicodeRange(char32_t cp) : first(cp), last(cp) {}
		explicit UnicodeRange(char32_t f, char32_t l) : first(f), last(l) {}
		char32_t first;
		char32_t last;
		int size() const { return last - first + 1; }
		bool operator()(const UnicodeRange& lhs, const UnicodeRange& rhs) const 
		{
			return lhs.last < rhs.first;
		}
	};

	class stb_impl : public FontHandle::Impl, public AlignedAllocator16
	{
	public:
		stb_impl(const std::string& fnt_name, const std::string& fnt_path, float size, const Color& color, bool init_texture)
			: FontHandle::Impl(fnt_name, fnt_path, size, color, init_texture),
			  font_handle_(),
			  font_data_(),
			  ascent_(0),
			  descent_(0),
			  line_gap_(0),
			  baseline_(0),
			  scale_(1.0f),
			  font_size_(default_dpi * size / 72.0f),
			  pc_(),
			  packed_char_(),
			  pixels_(),
			  font_texture_()
		{
			// Read font data and initialise
			font_data_ = sys::read_file(fnt_path);
			auto ttf_buffer = reinterpret_cast<const unsigned char*>(font_data_.c_str());
			stbtt_InitFont(&font_handle_, ttf_buffer, 0);

			scale_ = stbtt_ScaleForPixelHeight(&font_handle_, size);
			int line_gap = 0;
			stbtt_GetFontVMetrics(&font_handle_, &ascent_, &descent_, &line_gap);
			baseline_ = static_cast<int>(ascent_ * scale_);
			line_gap_ = line_gap * scale_;

			float em_scale = stbtt_ScaleForMappingEmToPixels(&font_handle_, size);

			has_kerning_ = font_handle_.kern ? true : false;

			x_height_ = ascent_ * scale_;

			std::ostringstream debug_ss;
			debug_ss << "Loaded font '" << fnt_ << "'\n\tfamily name: '" << "unknown"
				<< "'\n\tnumber of glyphs: " << font_handle_.numGlyphs
				<< "\n\tunits per EM: " << (size / em_scale)
				<< "\n\thas_kerning: " << (has_kerning_ ? "true" : "false")
				;
			LOG_DEBUG(debug_ss.str());

			pixels_.resize(surface_width * surface_height);
			stbtt_PackBegin(&pc_, pixels_.data(), surface_width, surface_height, 0, 1, nullptr);
			if(init_texture) {
				font_texture_ = Texture::createTexture2D(surface_width, surface_height, PixelFormat::PF::PIXELFORMAT_R8);
				font_texture_->setUnpackAlignment(0, 1);
				font_texture_->setFiltering(0, Texture::Filtering::LINEAR, Texture::Filtering::LINEAR, Texture::Filtering::NONE);
				addGlyphsToTexture(FontDriver::getCommonGlyphs());
			}
		}

		~stb_impl() 
		{
			stbtt_PackEnd(&pc_);
		}

		int getDescender() override
		{
			return static_cast<int>(descent_ * scale_ * 65536.0f);
		}

		void getBoundingBox(const std::string& str, long* w, long* h) override
		{
		}

		std::vector<unsigned> getGlyphs(const std::string& text) override		
		{
			std::vector<unsigned> res;
			for(auto cp : utils::utf8_to_codepoint(text)) {
				res.emplace_back(stbtt_FindGlyphIndex(&font_handle_, cp));
			}
			return res;
		}

		const std::vector<point>& getGlyphPath(const std::string& text) override
		{
			auto it = glyph_path_cache_.find(text);
			if(it != glyph_path_cache_.end()) {
				return it->second;
			}
			std::vector<point>& path = glyph_path_cache_[text];

			auto cp_str = utils::utf8_to_codepoint(text);

			std::vector<char32_t> glyphs_to_add;
			for(char32_t cp : cp_str) {
				auto it = packed_char_.find(UnicodeRange(cp));
				if(it == packed_char_.end()) {
					glyphs_to_add.emplace_back(cp);
				}
			}
			if(!glyphs_to_add.empty()) {
				addGlyphsToTexture(glyphs_to_add);
			}

			point pen;
			for(char32_t cp : cp_str) {
				path.emplace_back(pen);
				auto it = packed_char_.find(UnicodeRange(cp));
				if(it == packed_char_.end()) {
					cp = 0xfffd;
					it = packed_char_.find(UnicodeRange(0xfffd));
					if(it == packed_char_.end()) {
						continue;
					}
				}
				
				stbtt_packedchar *b = it->second.data() + cp - it->first.first;
				pen.x += static_cast<int>(b->xadvance * 65536.0f);
			}
			path.emplace_back(pen);

			return path;
		}

		FontRenderablePtr createRenderableFromPath(FontRenderablePtr font_renderable, const std::string& text, const std::vector<point>& path) override
		{			
			auto cp_string = utils::utf8_to_codepoint(text);
			int glyphs_in_text = 0;
			std::vector<char32_t> glyphs_to_add;
			for(char32_t cp : cp_string) {
				++glyphs_in_text;

				auto it = packed_char_.find(UnicodeRange(cp));
				if(it == packed_char_.end()) {
					glyphs_to_add.emplace_back(cp);
				}
			}
			if(!glyphs_to_add.empty()) {
				addGlyphsToTexture(glyphs_to_add);
			}
			
			if(font_renderable == nullptr) {
				font_renderable = std::make_shared<FontRenderable>();
				font_renderable->setTexture(font_texture_);
			}

			int width = font_renderable->getWidth();
			int height = font_renderable->getHeight();
			int max_height = 0;

			std::vector<font_coord> coords;
			coords.reserve(glyphs_in_text * 6);
			int n = 0;
			for(char32_t cp : cp_string) {
				ASSERT_LOG(n < static_cast<int>(path.size()), "Insufficient points were supplied to create a path from the string '" << text << "'");
				auto& pt =path[n];
				auto it = packed_char_.find(UnicodeRange(cp));
				if(it == packed_char_.end()) {
					it = packed_char_.find(UnicodeRange(0xfffd));
					if(it == packed_char_.end()) {
						continue;
					}
				}

				stbtt_packedchar *b = it->second.data() + cp - it->first.first;

				//width += pt.x >> 16;
				//width += static_cast<int>(b->xoff2 - b->xoff);
				max_height = std::max(max_height, static_cast<int>(b->yoff2 - b->yoff));

				const float u1 = font_texture_->getTextureCoordW(0, b->x0);
				const float v1 = font_texture_->getTextureCoordH(0, b->y0);
				const float u2 = font_texture_->getTextureCoordW(0, b->x1);
				const float v2 = font_texture_->getTextureCoordH(0, b->y1);

				const float x1 = static_cast<float>(pt.x) / 65536.0f + b->xoff;
				const float y1 = static_cast<float>(pt.y) / 65536.0f + b->yoff;
				const float x2 = x1 + b->xoff2 - b->xoff;
				const float y2 = y1 + b->yoff2 - b->yoff;
				coords.emplace_back(glm::vec2(x1, y2), glm::vec2(u1, v2));
				coords.emplace_back(glm::vec2(x1, y1), glm::vec2(u1, v1));
				coords.emplace_back(glm::vec2(x2, y1), glm::vec2(u2, v1));

				coords.emplace_back(glm::vec2(x2, y1), glm::vec2(u2, v1));
				coords.emplace_back(glm::vec2(x1, y2), glm::vec2(u1, v2));
				coords.emplace_back(glm::vec2(x2, y2), glm::vec2(u2, v2));
				++n;
			}
			height += max_height;
			width = std::max(width, path.back().x >> 16);

			font_renderable->setWidth(width);
			font_renderable->setHeight(height);
			font_renderable->update(&coords);
			return font_renderable;
		}

		ColoredFontRenderablePtr createColoredRenderableFromPath(ColoredFontRenderablePtr font_renderable, const std::string& text, const std::vector<point>& path, const std::vector<KRE::Color>& colors) override
		{
			auto cp_string = utils::utf8_to_codepoint(text);
			int glyphs_in_text = 0;
			std::vector<char32_t> glyphs_to_add;
			for(char32_t cp : cp_string) {
				++glyphs_in_text;

				auto it = packed_char_.find(UnicodeRange(cp));
				if(it == packed_char_.end()) {
					glyphs_to_add.emplace_back(cp);
				}
			}
			if(!glyphs_to_add.empty()) {
				addGlyphsToTexture(glyphs_to_add);
			}
			ASSERT_LOG(glyphs_in_text == colors.size(), "Not enough/Too many colors for the text.");
			
			if(font_renderable == nullptr) {
				font_renderable = std::make_shared<ColoredFontRenderable>();
				font_renderable->setTexture(font_texture_);
			}

			int width = font_renderable->getWidth();
			int height = font_renderable->getHeight();
			int max_height = 0;

			std::vector<font_coord> coords;
			coords.reserve(glyphs_in_text * 6);
			int n = 0;
			for(char32_t cp : cp_string) {
				ASSERT_LOG(n < static_cast<int>(path.size()), "Insufficient points were supplied to create a path from the string '" << text << "'");
				auto& pt =path[n];
				auto it = packed_char_.find(UnicodeRange(cp));
				if(it == packed_char_.end()) {
					it = packed_char_.find(UnicodeRange(0xfffd));
					if(it == packed_char_.end()) {
						continue;
					}
				}

				stbtt_packedchar *b = it->second.data() + cp - it->first.first;

				//width += pt.x >> 16;
				//width += static_cast<int>(b->xoff2 - b->xoff);
				max_height = std::max(max_height, static_cast<int>(b->yoff2 - b->yoff));

				const float u1 = font_texture_->getTextureCoordW(0, b->x0);
				const float v1 = font_texture_->getTextureCoordH(0, b->y0);
				const float u2 = font_texture_->getTextureCoordW(0, b->x1);
				const float v2 = font_texture_->getTextureCoordH(0, b->y1);

				const float x1 = static_cast<float>(pt.x) / 65536.0f + b->xoff;
				const float y1 = static_cast<float>(pt.y) / 65536.0f + b->yoff;
				const float x2 = x1 + b->xoff2 - b->xoff;
				const float y2 = static_cast<float>(pt.y) / 65536.0f + b->yoff2;
				coords.emplace_back(glm::vec2(x1, y2), glm::vec2(u1, v2));
				coords.emplace_back(glm::vec2(x1, y1), glm::vec2(u1, v1));
				coords.emplace_back(glm::vec2(x2, y1), glm::vec2(u2, v1));

				coords.emplace_back(glm::vec2(x2, y1), glm::vec2(u2, v1));
				coords.emplace_back(glm::vec2(x1, y2), glm::vec2(u1, v2));
				coords.emplace_back(glm::vec2(x2, y2), glm::vec2(u2, v2));
				++n;
			}
			height += max_height;
			width = std::max(width, path.back().x >> 16);

			font_renderable->setWidth(width);
			font_renderable->setHeight(height);
			font_renderable->update(&coords);
			font_renderable->setVerticesPerColor(6);
			font_renderable->updateColors(colors);
			return font_renderable;
		}

		long calculateCharAdvance(char32_t cp) override
		{
			//int advance = 0;
			//int bearing = 0;
			//stbtt_GetCodepointHMetrics(&font_handle_, cp, &advance, &bearing);
			//return static_cast<int>(advance * scale_ * 65536.0f);
			auto it = packed_char_.find(UnicodeRange(cp));
			if(it == packed_char_.end()) {
				int advance = 0;
				int bearing = 0;
				stbtt_GetCodepointHMetrics(&font_handle_, cp, &advance, &bearing);
				return static_cast<int>(advance * scale_ * 65536.0f);
			}		
			stbtt_packedchar *b = it->second.data() + cp - it->first.first;
			return static_cast<int>(b->xadvance * 65536.0f);
		}

		void addGlyphsToTexture(const std::vector<char32_t>& codepoints) override
		{
			if(codepoints.empty()) {
				LOG_WARN("stb_impl::addGlyphsToTexture: no codepoints.");
				return;
			}
			int old_size = packed_char_.size();
			auto ttf_buffer = reinterpret_cast<const unsigned char*>(font_data_.c_str());
			if(font_size_ < 20.0f) {
				stbtt_PackSetOversampling(&pc_, 2, 2);
			}
			
			std::vector<stbtt_pack_range> ranges;
			char32_t last_cp = codepoints.front();
			char32_t first_cp = codepoints.front();
			int num_chars = 1;
			for(auto it = codepoints.begin() + 1; it != codepoints.end(); ++it) {
				char32_t cp = *it;
				if(cp == last_cp+1) {
					++num_chars;
				} else {
					auto& packed_data = packed_char_[UnicodeRange(first_cp, first_cp+num_chars-1)];
					packed_data.resize(num_chars);

					stbtt_pack_range range;
					range.num_chars_in_range          = num_chars;
					range.chardata_for_range          = packed_data.data();
					range.font_size                   = font_size_;
					range.first_unicode_char_in_range = first_cp;
					ranges.emplace_back(range);

					num_chars = 1;
					first_cp = cp;					
				}
				last_cp = cp;
			}
			auto& packed_data = packed_char_[UnicodeRange(first_cp, first_cp+num_chars-1)];
			packed_data.resize(num_chars);

			stbtt_pack_range range;
			range.num_chars_in_range          = num_chars;
			range.chardata_for_range          = packed_data.data();
			range.font_size                   = font_size_;
			range.first_unicode_char_in_range = first_cp;
			ranges.emplace_back(range);

			stbtt_PackFontRanges(&pc_, ttf_buffer, 0, ranges.data(), ranges.size());

			font_texture_->update2D(0, 0, 0, surface_width, surface_height, surface_width, pixels_.data());
		}

		void* getRawFontHandle() override
		{
			return &font_handle_;
		}

		float getLineGap() const override
		{
			return line_gap_;
		}
	private:
		stbtt_fontinfo font_handle_;
		std::string font_data_;
		int ascent_;
		int descent_;
		int baseline_;
		float scale_;
		float font_size_;
		float line_gap_;
		stbtt_pack_context pc_;
		std::map<UnicodeRange, std::vector<stbtt_packedchar>, UnicodeRange> packed_char_;
		std::vector<unsigned char> pixels_;
		TexturePtr font_texture_;
	};

	FontDriverRegistrar stb_font_impl("stb", [](const std::string& fnt_name, const std::string& fnt_path, float size, const Color& color, bool init_texture){ 
		return std::unique_ptr<stb_impl>(new stb_impl(fnt_name, fnt_path, size, color, init_texture));
	});
}
