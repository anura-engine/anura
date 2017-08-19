/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#include <string>
#include <cairo.h>

#include "Texture.hpp"

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

namespace graphics
{
	// Basic glue class between cairo and Anura's texture/surface formats.
	// Use a cairo_context when you want to use vector graphics to render
	// a texture. Create the cairo_context, use get() to get out the cairo_t*
	// and then perform whatever cairo draw calls you want. Then call write()
	// to extract a graphics::texture for use in Anura.
	class cairo_context
	{
	public:
		cairo_context(int w, int h);
		~cairo_context();

		cairo_t* get() const;
		KRE::TexturePtr write(const variant& node) const;
	
		void render_svg(const std::string& fname, int w, int h);
		void write_png(const std::string& fname);

		void set_pattern(cairo_pattern_t* pattern, bool take_ownership=true);

		cairo_pattern_t* get_pattern_ownership(bool* get_ownership);

		float width() const { return static_cast<float>(width_); }
		float height() const { return static_cast<float>(height_); }
	private:
		cairo_context(const cairo_context&);
		void operator=(const cairo_context&);

		cairo_surface_t* surface_;
		cairo_t* cairo_;
		int width_, height_;

		cairo_pattern_t* temp_pattern_;
	};

	struct cairo_matrix_saver
	{
		cairo_matrix_saver(cairo_context& ctx);
		~cairo_matrix_saver();

		cairo_context& ctx_;
	};

	class cairo_callable : public game_logic::FormulaCallable
	{
	public:
		cairo_callable();
	private:
		DECLARE_CALLABLE(cairo_callable);
	};

	namespace cairo_font
	{
		KRE::TexturePtr render_text_uncached(const std::string& text, const KRE::Color& color, int size, const std::string& font_name="");

		int char_width(int size, const std::string& fn="");
		int char_height(int size, const std::string& fn="");
	}

	struct CairoCacheStatus {
		int num_items;
		int memory_usage;
	};

	CairoCacheStatus get_cairo_image_cache_status();
}
