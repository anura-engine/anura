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

#include "Canvas.hpp"
#include "DisplayDevice.hpp"

#include "asserts.hpp"
#include "gui_section.hpp"
#include "string_utils.hpp"
#include "variant.hpp"

namespace 
{
	typedef std::map<std::string, ConstGuiSectionPtr> cache_map;
	cache_map cache;
}

std::vector<std::string> GuiSection::getSections()
{
	std::vector<std::string> v;
	for(auto it : cache) {
		v.push_back(it.first);
	}
	return v;
}

void GuiSection::init(variant node)
{
	for(const variant& section_node : node["section"].as_list()) {
		const std::string& id = section_node["id"].as_string();
		cache[id].reset(new GuiSection(section_node));
	}
}

ConstGuiSectionPtr GuiSection::get(const variant& v)
{
	if(v.has_key("name")) {
		return get(v["name"].as_string());
	} else {
		const std::string& id = v["id"].as_string();
		cache[id].reset(new GuiSection(v));
		return cache[id];
	}
}

ConstGuiSectionPtr GuiSection::get(const std::string& key)
{
	cache_map::const_iterator itor = cache.find(key);
	if(itor == cache.end()) {
		ASSERT_LOG(false, "GUI section " << key << " not found in cache");
		return ConstGuiSectionPtr();
	}

	return itor->second;
}

GuiSection::GuiSection(variant node)
	: texture_(KRE::Texture::createTexture(node["image"])),
    area_(node["rect"]),
	x_adjust_(0), y_adjust_(0), x2_adjust_(0), y2_adjust_(0)
{
	draw_area_ = area_;

	if(node.has_key("frame_info")) {
		std::vector<int> buf = node["frame_info"].as_list_int();
		if(buf.size() == 8) {
			x_adjust_ = buf[0];
			y_adjust_ = buf[1];
			x2_adjust_ = buf[2];
			y2_adjust_ = buf[3];
			draw_area_ = rect(buf[4], buf[5], buf[6], buf[7]);
		}
	}
}

void GuiSection::blit(int x, int y, int w, int h) const
{
	const int scale = w/area_.w();

	auto canvas = KRE::Canvas::getInstance();
	rect dest(x+x_adjust_*scale, y+y_adjust_*scale, w - x_adjust_*scale - x2_adjust_*scale, h - y_adjust_*scale - y2_adjust_*scale);
	canvas->blitTexture(texture_, draw_area_, 0.0f, dest);
}
