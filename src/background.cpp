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

#include <cmath>

#include <iostream>
#include <map>

#include "Scissor.hpp"
#include "WindowManager.hpp"

#include "background.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "screen_handling.hpp"
#include "surface_palette.hpp"
#include "variant.hpp"
#include "variant_utils.hpp"

namespace 
{
	//a cache key with background name and palette ID.
	typedef std::pair<std::string, int> cache_key;
	typedef std::map<cache_key, std::shared_ptr<Background>> bg_cache;
	bg_cache cache;

#ifndef NO_EDITOR
	std::set<std::string> listening_for_files, files_updated;

	void on_bg_file_updated(std::string path)
	{
		files_updated.insert(path);
	}
#endif // NO_EDITOR
}

#ifndef NO_EDITOR
void Background::loadModifiedBackgrounds()
{
	static int prev_nitems = 0;
	const int nitems = static_cast<int>(cache.size());
	if(prev_nitems == nitems && files_updated.empty()) {
		return;
	}

	prev_nitems = nitems;

	std::set<std::string> error_paths;

	for(bg_cache::iterator j = cache.begin(); j != cache.end(); ++j) {
		if(j->second->file_.empty()) {
			continue;
		}

		if(listening_for_files.count(j->second->file_) == 0) {
			sys::notify_on_file_modification(j->second->file_, std::bind(on_bg_file_updated, j->second->file_));
			listening_for_files.insert(j->second->file_);
		}

		if(files_updated.count(j->second->file_)) {

			Background backup = *j->second;
			try {
				const std::string path = "data/backgrounds/" + j->second->id_ + ".cfg";
				*j->second = Background(json::parse_from_file(path), j->first.second);
			} catch(...) {
				LOG_ERROR("ERROR REFRESHING BACKGROUND");
				error_paths.insert(j->second->file_);
			}
			j->second->id_ = backup.id_;
			j->second->file_ = backup.file_;
		}
	}
}
#endif // NO_EDITOR

std::shared_ptr<Background> Background::get(const std::string& name, int palette_id)
{
	const cache_key id(name, palette_id);

	std::shared_ptr<Background>& obj = cache[id];
	if(obj) {
		obj->refreshPalette();
	} else {
		try {
			const std::string fname = "data/backgrounds/" + name + ".cfg";
			obj.reset(new Background(json::parse_from_file(fname), palette_id));
			obj->id_ = name;
			obj->file_ = module::map_file(fname);
		} catch(json::ParseError& e) {
			LOG_ERROR("Error parsing file: " << e.errorMessage());
		}
	}

	return obj;
}

std::vector<std::string> Background::getAvailableBackgrounds()
{
	std::vector<std::string> files;
	module::get_files_in_dir("data/backgrounds/", &files);

	std::vector<std::string> result;
	for(const std::string& fname : files) {
		if(fname.size() > 4 && std::equal(fname.end() - 4, fname.end(), ".cfg")) {
			result.emplace_back(std::string(fname.begin(), fname.end() - 4));
		}
	}

	return result;
}

Background::Background(variant node, int palette) 
	: palette_(palette),
	  top_rect_(),
	  bot_rect_()
{
	top_ = KRE::Color(node["top"]);
	bot_ = KRE::Color(node["bottom"]);

	width_ = node["width"].as_int();
	height_ = node["height"].as_int();

	bool colors_mapped = false;

	for(variant layer_node : node["layer"].as_list()) {
		std::shared_ptr<Layer> bg = std::make_shared<Layer>();
		bg->image = layer_node["image"].as_string();
		bg->image_formula = layer_node["image_formula"].as_string_default();
		ASSERT_LOG(bg->image_formula.empty(), "Image formula's aren't supported.");
		bg->xscale = layer_node["xscale"].as_int(100);
		bg->yscale_top = bg->yscale_bot = layer_node["yscale"].as_int(100);
		bg->yscale_top = layer_node["yscale_top"].as_int(bg->yscale_top);
		bg->yscale_bot = layer_node["yscale_bot"].as_int(bg->yscale_bot);
		bg->xspeed = layer_node["xspeed"].as_int(0);
		bg->xpad = layer_node["xpad"].as_int(0);
		bg->xoffset = layer_node["xoffset"].as_int(0);
		bg->yoffset = layer_node["yoffset"].as_int(0);
		bg->scale = layer_node["scale"].as_int(2);
		bg->blend = layer_node["blend"].as_bool(true);
		bg->notile = layer_node["notile"].as_bool(false);
		if(bg->scale < 1) {
			bg->scale = 1;
		}

		bg->setBlendState(bg->blend);

		bg->texture = graphics::get_palette_texture(bg->image, layer_node["image"], palette_);
		bg->setTexture(bg->texture);

		if(palette_ != -1) {
			bg->texture->setPalette(palette_);
		}

		if(palette_ != -1 && !colors_mapped) {
			top_ = bg->texture->mapPaletteColor(top_, palette);
			bot_ = bg->texture->mapPaletteColor(bot_, palette);
			colors_mapped = true;
		}

		using namespace KRE;
		auto ab = DisplayDevice::createAttributeSet(false, false, false);
		bg->attr_ = std::make_shared<Attribute<short_vertex_texcoord>>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW);
		bg->attr_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::SHORT, false, sizeof(short_vertex_texcoord), offsetof(short_vertex_texcoord, vertex)));
		bg->attr_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(short_vertex_texcoord), offsetof(short_vertex_texcoord, tc)));
		ab->addAttribute(bg->attr_);
		ab->setDrawMode(DrawMode::TRIANGLES);
		bg->addAttributeSet(ab);

		if(layer_node.has_key("mode")) {
			if(layer_node["mode"].is_string()) {
				std::string blend_mode = layer_node["mode"].as_string_default();
				if(blend_mode == "GL_MAX" || blend_mode == "MAX") {
					bg->setBlendEquation(BlendEquation(BlendEquationConstants::BE_MAX));
				} else if(blend_mode == "GL_MIN" || blend_mode == "MIN") { 
					bg->setBlendEquation(BlendEquation(BlendEquationConstants::BE_MIN));
				} else {
					bg->setBlendEquation(BlendEquation(BlendEquationConstants::BE_ADD));
				}
			} else {
				bg->setBlendEquation(BlendEquation(layer_node["mode"]));
			}
		}
		
		bg->color = Color(layer_node, DecodingHint::DECIMAL);
		bg->setColor(bg->color);

		if(layer_node.has_key("color_above")) {
			bg->color_above.reset(new Color(layer_node["color_above"]));
			if(palette_ != -1) {
				*bg->color_above = bg->texture->mapPaletteColor(*bg->color_above, palette);
			}
		}

		if(layer_node.has_key("color_below")) {
			bg->color_below.reset(new Color(layer_node["color_below"]));
			if(palette_ != -1) {
				*bg->color_below = bg->texture->mapPaletteColor(*bg->color_below, palette);
			}
		}

		bg->y1 = layer_node["y1"].as_int();
		bg->y2 = layer_node["y2"].as_int();

		bg->foreground = layer_node["foreground"].as_bool(false);
		bg->tile_upwards = layer_node["tile_upwards"].as_bool(false);
		bg->tile_downwards = layer_node["tile_downwards"].as_bool(false);
		layers_.emplace_back(bg);
	}
}

void Background::refreshPalette()
{
	for(auto bg : layers_) {
		if(bg->texture) {
			if(palette_ != -1) {
				bg->texture->setPalette(palette_);
			}
		}
	}
}

variant Background::write() const
{
	variant_builder res;
	res.add("top", top_.write());
	res.add("bottom", bot_.write());
	res.add("width", formatter() << width_);
	res.add("height", formatter() << height_);

	for(auto& bgp : layers_) {
		Background::Layer& bg = *bgp;
		variant_builder layer_node;
		layer_node.add("image", bg.image);
		layer_node.add("xscale", formatter() << bg.xscale);
		if(bg.yscale_top == bg.yscale_bot) {
			layer_node.add("yscale", formatter() << bg.yscale_top);
		} else {
			layer_node.add("yscale_top", formatter() << bg.yscale_top);
			layer_node.add("yscale_bot", formatter() << bg.yscale_bot);
		}
		layer_node.add("xspeed", formatter() << bg.xspeed);
		layer_node.add("xpad", formatter() << bg.xpad);
		layer_node.add("xoffset", formatter() << bg.xoffset);
		layer_node.add("yoffset", formatter() << bg.yoffset);
		layer_node.add("y1", formatter() << bg.y1);
		layer_node.add("y2", formatter() << bg.y2);
		layer_node.add("scale", formatter() << bg.scale);
		layer_node.add("red", formatter() << bg.color.r_int());
		layer_node.add("green", formatter() << bg.color.g_int());
		layer_node.add("blue", formatter() << bg.color.b_int());
		layer_node.add("alpha", formatter() << bg.color.a_int());
		if(bg.blend == false) {
			layer_node.add("blend", false);
		}

		if(bg.color_above) {
			layer_node.add("color_above", bg.color_above->write());
		}

		if(bg.color_below) {
			layer_node.add("color_below", bg.color_below->write());
		}
		if(bg.foreground) {
			layer_node.add("foreground", "true");
		}

		if(bg.tile_upwards) {
			layer_node.add("tile_upwards", "true");
		}

		if(bg.tile_downwards) {
			layer_node.add("tile_downwards", "true");
		}

		res.add("layer", layer_node.build());
	}
	return res.build();
}

void Background::draw(int x, int y, const rect& area, const std::vector<rect>& opaque_areas, float rotation, float xdelta, float ydelta, int cycle) const
{
	auto& gs = graphics::GameScreen::get();
	const int height = height_ + offset_.y * 2;
	//LOG_DEBUG("xy: " << x << "," << y << " wh: " << gs.getWidth() << "," << gs.getHeight());
	//LOG_DEBUG("height_: " << height_ << ", offset_.y: " << offset_.y << ", height: " << height);
	//LOG_DEBUG("area: " << area);

	//set the background colors for the level. The area above 'height' is
	//painted with the top color, and the area below height is painted with
	//the bottom color. For efficiency we do this using color clearing, with
	//scissors to divide the screen into top and bottom.
	if(height < y) {
		//the entire screen is full of the bottom color
		//LOG_DEBUG("fill all bot: " << bot_);
		bot_rect_.update(area, bot_);
		bot_rect_.enable();
		top_rect_.disable();
	} else if(height > area.y2()) {
		//the entire screen is full of the top color.
		//LOG_DEBUG("fill all top: " << top_);
		top_rect_.update(area, top_);
		top_rect_.enable();
		bot_rect_.disable();
	} else {
		//both bottom and top colors are on the screen, so draw them both,
		const int dist_from_bottom = height - y;

		//LOG_DEBUG("fill top: " << x << "," << y << "," << area.w() << "," << dist_from_bottom << " with: " << top_);
		top_rect_.update(x, y, area.w(), dist_from_bottom, top_);
		top_rect_.enable();

		//LOG_DEBUG("fill bot: " << x << "," << dist_from_bottom << "," << area.w() << "," << (area.h() - dist_from_bottom) << " with: " << bot_);
		bot_rect_.update(x, dist_from_bottom, area.w(), area.h() - dist_from_bottom, bot_);
		bot_rect_.enable();
	}
	auto wnd = KRE::WindowManager::getMainWindow();
	wnd->render(&bot_rect_);
	wnd->render(&top_rect_);

	drawLayers(x, y, area, opaque_areas, rotation, xdelta, ydelta, cycle);
}

namespace 
{
	void calculate_draw_areas(rect area, std::vector<rect>::const_iterator opaque1, std::vector<rect>::const_iterator opaque2, std::vector<rect>* areas) {
		if(opaque1 == opaque2) {
			areas->emplace_back(area);
			return;
		}

		rect sub_areas[4];
		for(; opaque1 != opaque2; ++opaque1) {
			const auto result = geometry::rect_difference(area, *opaque1, sub_areas);
			if(result == -1) {
				continue;
			}

			if(result != 1) {
				for(int n = 0; n < result; ++n) {
					calculate_draw_areas(sub_areas[n], opaque1+1, opaque2, areas);
				}
				return;
			}
			area = sub_areas[0];
		}
		areas->emplace_back(area);
	}
}

void Background::drawLayers(int x, int y, const rect& area_ref, const std::vector<rect>& opaque_areas, float rotation, float xdelta, float ydelta, int cycle) const
{
	auto wnd = KRE::WindowManager::getMainWindow();
	static std::vector<rect> areas;
	areas.clear();
	calculate_draw_areas(area_ref, opaque_areas.begin(), opaque_areas.end(), &areas);

	for(auto& bg : layers_) {
		if(bg->foreground == false) {

			for(auto& a : areas) {
				drawLayer(x, y, a, rotation, xdelta, ydelta, *bg, cycle);
			}
			wnd->render(bg.get());
			bg->attr_->clear();
			bg->getAttributeSet().back()->setCount(0);
		}
	}
}

void Background::drawForeground(int xpos, int ypos, float rotation, int cycle) const
{
	auto wnd = KRE::WindowManager::getMainWindow();
	for(auto& bg : layers_) {
		if(bg->foreground) {
			drawLayer(xpos, ypos, rect(xpos, ypos, graphics::GameScreen::get().getVirtualWidth(), graphics::GameScreen::get().getVirtualHeight()), rotation, 0.0f, 0.0f, *bg, cycle);
			if(bg->attr_->size() > 0) {
				wnd->render(bg.get());
			}
			bg->attr_->clear();
		}
	}
}

void Background::setOffset(const point& offset)
{
	offset_ = offset;
}

void Background::drawLayer(int x, int y, const rect& area, float rotation, float xdelta, float ydelta, const Background::Layer& bg, int cycle) const
{
	auto& gs = graphics::GameScreen::get();
	const float ScaleImage = 2.0f;
	int y1 = static_cast<int>(y + (bg.yoffset+offset_.y)*ScaleImage - (y*bg.yscale_top)/100);
	int y2 = static_cast<int>(y + (bg.yoffset+offset_.y)*ScaleImage - (y*bg.yscale_bot)/100 + (bg.y2 - bg.y1) * ScaleImage);
	/*
	if(!bg.tile_downwards && y2 <= y) {
		return;
	}

	if(!bg.tile_downwards && y2 <= area.y()) {
		return;
	}

	if(!bg.tile_upwards && y1 > area.y2()) {
		return;
	}

	if(!bg.texture) {
		return;
	}
	*/
	ASSERT_GT(bg.texture->surfaceHeight(), 0);
	ASSERT_GT(bg.texture->surfaceWidth(), 0);

	if(bg.y2 == 0) {
		bg.y2 = bg.texture->surfaceHeight();
	}

	float v1 = bg.texture->getTextureCoordH(0, bg.y1);
	float v2 = bg.texture->getTextureCoordH(0, bg.y2);

	const int screen_h = graphics::GameScreen::get().getHeight();

	if(y1 < area.y()) {
		//Making y1 == y2 is problematic, so don't allow it.
		const int target_y = area.y() == y2 ? area.y()-1 : area.y();
		v1 += (static_cast<float>(target_y - y1)/static_cast<float>(y2 - y1))*(v2 - v1);
		y1 = target_y;
	}

	auto wnd = KRE::WindowManager::getMainWindow();
	if(y1 > area.y()) {
		if(bg.tile_upwards) {
			v1 -= (static_cast<float>(y1 - area.y())/static_cast<float>(y2 - y1))*(v2 - v1);
			y1 = area.y();
		} else if(bg.color_above != nullptr) {
			const int height = y1 - area.y();

			bg.above_rect.update(area.x(), y1, area.w(), height, *bg.color_above);
			bg.above_rect.enable();
			wnd->render(&bg.above_rect);
		}
	}

	if(y2 < area.y2()) {
		if(bg.tile_downwards) {
			v2 += (static_cast<float>(area.y2() - y2)/static_cast<float>(y2 - y1))*(v2 - v1);
			y2 = area.y() + area.h();
		} else if(bg.color_below != nullptr) {
			const int xpos = area.x() - x;
			const int ypos = area.y2() - y;
			const int width = area.w();
			const int height = area.y2() - y2;

			bg.below_rect.update(xpos, screen_h - ypos, width, height, *bg.color_below);
			bg.below_rect.enable();
			wnd->render(&bg.below_rect);
		}
	}

	if(y2 > area.y2()) {
		v2 -= (static_cast<float>(y2 - area.y2())/static_cast<float>(y2 - y1))*(v2 - v1);
		y2 = area.y2();
	}

	if(y2 <= y1) {
		return;
	}

	if(v1 > v2) {
		return;
	}

	//clamp [v1, v2] into the [0.0, 1.0] range
	if(v1 > 1.0f) {
		const float f = std::floor(v1);
		v1 -= f;
		v2 -= f;
	}

	if(v1 < 0.0f) {
		const float diff = v2 - v1;
		const float f = std::floor(-v1);
		v1 = 1.0f - (-v1 - f);
		v2 = v1 + diff;
	}

	int screen_width = area.w();

	const float xscale = static_cast<float>(bg.xscale) / 100.0f;
	float xpos = (-static_cast<float>(bg.xspeed)*static_cast<float>(cycle)/1000.0f + int(static_cast<float>(x + bg.xoffset)*xscale))
		/ static_cast<float>((bg.texture->surfaceWidth()+bg.xpad)*ScaleImage) + static_cast<float>(area.x() - x)/static_cast<float>((bg.texture->surfaceWidth()+bg.xpad)*ScaleImage);

	//clamp xpos into the [0.0, 1.0] range
	if(xpos > 0) {
		xpos -= floor(xpos);
	} else {
		while(xpos < 0) { xpos += 1.0f; }
		//xpos += ceil(-xpos);
	}

	if(bg.xpad > 0) {
		xpos *= 1.0f + static_cast<float>(bg.xpad) / bg.texture->actualWidth();
	}

	x = area.x();

	std::vector<KRE::short_vertex_texcoord> q;

	while(screen_width > 0) {
		const int texture_blit_width = static_cast<int>((1.0f - xpos) * bg.texture->actualWidth() * ScaleImage);
		const int blit_width = std::min(texture_blit_width, screen_width);

		if(blit_width > 0) {
			const float xpos2 = xpos + static_cast<float>(blit_width) / (bg.texture->actualWidth() * 2.0f);

			const short x1 = x;
			const short x2 = (x1 + blit_width);

			const float u1 = bg.texture->getNormalisedTextureCoordW<float>(0, xpos);
			const float u2 = bg.texture->getNormalisedTextureCoordW<float>(0, xpos2);

			q.emplace_back(glm::i16vec2(x1, y1), glm::vec2(u1, v1));
			q.emplace_back(glm::i16vec2(x2, y1), glm::vec2(u2, v1));
			q.emplace_back(glm::i16vec2(x2, y2), glm::vec2(u2, v2));

			q.emplace_back(glm::i16vec2(x2, y2), glm::vec2(u2, v2));
			q.emplace_back(glm::i16vec2(x1, y1), glm::vec2(u1, v1));
			q.emplace_back(glm::i16vec2(x1, y2), glm::vec2(u1, v2));
		}

		x += static_cast<int>(blit_width + bg.xpad * ScaleImage);

		xpos = 0.0f;
		screen_width -= static_cast<int>(blit_width + bg.xpad * ScaleImage);
	}
	bg.attr_->update(&q, bg.attr_->end());
}
