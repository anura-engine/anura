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

#include "Blittable.hpp"
#include "WindowManager.hpp"

#include "filesystem.hpp"
#include "graphical_font.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "utf8_to_codepoint.hpp"
#include "variant_utils.hpp"

namespace 
{
	typedef std::map<std::string, GraphicalFontPtr> cache_map;
	cache_map& get_cache()
	{
		static cache_map res;
		return res;
	}
}

PREF_BOOL(enable_graphical_fonts, true, "Loads graphical fonts");

void GraphicalFont::init(variant node)
{
	for(const variant& font_node : node["font"].as_list()) {
		GraphicalFontPtr font(new GraphicalFont(font_node));
		get_cache()[font->id()] = font;
	}
}

ConstGraphicalFontPtr GraphicalFont::get(const std::string& id)
{
	cache_map::const_iterator itor = get_cache().find(id);
	if(itor == get_cache().end()) {
		return ConstGraphicalFontPtr();
	}

	return itor->second;
}

GraphicalFont::GraphicalFont(variant node)
  : id_(node["id"].as_string()), 
    texture_file_(node["texture"].as_string()),
	texture_(KRE::Texture::createTexture(node["texture"].as_string())),
    kerning_(node["kerning"].as_int(2))
{
	int pad = 2;
	if (node.has_key("pad")){
		pad = node["pad"].as_int(2);
	}
	
	rect current_rect;
	for(const variant& char_node : node["chars"].as_list()) {
		if(char_node.has_key("pad")) {
			pad = char_node["pad"].as_int();
		}

		const std::string& chars = char_node["chars"].as_string();
		if(char_node.has_key("width")) {
			current_rect = rect(current_rect.x(), current_rect.y(),
			                    char_node["width"].as_int(),
			                    current_rect.h());
		} else {
			current_rect = rect(char_node["rect"].as_list_int());
		}
		for(auto codepoint : utils::utf8_to_codepoint(chars)) {
			if (codepoint == 0)
				break;

			char_rect_map_[codepoint] = current_rect;

			current_rect = rect(current_rect.x() + current_rect.w() + pad,
			                    current_rect.y(),
			                    current_rect.w(), current_rect.h());
		}
	}
}

const rect& GraphicalFont::get_codepoint_area(unsigned int codepoint) const
{
	auto itor = char_rect_map_.find(codepoint);
	if(itor == char_rect_map_.end()) {
		static rect result;
		return result;
	}

	return itor->second;
}

rect GraphicalFont::draw(int x, int y, const std::string& text, int size, const KRE::Color& color) const
{
	return doDraw(x, y, text, true, size, color);
}

rect GraphicalFont::doDraw(int x, int y, const std::string& text, bool draw_text, int size, const KRE::Color& color) const
{
	if(text.empty()) {
		return rect(x, y, 0, 0);
	}

	int x2 = x, y2 = y;
	int xpos = x, ypos = y, highest = 0;

	std::vector<KRE::vertex_texcoord> font_vtxarray;

	for(auto codepoint : utils::utf8_to_codepoint(text)) {
		// ASCII \n character and Unicode code point are the same
		// going to ignore the other Unicode line seperators, 
		// due to uncommon usage.
		if(codepoint == '\n') {
			ypos = ypos + ((highest+4)*size)/2;
			xpos = x;
			highest = 0;
			continue;
		} else if(codepoint == 0) {
			break;
		}

		char_rect_map::const_iterator it = char_rect_map_.find(codepoint);
		if (it == char_rect_map_.end()) {
			continue;
		}

		const rectf& r = it->second.as_type<float>();

		if(draw_text) {
			const rectf uv = rectf::from_coordinates(texture_->getTextureCoordW(0, it->second.x1()),
				texture_->getTextureCoordH(0, it->second.y1()),
				texture_->getTextureCoordW(0, it->second.x2()),
				texture_->getTextureCoordH(0, it->second.y2()));

			const float x = static_cast<float>(xpos & preferences::xypos_draw_mask);
			const float y = static_cast<float>(ypos & preferences::xypos_draw_mask);

			font_vtxarray.emplace_back(glm::vec2(x,y), glm::vec2(uv.x1(),uv.y1()));
			font_vtxarray.emplace_back(glm::vec2(x + r.w()*size,y), glm::vec2(uv.x2(),uv.y1()));
			font_vtxarray.emplace_back(glm::vec2(x + r.w()*size,y + r.h()*size), glm::vec2(uv.x2(),uv.y2()));
			font_vtxarray.emplace_back(glm::vec2(x + r.w()*size,y + r.h()*size), glm::vec2(uv.x2(),uv.y2()));
			font_vtxarray.emplace_back(glm::vec2(x,y), glm::vec2(uv.x1(),uv.y1()));
			font_vtxarray.emplace_back(glm::vec2(x,y + r.h()*size), glm::vec2(uv.x1(),uv.y2()));
		}

		if(ypos + r.h()*size > y2) {
			y2 = ypos + static_cast<int>(r.h()*size);
		}
		
		xpos += static_cast<int>(r.w()*size) + kerning_*size;

		if(xpos > x2) {
			x2 = xpos;
		}

		if(static_cast<int>(r.h()) > highest) {
			highest = static_cast<int>(r.h());
		}
	}

	if(draw_text && !font_vtxarray.empty()) {
		KRE::Blittable blit;
		blit.setTexture(texture_);
		blit.update(&font_vtxarray);
		blit.setColor(color);
		blit.setDrawMode(KRE::DrawMode::TRIANGLES);
		auto wnd = KRE::WindowManager::getMainWindow();
		wnd->render(&blit);
		//auto canvas = KRE::Canvas::getInstance();
		//canvas->blitTexture(texture_, font_vtxarray, 0, color);
	}
	return rect(x, y, x2 - x, y2 - y);
}

rect GraphicalFont::dimensions(const std::string& text, int size) const
{
	return doDraw(0, 0, text, false, size, KRE::Color::colorWhite());
}

// Initialize the graphical font for the given locale
void GraphicalFont::initForLocale(const std::string& locale) 
{
	if(!g_enable_graphical_fonts) {
		return;
	}

	std::string names[] = {"base_fonts", "fonts"};
	for(auto& name : names) {
		std::string filename = "data/" + name + "." + locale + ".cfg";
		if (!sys::file_exists(filename))
			filename = "data/" + name + ".cfg";
		LOG_INFO("LOADING FONT: " << filename << " -> " << module::map_file(filename));
		GraphicalFont::init(json::parse_from_file(filename));
	}
}

