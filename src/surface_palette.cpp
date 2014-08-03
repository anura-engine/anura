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

#include "asserts.hpp"
#include "surface_cache.hpp"
#include "surface_palette.hpp"

namespace graphics
{
	namespace 
	{
		struct palette_definition 
		{
			std::string name;
			std::map<uint32_t, uint32_t> mapping;
		};

		std::vector<palette_definition> palettes;

		void load_palette_def(const std::string& id)
		{
			palette_definition def;
			def.name = id;
			KRE::SurfacePtr s = SurfaceCache::get("palette/" + id + ".png", false);

			auto converted = s->convert(KRE::PixelFormat::PF::PIXELFORMAT_ARGB8888);
			s = converted;

			ASSERT_LOG(s != nullptr, "COULD NOT LOAD PALETTE IMAGE " << id);
			ASSERT_LOG(s->getPixelFormat()->bytesPerPixel() == 4, "PALETTE " << id << " NOT IN 32bpp PIXEL FORMAT");

			const uint32_t* pixels = reinterpret_cast<const uint32_t*>(s->pixelsWriteable());
			for(unsigned n = 0; n < s->width() * s->height() - 1; n += 2) {
				def.mapping.insert(std::pair<uint32_t,uint32_t>(pixels[0], pixels[1]));
				pixels += 2;
			}

			palettes.push_back(def);
		}
	}

	int get_palette_id(const std::string& name)
	{
		if(name.empty()) {
			return -1;
		}

		static std::map<std::string, int> m;
		std::map<std::string, int>::const_iterator i = m.find(name);
		if(i != m.end()) {
			return i->second;
		}

		const int id = m.size();
		m[name] = id;
		load_palette_def(name);
		return get_palette_id(name);
	}

	const std::string& get_palette_name(int id)
	{
		if(id < 0 || static_cast<unsigned>(id) >= palettes.size()) {
			static const std::string str;
			return str;
		} else {
			return palettes[id].name;
		}
	}

	KRE::SurfacePtr map_palette(KRE::SurfacePtr s, int palette)
	{
		if(palette < 0 || static_cast<unsigned>(palette) >= palettes.size() || palettes[palette].mapping.empty()) {
			return s;
		}

		auto result = KRE::Surface::create(s->width(), s->height(), KRE::PixelFormat::PF::PIXELFORMAT_ARGB8888);
		s = s->convert(KRE::PixelFormat::PF::PIXELFORMAT_ARGB8888);
		ASSERT_LOG(s->getPixelFormat()->bytesPerPixel() == 4, "SURFACE NOT IN 32bpp PIXEL FORMAT");

		LOG_INFO("mapping palette " << palette);

		uint32_t* dst = reinterpret_cast<uint32_t*>(result->pixelsWriteable());
		const uint32_t* src = reinterpret_cast<const uint32_t*>(s->pixels());

		const std::map<uint32_t,uint32_t>& mapping = palettes[palette].mapping;

		for(int n = 0; n != s->width() * s->height(); ++n) {
			std::map<uint32_t,uint32_t>::const_iterator i = mapping.find(*src);
			if(i != mapping.end()) {
				*dst = i->second;
			} else {
				*dst = *src;
			}

			++src;
			++dst;
		}
		return result;
	}

	KRE::Color map_palette(const KRE::Color& c, int palette)
	{
		if(palette < 0 || static_cast<unsigned>(palette) >= palettes.size() || palettes[palette].mapping.empty()) {
			return c;
		}

		const std::map<uint32_t,uint32_t>& mapping = palettes[palette].mapping;
		std::map<uint32_t,uint32_t>::const_iterator i = mapping.find(c.asARGB());
		if(i != mapping.end()) {
			return KRE::Color(i->second, KRE::ColorByteOrder::RGBA);
		} else {
			return c;
		}
	}
}
