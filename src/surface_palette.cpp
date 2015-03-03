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

#include <map>
#include <vector>

#include <boost/bimap.hpp>

#include "asserts.hpp"
#include "module.hpp"
#include "surface_cache.hpp"
#include "surface_palette.hpp"

namespace graphics
{
	namespace 
	{
		typedef boost::bimap<std::string,int> palette_map_type;
		typedef palette_map_type::value_type palette_pair;

		palette_map_type& get_palette_map()
		{
			static palette_map_type res;
			return res;
		}
	}

	KRE::SurfacePtr get_palette_surface(int palette)
	{
		auto& name = get_palette_name(palette);
		if(name.empty()) {
			return nullptr;
		}
		return KRE::Surface::create(module::map_file("palette/" + name + ".png"));
	}


	int get_palette_id(const std::string& name)
	{
		if(name.empty()) {
			return -1;
		}

		auto it = get_palette_map().left.find(name);
		if(it != get_palette_map().left.end()) {
			return it->second;
		}
		int id = get_palette_map().size();
		get_palette_map().insert(palette_pair(name, id));
		LOG_DEBUG("Added palette '" << name << "' at index: " << id);
		return id;
	}

	const std::string& get_palette_name(int id)
	{
		auto it = get_palette_map().right.find(id);
		if(it == get_palette_map().right.end()) {
			static const std::string str;
			return str;
		}
		return it->second;
	}

	KRE::SurfacePtr map_palette(KRE::SurfacePtr surface, int palette)
	{
		using namespace KRE;

		auto psurf = get_palette_surface(palette);
		if(psurf == nullptr) {
			return surface;
		}

		// generate a map of the palette.
		std::map<uint32_t, uint32_t> color_map;
		if(psurf->width() > psurf->height()) {
			for(int x = 0; x != psurf->width(); ++x) {
				Color normal_color = psurf->getColorAt(x, 0);
				Color mapped_color = psurf->getColorAt(x, 1);
				color_map[normal_color.asRGBA()] = mapped_color.asRGBA();
			}
		} else {
			for(int y = 0; y != psurf->height(); ++y) {
				Color normal_color = psurf->getColorAt(0, y);
				Color mapped_color = psurf->getColorAt(1, y);
				color_map[normal_color.asRGBA()] = mapped_color.asRGBA();
			}
		}

		int rp = surface->rowPitch();
		int bpp = surface->bytesPerPixel();
		std::vector<uint8_t> new_pixels;
		new_pixels.resize(rp * surface->height());

		surface->iterateOverSurface([&color_map, &new_pixels, rp, bpp](int x, int y, int r, int g, int b, int a) {
			uint32_t color = (static_cast<uint32_t>(r) << 24)
				| (static_cast<uint32_t>(g) << 16)
				| (static_cast<uint32_t>(b) << 8)
				| (static_cast<uint32_t>(a));
			const int index = x * bpp + y * rp;
			
			auto it = color_map.find(color);
			if(it == color_map.end()) {
				new_pixels[index + 0] = r;
				new_pixels[index + 1] = g;
				new_pixels[index + 2] = b;
				new_pixels[index + 3] = a;
			} else {
				new_pixels[index + 0] = (it->second >> 24) & 0xff;
				new_pixels[index + 1] = (it->second >> 16) & 0xff;
				new_pixels[index + 2] = (it->second >>  8) & 0xff;
				new_pixels[index + 3] = (it->second >>  0) & 0xff;
			}
		});
		auto new_surf = Surface::create(surface->width(), surface->height(), PixelFormat::PF::PIXELFORMAT_RGBA8888);
		new_surf->writePixels(&new_pixels[0], new_pixels.size());
		return new_surf;
	}
}
