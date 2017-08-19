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

#include <algorithm>
#include <future>
#include <thread>
#include <cairo.h>

#include "asserts.hpp"
#include "easy_svg.hpp"
#include "svg_parse.hpp"
#include "stb_rect_pack.h"

namespace KRE
{
	namespace 
	{
		const int max_surface_width = 4096;
		const int max_surface_height = 4096;

		class CairoContext
		{
		public:
			CairoContext(int width, int height) 
				: surface_(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height)),
				  cairo_(cairo_create(surface_)),
				  width_(width),
				  height_(height)
			{
				ASSERT_LOG(width_ > 0 && height_ > 0, "Supplied width and/or height values are bad. " << width_ << " x " << height_);
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
			KRE::SurfacePtr createSurface(const std::string& filename)
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
				return surf;
			}
		private:
			cairo_surface_t* surface_;
			cairo_t* cairo_;
			int width_, height_;

			CairoContext() = delete;
			CairoContext(const CairoContext&) = delete;
			CairoContext& operator=(const CairoContext&) = delete;
		};
	}

	TexturePtr svg_texture_from_file(const std::string& file, int width, int height)
	{
		CairoContext ctx(width, height);
		auto ff = Surface::getFileFilter(FileFilterType::LOAD);
		return ctx.render(ff(file));
	}

	TexturePtr svgs_to_single_texture(const std::vector<std::string>& files, const std::vector<point>& wh, std::vector<rectf>* tex_coords)
	{
		ASSERT_LOG(files.size() == wh.size(), "Number of files is different from the number of sizes provided.");

		const int max_threads = 8;
		std::vector<std::future<void>> futures;
		std::vector<SurfacePtr> images;
		images.resize(files.size());
		const int n_incr = images.size() / max_threads < 1 ? 1 : images.size() / max_threads;
		const auto ff = Surface::getFileFilter(FileFilterType::LOAD);
		for(int n = 0; n < static_cast<int>(images.size()); n += n_incr) {
			int n2 = n + n_incr > static_cast<int>(images.size()) ? files.size() : n + n_incr;
			futures.push_back(std::async([&images, n, n2, &files, &wh, &ff]() {
				for(int ndx = n; ndx != n2; ++ndx) {
					CairoContext ctx(wh[ndx].x, wh[ndx].y);
					images[ndx] = ctx.createSurface(ff(files[ndx]));
					ASSERT_LOG(images[ndx] != nullptr, "Image file couldn't be read: " << files[ndx]);
				}
			}));
		}
		for(auto& f : futures) {
			f.get();
		}

		std::vector<stbrp_node> nodes;
		nodes.resize(max_surface_width);

		const int increment = 64;
		int width = 64;
		int height = 64;

		int nn = 0;

		std::vector<stbrp_rect> rects;
		for(auto& img : images) {
			stbrp_rect r;
			r.id = rects.size();
			r.w = img->width();
			r.h = img->height();
			rects.emplace_back(r);
		}

		bool packed = false;
		while(!packed) {
			for(auto& r : rects) {
				r.x = r.y = r.was_packed = 0;
			}
			stbrp_context context;
			stbrp_init_target(&context, width, height, nodes.data(), nodes.size());
			stbrp_pack_rects(&context, rects.data(), rects.size());

			packed = true;
			for(auto& r : rects) {
				if(!r.was_packed) {
					if(nn & 1) {
						height += increment;
					} else {
						width += increment;
					}
					++nn;
					if(width > max_surface_width || height > max_surface_height) {
						return nullptr;
					}
					packed = false;
					break;
				}
			}

		}

		std::vector<rect> outr;
		outr.resize(images.size());

		auto out = Surface::create(width, height, PixelFormat::PF::PIXELFORMAT_RGBA8888);
		for(auto& r : rects) {
			outr[r.id] = rect(r.x, r.y, r.w, r.h);
			rect alpha_borders(0, 0, r.w, r.h);
			out->blitTo(images[r.id], alpha_borders, outr[r.id]);
		}
		out->savePng("scrollbar_elements.png");
		auto out_tex = Texture::createTexture(out);
		if(tex_coords != nullptr) {
			tex_coords->clear();
			tex_coords->reserve(outr.size());
			for(const auto& r : outr) {
				tex_coords->emplace_back(out_tex->getTextureCoords(0, r));
			}
		}
		return out_tex;
	}

	TexturePtr svgs_to_single_texture(const std::vector<std::string>& files, int width, int height, std::vector<rectf>* tex_coords)
	{
		std::vector<point> wh(files.size(), point(width, height));
		return svgs_to_single_texture(files, wh, tex_coords);
	}
}
