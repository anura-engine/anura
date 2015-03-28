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
#include "json_parser.hpp"
#include "module.hpp"
#include "surface_palette.hpp"

namespace graphics
{
	namespace 
	{
		typedef boost::bimap<std::string,int> palette_map_type;
		typedef palette_map_type::value_type palette_pair;

		void read_all_palettes(palette_map_type& pmap)
		{
			try {
				variant v = json::parse_from_file(module::map_file("data/palettes.cfg"));
				int id = 0;
				if(v.is_map()) {
					auto m = v.as_map();
					for(auto& p : m) {
						if(p.second.is_list()) {
							auto palette_names = p.second.as_list_string();
							for(auto& ps : palette_names) {
								LOG_DEBUG("Added palette: " << ps << " at " << id);
								pmap.insert(palette_pair(ps, id++));
							}
						}
					}
				}
			} catch(json::ParseError&) {
			}
		}

		palette_map_type& get_palette_map()
		{
			static palette_map_type res;
			if(res.empty()) {
				read_all_palettes(res);
			}
			return res;
		}

		typedef std::map<std::string, std::weak_ptr<KRE::Texture>> palette_texture_cache;
		palette_texture_cache& get_palette_texture_cache()
		{
			static palette_texture_cache res;
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
		int id = static_cast<int>(get_palette_map().size());
		get_palette_map().insert(palette_pair(name, id));
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
		new_surf->writePixels(&new_pixels[0], static_cast<int>(new_pixels.size()));
		return new_surf;
	}

	KRE::TexturePtr get_palette_texture(const std::string& name, const variant& node, int palette)
	{
		std::vector<int> p;
		p.emplace_back(palette);
		return get_palette_texture(name, node, p);
	}

	KRE::TexturePtr get_palette_texture(const std::string& name, const variant& node, const std::vector<int>& palette)
	{
		ASSERT_LOG(!name.empty(), "palettes are set but image is empty.");
		auto it = get_palette_texture_cache().find(name);
		KRE::TexturePtr tex = nullptr;
		if(it != get_palette_texture_cache().end()) {
			tex = it->second.lock();
		}
		if(tex == nullptr) {
			tex = KRE::Texture::createTexture(node);
			get_palette_texture_cache()[name] = tex;
		}
		ASSERT_LOG(tex != nullptr, "No texture was created.");

		std::stringstream ss;
		for(auto& palette_id : palette) {
			if(!tex->hasPaletteAt(palette_id)) {
				//LOG_DEBUG("no palette at " << palette_id << " texture id: " << tex->id() << " : " << tex.get());
				tex->addPalette(palette_id, graphics::get_palette_surface(palette_id));
				ss << " " << palette_id;
			}
		}
		std::string palette_str = ss.str();
		if(!palette_str.empty()) {
			LOG_DEBUG("Adding palettes: " << palette_str << " at: " << " to texture id: " << tex->id() << ", '" << name << "'");
		} else {
			LOG_DEBUG("Return texture for '" << name << "', id=" << tex->id() << " has_palette: " << (tex->isPaletteized() ? "yes" : "no"));
		}
		return tex;
	}
}
