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

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H
#include FT_LCD_FILTER_H
#include FT_BBOX_H

#include <unordered_map>

#include "formatter.hpp"
#include "FontFreetype.hpp"
#include "utf8_to_codepoint.hpp"

#include "DisplayDevice.hpp"
#include "SceneObject.hpp"
#include "Shaders.hpp"

namespace KRE
{
	namespace
	{
		const int default_dpi = 96;
		const int surface_width = 2048;
		const int surface_height = 2048;

		font_path_cache& get_font_path_cache()
		{
			static font_path_cache res;
			return res;
		}

		font_path_cache& generic_font_lookup()
		{
			static font_path_cache res;
			if(res.empty()) {
				// XXX ideally we read this from a config file.
				res["serif"] = "FreeSerif.ttf";
				res["sans-serif"] = "FreeSans.ttf";
				res["cursive"] = "Allura-Regular.ttf";
				res["fantasy"] = "TradeWinds-Regular.ttf";
				res["monospace"] = "SourceCodePro-Regular.ttf";
			}
			return res;
		}

		FT_Library& get_ft_library()
		{
			static FT_Library library = nullptr;
			if(library == nullptr) {
				FT_Error error = FT_Init_FreeType(&library);
				ASSERT_LOG(error == 0, "Unable to initialise freetype library: " << error);				
			}
			return library;
		}

		struct CacheKey
		{
			CacheKey(const std::string& fn, float sz) : font_name(fn), size(sz) {}
			std::string font_name;
			float size;
			bool operator<(const CacheKey& other) const {
				return font_name == other.font_name ? size < other.size : font_name < other.font_name;
			}
		};

		typedef std::map<CacheKey, FontHandlePtr> font_cache;
		font_cache& get_font_cache()
		{
			static font_cache res;
			return res;
		}

		// Returns a list of what we consider 'common' codepoints
		// these generally consist of the 7-bit ASCII characters.
		// and the unicode replacement character 0xfffd
		std::vector<char32_t>& get_common_glyphs()
		{
			static std::vector<char32_t> res;
			if(res.empty()) {
				// replace character.
				res.emplace_back(0xfffd);
				for(char32_t n = 0x21; n < 0x7f; ++n) {
					res.emplace_back(n);
				}
			}
			return res;
		}
	}

	FontRenderable::FontRenderable() 
		: SceneObject("font-renderable")
	{
		ShaderProgramPtr shader = ShaderProgram::getProgram("font_shader");
		setShader(shader);
		auto as = DisplayDevice::createAttributeSet();
		attribs_.reset(new Attribute<font_coord>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
		attribs_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(font_coord), offsetof(font_coord, vtx)));
		attribs_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE,  2, AttrFormat::FLOAT, false, sizeof(font_coord), offsetof(font_coord, tc)));
		as->addAttribute(AttributeBasePtr(attribs_));
		as->setDrawMode(DrawMode::TRIANGLES);
		as->clearblendState();
		as->clearBlendMode();

		addAttributeSet(as);

		int u_ignore_alpha = shader->getUniform("ignore_alpha");
		shader->setUniformDrawFunction([u_ignore_alpha](ShaderProgramPtr shader) {
			shader->setUniformValue(u_ignore_alpha, 0);
		});				
	}

	void FontRenderable::preRender(const WindowPtr& wnd)
	{
		//ASSERT_LOG(color_ != nullptr, "Color pointer was null.");
		if(color_ != nullptr) {
			setColor(*color_);
		}
	}

	void FontRenderable::setColorPointer(const ColorPtr& color) 
	{
		ASSERT_LOG(color != nullptr, "Font color was null.");
		color_ = color;
	}

	void FontRenderable::update(std::vector<font_coord>* queue)
	{
		attribs_->update(queue, attribs_->end());
	}

	void FontRenderable::clear()
	{
		attribs_->clear();
	}

	FontDriver::FontDriver()
	{
	}

	void FontDriver::setAvailableFonts(const font_path_cache& font_map)
	{
		get_font_path_cache() = font_map;
	}

	FontHandlePtr FontDriver::getFontHandle(const std::vector<std::string>& font_list, float size, const Color& color, bool init_texture)
	{
		std::string selected_font;
		std::string font_path;
		for(auto& fnt : font_list) {
			auto it = get_font_path_cache().find(fnt);
			if(it != get_font_path_cache().end()) {
				selected_font = it->first;
				font_path = it->second;
				break;
			} else {
				it = get_font_path_cache().find(fnt + ".ttf");
				if(it != get_font_path_cache().end()) {
					selected_font = it->first;
					font_path = it->second;
					break;
				} else {
					it = get_font_path_cache().find(fnt + ".otf");
					if(it != get_font_path_cache().end()) {
						selected_font = it->first;
						font_path = it->second;
						break;
					} else {
						it = generic_font_lookup().find(fnt);
						if(it != generic_font_lookup().end()) {
							it = get_font_path_cache().find(it->second);
							if(it != get_font_path_cache().end()) {
								selected_font = it->first;
								font_path = it->second;
								break;
							}
						}
					}
				}
			}
		}

		if(font_path.empty()) {
			std::ostringstream ss;
			ss << "Unable to find a font to match in the given list:";
			for(auto& fnt : font_list) {
				ss << " " << fnt;
			}
			throw FontError2(ss.str());
		}

		auto it = get_font_cache().find(CacheKey(font_path, size));
		if(it != get_font_cache().end()) {
			return it->second;
		}
		auto fh = std::make_shared<FontHandle>(selected_font, font_path, size, color, init_texture);
		get_font_cache()[CacheKey(font_path, size)] = fh;
		return fh;
	}

	class FontHandle::Impl
	{
	public:
		Impl(const std::string& fnt_name, const std::string& fnt_path, float size, const Color& color, bool init_texture)
			: fnt_(fnt_name),
			  fnt_path_(fnt_path),
			  size_(size),
			  color_(color),
			  face_(nullptr),
			  has_kerning_(false),
			  x_height_(0),
			  font_texture_(),
			  next_font_x_(0),
			  next_font_y_(0),
			  last_line_height_(0),
			  glyph_info_(),
			  all_glyphs_added_(false),
			  font_load_flags_(FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT),
			  glyph_path_cache_()
		{
			// XXX starting off with a basic way of rendering glyphs.
			// It'd be better to render all the glyphs to a texture,
			// then use a VBO to store texture co-ords for indivdual glyphs and 
			// vertex co-ords where to place them. Thus we could always use the 
			// same texture for a particular font.
			// More advanded ideas involve decomposing glyph outlines, triangulating 
			// and storing those in a VBO for rendering.
			auto lib = get_ft_library();
			FT_Error error = FT_New_Face(lib, fnt_path_.c_str(), 0, &face_);
			ASSERT_LOG(error == 0, "Error reading font file: " << fnt_name << ", error was: " << error);
			error = FT_Set_Char_Size(face_, static_cast<int>(size * 64), 0, default_dpi, 0);
			LOG_DEBUG("FT_Set_Char_Size: " << static_cast<int>(size * 64));
			ASSERT_LOG(error == 0, "Error setting character size, file: " << fnt_name << ", error was: " << error);
			has_kerning_ = FT_HAS_KERNING(face_) ? true : false;
			std::ostringstream debug_ss;
			debug_ss << "Loaded font '" << fnt_ << "'\n\tfamily name: '" << face_->family_name 
				<< "'\n\tnumber of glyphs: " << face_->num_glyphs
				<< "\n\tunits per EM: " << face_->units_per_EM 
				<< "\n\thas_kerning: " 
				<< (has_kerning_ ? "true" : "false");
			LOG_DEBUG(debug_ss.str());

			FT_UInt glyph_index = FT_Get_Char_Index(face_, 'x');
			FT_Load_Glyph(face_, glyph_index, font_load_flags_);
			x_height_ = face_->glyph->metrics.height / 64.0f;

			if(init_texture) {
				// This is an empirical fudge that just adds all the glyphs in the
				// font to the texture on the caveat that they will fit.
				float px_sz = size / 72.0f * default_dpi;
				if((surface_width / px_sz) * (surface_height / px_sz) > face_->num_glyphs) {
					addAllGlyphsToTexture();
				} else {
					addGlyphsToTexture(get_common_glyphs());
				}
			}
		}
		~Impl() 
		{
			if(face_) {
				FT_Done_Face(face_);
				face_ = nullptr;
			}
		}
		FT_Pos getDescender() 
		{
			return face_->size->metrics.descender * (65536/64);
		}
		void getBoundingBox(const std::string& str, long* w, long* h) 
		{
			FT_GlyphSlot slot = face_->glyph;
			FT_UInt previous_glyph = 0;
			ASSERT_LOG(w != nullptr && h != nullptr, "w or h is nullptr");
			FT_Vector pen = { 0, 0 };
			for(char32_t cp : utils::utf8_to_codepoint(str)) {
				FT_UInt glyph_index = FT_Get_Char_Index(face_, cp);
				if(has_kerning_ && previous_glyph && glyph_index) {
					FT_Vector  delta;
					FT_Get_Kerning(face_, previous_glyph, glyph_index, FT_KERNING_DEFAULT, &delta);
					pen.x += delta.x;
				}
				FT_Error error;
				if((error = FT_Load_Glyph(face_, glyph_index, font_load_flags_)) != 0) {
					continue;
				}
				pen.x += slot->linearHoriAdvance;
				pen.y += 0;
				previous_glyph = glyph_index;
			}
			// This is to ensure that the returned dimensions are tight, i.e. the final advance is replaced by the width of the character.
			*w = (pen.x - slot->linearHoriAdvance + slot->metrics.width*65536L);
			*h = (pen.y - slot->linearHoriAdvance + slot->metrics.height*65536L);
		}

		std::vector<unsigned> getGlyphs(const std::string& text)
		{
			std::vector<unsigned> res;
			for(auto cp : utils::utf8_to_codepoint(text)) {
				res.emplace_back(FT_Get_Char_Index(face_, cp));
			}
			return res;
		}

		const std::vector<point>& getGlyphPath(const std::string& text)
		{
			auto it = glyph_path_cache_.find(text);
			if(it != glyph_path_cache_.end()) {
				return it->second;
			}
			std::vector<point>& path = glyph_path_cache_[text];

			FT_Vector pen = { 0, 0 };
			FT_Error error;
			FT_UInt previous_glyph = 0;
			FT_Pos  prev_rsb_delta = 0;
			for(char32_t cp : utils::utf8_to_codepoint(text)) {
				path.emplace_back(pen.x, pen.y);
				FT_UInt glyph_index = FT_Get_Char_Index(face_, cp);
				if(has_kerning_ && previous_glyph && glyph_index) {
					static char32_t previous_cp = 0;
					FT_Vector delta;
					FT_Get_Kerning(face_, previous_glyph, glyph_index, FT_KERNING_UNFITTED, &delta);
					//if(delta.x != 0) {
					//	LOG_DEBUG("Kerning between: '" << char(previous_cp) << "' and '" << char(cp) << "' : " << delta.x);
					//}
					pen.x += static_cast<long>(delta.x) << 6;
					previous_cp = cp;
				}
				if((error = FT_Load_Glyph(face_, glyph_index, font_load_flags_)) != 0) {
					continue;
				}
				FT_GlyphSlot slot = face_->glyph;

				/*if(prev_rsb_delta - slot->lsb_delta >= 32) {
					pen.x -= 65536;
				} else if(prev_rsb_delta - slot->lsb_delta < -32) {
					pen.x += 65536;
				}
				prev_rsb_delta = slot->rsb_delta; */

				pen.x += slot->linearHoriAdvance;
				pen.y += 0;//slot->linearVertAdvance;

				previous_glyph = glyph_index;
			}
			// pushing back the end point so we know where the next letter starts.
			path.emplace_back(pen.x, pen.y);
			return path;
		}
		
		// text is a utf-8 string, path is expected to have at least has many data points as there
		// are codepoints in the string. path should be in units consist with FT_Pos
		// N.B. the origin of the Renderable object created is the baseline of the font
		FontRenderablePtr createRenderableFromPath(FontRenderablePtr font_renderable, const std::string& text, const std::vector<point>& path)
		{
			auto cp_string = utils::utf8_to_codepoint(text);
			int glyphs_in_text = 0;
			std::vector<char32_t> glyphs_to_add;
			for(char32_t cp : cp_string) {
				++glyphs_in_text;
				auto it = glyph_info_.find(cp);
				if(it == glyph_info_.end()) {
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

			int width = 0;
			int height = 0;

			std::vector<font_coord> coords;
			coords.reserve(glyphs_in_text * 6);
			int n = 0;
			for(char32_t cp : cp_string) {
				ASSERT_LOG(n < static_cast<int>(path.size()), "Insufficient points were supplied to create a path from the string '" << text << "'");
				auto& pt =path[n];
				auto it = glyph_info_.find(cp);
				if(it == glyph_info_.end()) {
					it = glyph_info_.find(0xfffd);
					if(it == glyph_info_.end()) {
						continue;
					}
				}
				GlyphInfo& gi = it->second;
				
				width += gi.width;
				height = std::max(height, static_cast<int>(gi.height));

				const float u1 = font_texture_->getTextureCoordW(0, gi.tex_x);
				const float v1 = font_texture_->getTextureCoordH(0, gi.tex_y);
				const float u2 = font_texture_->getTextureCoordW(0, gi.tex_x + gi.width);
				const float v2 = font_texture_->getTextureCoordH(0, gi.tex_y + gi.height);

				const float x1 = static_cast<float>(pt.x) / 65536.0f;
				const float y1 = static_cast<float>(pt.y) / 65536.0f - gi.bearing_y/64.0f;
				const float x2 = x1 + static_cast<float>(gi.width);
				const float y2 = y1 + static_cast<float>(gi.height);
				coords.emplace_back(glm::vec2(x1, y2), glm::vec2(u1, v2));
				coords.emplace_back(glm::vec2(x1, y1), glm::vec2(u1, v1));
				coords.emplace_back(glm::vec2(x2, y1), glm::vec2(u2, v1));

				coords.emplace_back(glm::vec2(x2, y1), glm::vec2(u2, v1));
				coords.emplace_back(glm::vec2(x1, y2), glm::vec2(u1, v2));
				coords.emplace_back(glm::vec2(x2, y2), glm::vec2(u2, v2));
				++n;
			}

			font_renderable->setWidth(width);
			font_renderable->setHeight(height);
			font_renderable->update(&coords);
			return font_renderable;
		}

		long calculateCharAdvance(char32_t cp)
		{
			FT_Error error;
			FT_GlyphSlot slot = face_->glyph;
			if((error = FT_Load_Char(face_, cp, font_load_flags_)) != 0) {
				return 0;
			}
			return slot->linearHoriAdvance;
		}

		// Adds all the glyphs in the font to the texture.
		// Assumes you've calculated that they'll all fit.
		void addAllGlyphsToTexture()
		{
			std::vector<char32_t> glyphs;
			for(int n = 0; n != face_->num_glyphs; ++n) {
			}
			addGlyphsToTexture(glyphs);
			all_glyphs_added_ = true;
		}

		void addGlyphsToTexture(const std::vector<char32_t>& glyphs) 
		{
			/*const FT_Matrix shear = { // xx xy || yx yy
				1 << 16, static_cast<int>(13566.0f/size_),
				0,       1<< 16,
			};*/

			if(font_texture_ == nullptr) {
				// XXX if slot->bitmap.pixel_mode == FT_PIXEL_MODE_LCD then allocate a RGBA surface
				font_texture_ = Texture::createTexture2D(surface_width, surface_height, PixelFormat::PF::PIXELFORMAT_R8);
				font_texture_->setUnpackAlignment(0, 1);
				next_font_x_ = next_font_y_ = 0;
			}
			FT_Error error;
			FT_GlyphSlot slot = face_->glyph;
			// use a simple packing algorithm.
			for(auto& cp : glyphs) {
				if(glyph_info_.find(cp) != glyph_info_.end()) {
					continue;
				}
				if((error = FT_Load_Char(face_, cp, font_load_flags_/*&~FT_LOAD_RENDER*/)) != 0) {
					LOG_ERROR("Font '" << fnt_ << "' does not contain glyph for: " << utils::codepoint_to_utf8(cp));
					continue;
				}
				//FT_Outline_Transform(&slot->outline, &shear);
				//FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
				if(slot->bitmap.buffer == nullptr) {
					continue;
				}

				GlyphInfo& gi = glyph_info_[cp];
				gi.width = static_cast<unsigned short>(slot->metrics.width/64);
				gi.height = static_cast<unsigned short>(slot->metrics.height/64);
				gi.advance_x = slot->linearHoriAdvance;
				gi.advance_y = 0;
				gi.bearing_x = slot->metrics.horiBearingX;
				gi.bearing_y = slot->metrics.horiBearingY;
				last_line_height_ = std::max(last_line_height_, gi.height);
				if(gi.width + next_font_x_ > surface_width) {
					next_font_x_ = 0;
					next_font_y_ += last_line_height_;
					ASSERT_LOG(next_font_y_ < surface_height, "This font would exceed to maximum surface size. " 
						<< surface_width << "x" << surface_height << ", number of glyphs: " << glyph_info_.size());
				}
				gi.tex_x = next_font_x_;
				gi.tex_y = next_font_y_;

				switch(slot->bitmap.pixel_mode) {
					case FT_PIXEL_MODE_MONO: {
						const int pixel_count = slot->bitmap.pitch * slot->bitmap.rows;
						std::vector<uint8_t> pixels(pixel_count, 0);
						for(int n = 0; n != pixel_count; n += 8) {
							pixels[n+0] = (slot->bitmap.buffer[n] & 128) ? 255 : 0;
							pixels[n+1] = (slot->bitmap.buffer[n] &  64) ? 255 : 0;
							pixels[n+2] = (slot->bitmap.buffer[n] &  32) ? 255 : 0;
							pixels[n+3] = (slot->bitmap.buffer[n] &  16) ? 255 : 0;
							pixels[n+4] = (slot->bitmap.buffer[n] &   8) ? 255 : 0;
							pixels[n+5] = (slot->bitmap.buffer[n] &   4) ? 255 : 0;
							pixels[n+6] = (slot->bitmap.buffer[n] &   2) ? 255 : 0;
							pixels[n+7] = (slot->bitmap.buffer[n] &   1) ? 255 : 0;
						}
						font_texture_->update2D(0, next_font_x_, next_font_y_, gi.width, gi.height, slot->bitmap.pitch, &pixels[0]);
						break;
					}
					case FT_PIXEL_MODE_GRAY:
						font_texture_->update2D(0, next_font_x_, next_font_y_, gi.width, gi.height, slot->bitmap.pitch, slot->bitmap.buffer);
						break;
					case FT_PIXEL_MODE_LCD:
					case FT_PIXEL_MODE_GRAY2:
					case FT_PIXEL_MODE_GRAY4:
					case FT_PIXEL_MODE_LCD_V:
					/* case FT_PIXEL_MODE_BGRA: */
					default:
						ASSERT_LOG(false, "Unhandled font pixel mode: " << slot->bitmap.pixel_mode);
						break;
				}
				next_font_x_ += gi.width;
			}
		}
		const GlyphInfo& getGlyphInfo(char32_t cp)
		{
			auto it = glyph_info_.find(cp);
			if(it != glyph_info_.end()) {
				return it->second;
			}
			static GlyphInfo res;
			memset(&res, 0, sizeof(GlyphInfo));
			return res;
		}
	private:
		std::string fnt_;
		std::string fnt_path_;
		float size_;
		Color color_;
		FT_Face face_;
		bool has_kerning_;
		float x_height_;
		TexturePtr font_texture_;
		int next_font_x_;
		int next_font_y_;
		unsigned short last_line_height_;
		// XXX see what is practically faster using a sorted list and binary search
		// or this map. Also a vector would have better locality.
		std::map<char32_t, GlyphInfo> glyph_info_;
		bool all_glyphs_added_;
		int font_load_flags_;
		std::map<std::string, std::vector<point>> glyph_path_cache_;
		friend class FontHandle;
	};
	
	FontHandle::FontHandle(const std::string& fnt_name, const std::string& fnt_path, float size, const Color& color, bool init_texture)
		: impl_(new Impl(fnt_name, fnt_path, size, color, init_texture))
	{
	}

	FontHandle::~FontHandle()
	{
	}

	float FontHandle::getFontSize()
	{
		return impl_->size_;
	}

	float FontHandle::getFontXHeight()
	{
		// XXX
		return impl_->x_height_;
	}

	const std::string& FontHandle::getFontName()
	{
		return impl_->fnt_;
	}

	const std::string& FontHandle::getFontPath()
	{
		return impl_->fnt_path_;
	}

	const std::string& FontHandle::getFontFamily()
	{
		// XXX
		return impl_->fnt_;
	}

	void FontHandle::renderText()
	{
	}

	void FontHandle::getFontMetrics()
	{
	}

	int FontHandle::getDescender() 
	{
		return impl_->getDescender();
	}

	const std::vector<point>& FontHandle::getGlyphPath(const std::string& text)
	{
		return impl_->getGlyphPath(text);
	}

	rect FontHandle::getBoundingBox(const std::string& text)
	{
		return rect();
	}

	FontRenderablePtr FontHandle::createRenderableFromPath(FontRenderablePtr r, const std::string& text, const std::vector<point>& path)
	{
		return impl_->createRenderableFromPath(r, text, path);
	}

	int FontHandle::calculateCharAdvance(char32_t cp)
	{
		return impl_->calculateCharAdvance(cp);
	}
	
	const GlyphInfo& FontHandle::getGlyphInfo(char32_t cp)
	{
		return impl_->getGlyphInfo(cp);
	}

	std::vector<unsigned> FontHandle::getGlyphs(const std::string& text)
	{
		return impl_->getGlyphs(text);
	}

	void* FontHandle::getRawFontHandle()
	{
		return impl_->face_;
	}
}
