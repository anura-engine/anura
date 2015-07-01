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
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <cairo.h>

#include "asserts.hpp"
#include "easy_svg.hpp"
#include "svg_parse.hpp"

namespace KRE
{
	namespace 
	{
		class CairoContext
		{
		public:
			CairoContext(int width, int height) 
				: surface_(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height)),
				  cairo_(cairo_create(surface_)),
				  width_(width),
				  height_(height)
			{
			}
			~CairoContext()
			{
				cairo_destroy(cairo_);
				cairo_surface_destroy(surface_);
			}
			KRE::TexturePtr render(const std::string& filename)
			{
				cairo_status_t status = cairo_status(cairo_);
				ASSERT_LOG(status == 0, "SVG rendering error rendering " << filename << ": " << cairo_status_to_string(status));

				KRE::SVG::parse handle(filename);
				KRE::SVG::render_context ctx(cairo_, width_, height_);
				handle.render(ctx);

				status = cairo_status(cairo_);
				ASSERT_LOG(status == 0, "SVG rendering error rendering " << filename << ": " << cairo_status_to_string(status));

				auto surf = KRE::Surface::create(width_, height_, KRE::PixelFormat::PF::PIXELFORMAT_ARGB8888);
				surf->writePixels(cairo_image_surface_get_data(surface_), cairo_image_surface_get_stride(surface_) * height_);
				surf->createAlphaMap();
				return KRE::Texture::createTexture(surf);
			}
		private:
			cairo_surface_t* surface_;
			cairo_t* cairo_;
			int width_, height_;
		};
	}

	TexturePtr svg_texture_from_file(const std::string& file, int width, int height)
	{
		CairoContext ctx(width, height);
		auto ff = Surface::getFileFilter(FileFilterType::LOAD);
		return ctx.render(ff(file));
	}
}
