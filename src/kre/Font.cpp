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

#include <map>

#include <cairo.h>
#include <cairo-ft.h>

#include "Font.hpp"

namespace KRE
{
	/* XXX This was an experimental work in process to render text. But it really has some flaws,
		not the least of which is that I suspect cairo will take ~4-5ms to render the text.
		Not exactly high performance.

	namespace
	{
		std::map<std::string,std::pair<cairo_font_face_t,FT_Face>>& get_scaled_font_cache()
		{
			static std::map<std::string,std::pair<cairo_font_face_t,FT_Face>> res;
			return res;
		}
	}

	TexturePtr Font::renderText(const std::string& text, const Color& color, int size, bool cache, const std::string& font_name)
	{
		// Create a surface to render text to.
		// we don't know how big the surface needs to be, so the
		// 512x512 is kind of a hack.
		cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 512, 512);
		cairo_t* cairo = cairo_create(surface);

		FT_Face face;
		cairo_font_face_t* font_face;
		auto it = get_scaled_font_cache().find(font_name);
		if(it != get_scaled_font_cache().end()) {
			face = FT::get_font_face(font_name);
			font_face = cairo_ft_font_face_create_for_ft_face(face, 0);
			get_scaled_font_cache()[font_name] = std::make_pair(font_face, face);
		} else {
			face = it->second.second;
			font_face = it->second.first;
		}

		cairo_font_options_t *font_options = cairo_font_options_create();
		cairo_matrix_t ctm, font_matrix;
		cairo_get_matrix(cairo, &ctm);
		cairo_get_font_matrix(cairo, &font_matrix);
		font_matrix.xx = font_matrix.yy = double(size);
		cairo_scaled_font_t *scaled_font = cairo_scaled_font_create(font_face, &font_matrix, &ctm, font_options);
		cairo_set_scaled_font(cairo, scaled_font);

		std::vector<cairo_glyph_t> glyphs;
		auto glyph_indicies = FT::get_glyphs_from_string(face, text);
		double x(0), y(0);
		for(auto g : glyph_indicies) {
			cairo_glyph_t cg;
			cg.index = g;
			cg.x = x;
			cg.y = y;
			glyphs.push_back(cg);

			cairo_text_extents_t extent;
			cairo_glyph_extents(cairo, &cg, 1, &extent);
			x += extent.x_advance;
			y += extent.y_advance;
		}

		cairo_glyph_path(cairo, &glyphs[0], glyphs.size());
		cairo_set_source_rgba(cairo, color.r(), color.g(), color.b(), color.a());
		cairo_fill(cairo);
		double x1, y1, x2, y2;
		cairo_path_extents(cairo, &x1, &y1, &x2, &y2);

		cairo_scaled_font_destroy(scaled_font);
		cairo_font_options_destroy(font_options);

		cairo_destroy(cairo);
		cairo_surface_destroy(surface);
	}*/

	namespace
	{
		typedef std::map<std::string, std::function<FontPtr()>> FontRegistry;
		FontRegistry& get_font_device_registry()
		{
			static FontRegistry res;
			return res;
		}

		struct CacheKey 
		{
			std::string text;
			Color color;
			int font_size;
			std::string font_name;

			bool operator<(const CacheKey& k) const {
				return text < k.text || (text == k.text && color < k.color)
					|| (text == k.text && color == k.color && font_size < k.font_size)
					|| (text == k.text && color == k.color && font_size == k.font_size && font_name < k.font_name);
		}	
		};

		typedef std::map<CacheKey, TexturePtr> RenderCache;
		RenderCache& get_render_cache()
		{
			static RenderCache res;
			return res;
		}

		std::string& get_default_font()
		{
			static std::string res;
			return res;
		}

		font_path_cache& get_font_path_cache()
		{
			static font_path_cache res;
			return res;
		}
	}

	void Font::registerFactoryFunction(const std::string& type, std::function<FontPtr()> create_fn)
	{
		auto it = get_font_device_registry().find(type);
		if(it != get_font_device_registry().end()) {
			LOG_WARN("Overwriting the Font Driver: " << type);
		}
		get_font_device_registry()[type] = create_fn;
	}

	FontPtr Font::getInstance(const std::string& hint)
	{
		ASSERT_LOG(!get_font_device_registry().empty(), "No display device drivers registered.");
		if(!hint.empty()) {
			auto it = get_font_device_registry().find(hint);
			if(it == get_font_device_registry().end()) {			
				LOG_WARN("Requested display driver '" << hint << "' not found, using default: " << get_font_device_registry().begin()->first);
				return get_font_device_registry().begin()->second();
			}
			return it->second();
		}
		return get_font_device_registry().begin()->second();
	}

	TexturePtr Font::renderText(const std::string& text, const Color& color, int size, bool cache, const std::string& font_name) const
	{
		if(!cache) {
			return doRenderText(text, color, size, font_name);
		}
		CacheKey key = {text, color, size, font_name};
		auto it = get_render_cache().find(key);
		if(it == get_render_cache().end()) {
			TexturePtr t = doRenderText(text, color, size, font_name);
			get_render_cache()[key] = t;
			return t;
		}
		return it->second;
	}

	void Font::getTextSize(const std::string& text, int* width, int* height, int size, const std::string& font_name) const
	{
		calcTextSize(text, size, font_name, width, height);
	}

	void Font::setAvailableFonts(const std::map<std::string, std::string>& font_map)
	{
		get_font_path_cache() = font_map;
	}

	std::vector<std::string> Font::getAvailableFonts()
	{
		std::vector<std::string> res;
		for(auto& fp : get_font_path_cache()) {
			res.emplace_back(fp.first);
		}
		return res;
	}

	std::string Font::findFontPath(const std::string& fontname)
	{
		auto it = get_font_path_cache().find(fontname);
		if(it == get_font_path_cache().end()) {
			//LOG_DEBUG("Font name: " << fontname << " not found.");
			std::stringstream ss;
			ss << "Font '" << fontname << "' not found in any available path.\n";
			ss << "Paths were: ";
			if(get_font_path_cache().size() > 0) {
				for(auto& fp : get_font_path_cache()) {
					ss << fp.first << " -> " << fp.second << "\n";
				}
			} else {
				ss << "<empty>";
			}
			//LOG_DEBUG(ss.str());
			throw FontError(ss.str().c_str());
		}
		//LOG_DEBUG("Font name " << it->first << " at " << it->second);
		return it->second;
	}

	int Font::charWidth(int size, const std::string& fn)
	{
		auto font_instance = getInstance();
		if(font_instance) {
			return font_instance->getCharWidth(size, fn);
		}
		ASSERT_LOG(false, "No font instance found.");
		return size;
	}

	int Font::charHeight(int size, const std::string& fn)
	{
		auto font_instance = getInstance();
		if(font_instance) {
			return font_instance->getCharHeight(size, fn);
		}
		ASSERT_LOG(false, "No font instance found.");
		return size;
	}

	void Font::setDefaultFont(const std::string& font_name)
	{
		get_default_font() = font_name;
	}

	const std::string& Font::getDefaultFont()
	{
		return get_default_font();
	}

	Font::Font()
	{
	}

	Font::~Font()
	{
	}
}
