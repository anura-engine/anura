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

#if 0 // needs fixing

#include <cmath>

#include <iostream>
#include <map>

#include "kre/Scissor.hpp"
#include "kre/WindowManager.hpp"

#include "background.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "variant.hpp"
#include "variant_utils.hpp"

namespace 
{
	//a cache key with background name and palette ID.
	typedef std::pair<std::string, int> cache_key;
	typedef std::map<cache_key, std::shared_ptr<background>> bg_cache;
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
void background::load_modified_backgrounds()
{
	static int prev_nitems = 0;
	const int nitems = cache.size();
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

			background backup = *j->second;
			try {
				const std::string path = "data/backgrounds/" + j->second->id_ + ".cfg";
				*j->second = background(json::parse_from_file(path), j->first.second);
			} catch(...) {
				std::cerr << "ERROR REFRESHING BACKGROUND\n";
				error_paths.insert(j->second->file_);
			}
			j->second->id_ = backup.id_;
			j->second->file_ = backup.file_;
		}
	}
}
#endif // NO_EDITOR

std::shared_ptr<background> background::get(const std::string& name, int palette_id)
{
	const cache_key id(name, palette_id);

	std::shared_ptr<background>& obj = cache[id];
	if(!obj) {
		const std::string fname = "data/backgrounds/" + name + ".cfg";
		obj.reset(new background(json::parse_from_file(fname), palette_id));
		obj->id_ = name;
		obj->file_ = module::map_file(fname);
	}

	return obj;
}

std::vector<std::string> background::get_available_backgrounds()
{
	std::vector<std::string> files;
	module::get_files_in_dir("data/backgrounds/", &files);

	std::vector<std::string> result;
	for(const std::string& fname : files) {
		if(fname.size() > 4 && std::equal(fname.end() - 4, fname.end(), ".cfg")) {
			result.push_back(std::string(fname.begin(), fname.end() - 4));
		}
	}

	return result;
}

background::background(variant node, int palette) : palette_(palette)
{
	top_ = KRE::Color(node["top"]);
	bot_ = KRE::Color(node["bottom"]);

	if(palette_ != -1) {
		top_ = graphics::map_palette(top_, palette);
		bot_ = graphics::map_palette(bot_, palette);
	}

	width_ = node["width"].as_int();
	height_ = node["height"].as_int();

	for(variant layer_node : node["layer"].as_list()) {
		layer bg;
		bg.image = layer_node["image"].as_string();
		bg.image_formula = layer_node["image_formula"].as_string_default();
		bg.xscale = layer_node["xscale"].as_int(100);
		bg.yscale_top = bg.yscale_bot = layer_node["yscale"].as_int(100);
		bg.yscale_top = layer_node["yscale_top"].as_int(bg.yscale_top);
		bg.yscale_bot = layer_node["yscale_bot"].as_int(bg.yscale_bot);
		bg.xspeed = layer_node["xspeed"].as_int(0);
		bg.xpad = layer_node["xpad"].as_int(0);
		bg.xoffset = layer_node["xoffset"].as_int(0);
		bg.yoffset = layer_node["yoffset"].as_int(0);
		bg.scale = layer_node["scale"].as_int(1);
		bg.blend = layer_node["blend"].as_bool(true);
		bg.notile = layer_node["notile"].as_bool(false);
		if(bg.scale < 1) {
			bg.scale = 1;
		}

		std::string blend_mode = layer_node["mode"].as_string_default();
#if defined(__GLEW_H__)
		if(GLEW_EXT_blend_minmax) {
			if(blend_mode == "GL_MAX") {
				bg.mode = GL_MAX;
			} else if(blend_mode == "GL_MIN") {
				bg.mode = GL_MIN;
			} else {
				bg.mode = GL_FUNC_ADD;
			}
		}
#endif
		
		bg.color = KRE::Color(layer_node);

		if(layer_node.has_key("color_above")) {
			bg.color_above.reset(new KRE::Color(layer_node["color_above"]));
			if(palette_ != -1) {
				*bg.color_above = graphics::map_palette(*bg.color_above, palette);
			}
		}

		if(layer_node.has_key("color_below")) {
			bg.color_below.reset(new KRE::Color(layer_node["color_below"]));
			if(palette_ != -1) {
				*bg.color_below = graphics::map_palette(*bg.color_below, palette);
			}
		}

		bg.y1 = layer_node["y1"].as_int();
		bg.y2 = layer_node["y2"].as_int();

		bg.foreground = layer_node["foreground"].as_bool(false);
		bg.tile_upwards = layer_node["tile_upwards"].as_bool(false);
		bg.tile_downwards = layer_node["tile_downwards"].as_bool(false);
		layers_.push_back(bg);
	}
}

variant background::write() const
{
	variant_builder res;
	res.add("top", top_.write());
	res.add("bottom", bot_.write());
	res.add("width", formatter() << width_);
	res.add("height", formatter() << height_);

	for(const layer& bg : layers_) {
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

void background::draw(int x, int y, const rect& area, const std::vector<rect>& opaque_areas, int rotation, int cycle) const
{
	auto& wnd = KRE::WindowManager::getMainWindow();
	const int height = height_ + offset_.y*2;

	//set the background colors for the level. The area above 'height' is
	//painted with the top color, and the area below height is painted with
	//the bottom color. For efficiency we do this using color clearing, with
	//scissors to divide the screen into top and bottom.
	if(height < y) {
		//the entire screen is full of the bottom color
		wnd->setClearColor(bot_);
		wnd->clear(KRE::ClearFlags::DISPLAY_CLEAR_COLOR);
	} else if(height > y + wnd->height()) {
		//the entire screen is full of the top color.
		wnd->setClearColor(top_);
		wnd->clear(KRE::ClearFlags::DISPLAY_CLEAR_COLOR);
	} else {
		//both bottom and top colors are on the screen, so draw them both,
		//using scissors to delinate their areas.
		const int dist_from_bottom = y + wnd->height() - height;

		const int scissor_scale = preferences::double_scale() ? 2 : 1;

		//the scissor test does not respect any rotations etc. We use a rotation
		//to transform the iPhone's display, which is fine normally, but
		//here we have to accomodate the iPhone being "on its side"
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
		KRE::Scissor::Manager sm1(rect(dist_from_bottom/scissor_scale, 0, (graphics::screen_height() - dist_from_bottom)/scissor_scale, graphics::screen_width()/scissor_scale));
#else
		KRE::Scissor::Manager sm1(rect(0, dist_from_bottom, preferences::actual_screen_width(), preferences::actual_screen_width()*(1-dist_from_bottom/600)));
#endif
		wnd->setClearColor(top_);
		wnd->clear(KRE::ClearFlags::DISPLAY_CLEAR_COLOR);

#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
		KRE::Scissor::Manager sm2(rect(0, 0, dist_from_bottom/scissor_scale, graphics::screen_width()/scissor_scale));
#else
		KRE::Scissor::Manager sm2(rect(0, 0, preferences::actual_screen_width(), dist_from_bottom));
#endif
		wnd->setClearColor(bot_);
		wnd->clear(KRE::ClearFlags::DISPLAY_CLEAR_COLOR);
	}

	draw_layers(x, y, area, opaque_areas, rotation, cycle);
}

namespace 
{
	void calculate_draw_areas(rect area, std::vector<rect>::const_iterator opaque1, std::vector<rect>::const_iterator opaque2, std::vector<rect>* areas) {
		if(opaque1 == opaque2) {
			areas->push_back(area);
			return;
		}

		rect sub_areas[4];
		for(; opaque1 != opaque2; ++opaque1) {
			const int result = Geometry::rect_difference(area, *opaque1, sub_areas);
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

		areas->push_back(area);
	}
}

void background::draw_layers(int x, int y, const rect& area_ref, const std::vector<rect>& opaque_areas, int rotation, int cycle) const
{
	static std::vector<rect> areas;
	areas.clear();
	calculate_draw_areas(area_ref, opaque_areas.begin(), opaque_areas.end(), &areas);

	for(auto& bg : layers_) {
		if(bg.foreground == false) {

			for(auto& a : areas) {
				draw_layer(x, y, a, rotation, bg, cycle);
			}

			if(!blit_queue.empty() && (i+1 == layers_.end() || i->texture != (i+1)->texture || (i+1)->foreground || i->blend != (i+1)->blend)) {
				if(bg.blend == false) {
					glDisable(GL_BLEND);
				}
				blit_queue.setTexture(bg.texture.getId());
				blit_queue.do_blit();
				blit_queue.clear();
				if(bg.blend == false) {
					glEnable(GL_BLEND);
				}
			}

		}
	}
}

void background::draw_foreground(double xpos, double ypos, int rotation, int cycle) const
{
	for(const layer& bg : layers_) {
		if(bg.foreground) {
			draw_layer(xpos, ypos, rect(xpos, ypos, graphics::screen_width(), graphics::screen_height()), rotation, bg, cycle);
			if(!blit_queue.empty()) {
				blit_queue.setTexture(bg.texture.getId());
				blit_queue.do_blit();
				blit_queue.clear();
			}
		}
	}
}

void background::set_offset(const point& offset)
{
	offset_ = offset;
}

void background::draw_layer(int x, int y, const rect& area, int rotation, const background::layer& bg, int cycle) const
{	
	const double ScaleImage = 2.0;
	GLshort y1 = y + (bg.yoffset+offset_.y)*ScaleImage - (y*bg.yscale_top)/100;
	GLshort y2 = y + (bg.yoffset+offset_.y)*ScaleImage - (y*bg.yscale_bot)/100 +
	                 (bg.y2 - bg.y1)*ScaleImage;

	if(!bg.tile_downwards && y2 <= y) {
		return;
	}

	if(!bg.tile_downwards && y2 <= area.y()) {
		return;
	}

	if(!bg.tile_upwards && y1 > area.y2()) {
		return;
	}

	if(!bg.texture.valid()) {
		if(palette_ == -1) {
			bg.texture = graphics::texture::get(bg.image, bg.image_formula);
		} else {
			bg.texture = graphics::texture::get_palette_mapped(bg.image, palette_);
		}

		if(bg.y2 == 0) {
			bg.y2 = bg.texture.height();
		}
	}

	if(!bg.texture.valid()) {
		return;
	}

	ASSERT_GT(bg.texture.height(), 0);
	ASSERT_GT(bg.texture.width(), 0);

	GLfloat v1 = bg.texture.translate_coord_y(double(bg.y1)/double(bg.texture.height()));
	GLfloat v2 = bg.texture.translate_coord_y(double(bg.y2)/double(bg.texture.height()));

	if(y1 < area.y()) {
		//Making y1 == y2 is problematic, so don't allow it.
		const int target_y = area.y() == y2 ? area.y()-1 : area.y();
		v1 += (GLfloat(target_y - y1)/GLfloat(y2 - y1))*(v2 - v1);
		y1 = target_y;
	}

	if(bg.tile_upwards && y1 > area.y()) {
		v1 -= (GLfloat(y1 - area.y())/GLfloat(y2 - y1))*(v2 - v1);
		y1 = area.y();
	} else if(bg.color_above && y1 > area.y()) {
		glEnable(GL_SCISSOR_TEST);

		const int xpos = area.x() - x;
		const int ypos = y1 - y;
		const int width = area.w();
		const int height = y1 - area.y();
#if TARGET_OS_IPHONE
		glScissor((graphics::screen_height() - ypos)/2, (graphics::screen_width() - (xpos + width))/2, height/2, width/2);
#else
		glScissor(xpos, graphics::screen_height() - ypos, width, height);
#endif
		glClearColor(bg.color_above->r, bg.color_above->g, bg.color_above->b, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_SCISSOR_TEST);
	}

	if(bg.tile_downwards && y2 < area.y() + area.h()) {
		v2 += (GLfloat((area.y() + area.h()) - y2)/GLfloat(y2 - y1))*(v2 - v1);
		y2 = area.y() + area.h();
	} else if(bg.color_below && y2 < area.y() + area.h()) {
		glEnable(GL_SCISSOR_TEST);

		const int xpos = area.x() - x;
		const int ypos = area.y() + area.h() - y;
		const int width = area.w();
		const int height = area.y() + area.h() - y2;

#if TARGET_OS_IPHONE
		glScissor((graphics::screen_height() - ypos)/2, (graphics::screen_width() - (xpos + width))/2, height/2, width/2);
#else
		glScissor(xpos, graphics::screen_height() - ypos, width, height);
#endif

		glClearColor(bg.color_below->r, bg.color_below->g, bg.color_below->b, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_SCISSOR_TEST);
	}

	if(y2 > area.y() + area.h()) {
		v2 -= (GLfloat(y2 - (area.y() + area.h()))/GLfloat(y2 - y1))*(v2 - v1);
		y2 = area.y() + area.h();
	}

	if(y2 <= y1) {
		return;
	}

	if(v1 > v2) {
		return;
	}

	//clamp [v1, v2] into the [0.0, 1.0] range
	if(v1 > 1.0) {
		const GLfloat f = floor(v1);
		v1 -= f;
		v2 -= f;
	}

	if(v1 < 0.0) {
		const GLfloat diff = v2 - v1;
		const GLfloat f = floor(-v1);
		v1 = 1.0 - (-v1 - f);
		v2 = v1 + diff;
	}

	int screen_width = area.w();

	const double xscale = double(bg.xscale)/100.0;
	GLfloat xpos = (-GLfloat(bg.xspeed)*GLfloat(cycle)/1000 + int(GLfloat(x + bg.xoffset)*xscale))/GLfloat((bg.texture.width()+bg.xpad)*ScaleImage) + GLfloat(area.x() - x)/GLfloat((bg.texture.width()+bg.xpad)*ScaleImage);

	//clamp xpos into the [0.0, 1.0] range
	if(xpos > 0) {
		xpos -= floor(xpos);
	} else {
		while(xpos < 0) { xpos += 1.0; }
		//xpos += ceil(-xpos);
	}

	if(bg.xpad > 0) {
		xpos *= GLfloat(bg.texture.width() + bg.xpad)/GLfloat(bg.texture.width());
	}

	glColor4f(bg.color[0], bg.color[1], bg.color[2], bg.color[3]);

#if defined(__GLEW_H__)
	if (GLEW_EXT_blend_minmax && (GLEW_ARB_imaging || GLEW_VERSION_1_4)) {
		glBlendEquation(bg.mode);
	}
#endif

	x = area.x();
	y = area.y();

	while(screen_width > 0) {
		const int texture_blit_width = (1.0 - xpos)*bg.texture.width()*ScaleImage;

		const int blit_width = std::min(texture_blit_width, screen_width);

		if(blit_width > 0) {
			const GLfloat xpos2 = xpos + GLfloat(blit_width)/(GLfloat(bg.texture.width())*2.0);

			const GLshort x1 = x;
			const GLshort x2 = x1 + blit_width;

			const GLfloat u1 = bg.texture.translate_coord_x(xpos);
			const GLfloat u2 = bg.texture.translate_coord_x(xpos2);

			blit_queue.repeat_last();
			blit_queue.add(x1, y1, u1, v1);
			blit_queue.repeat_last();
			blit_queue.add(x2, y1, u2, v1);
			blit_queue.add(x1, y2, u1, v2);
			blit_queue.add(x2, y2, u2, v2);
		}

		x += blit_width + bg.xpad*ScaleImage;

		xpos = 0.0;
		screen_width -= blit_width + bg.xpad*ScaleImage;
	}

	glColor4f(1.0,1.0,1.0,1.0);
#if defined(__GLEW_H__)
	if (GLEW_EXT_blend_minmax && (GLEW_ARB_imaging || GLEW_VERSION_1_4)) {
		glBlendEquation(GL_FUNC_ADD);
	}
#endif
}

#endif
