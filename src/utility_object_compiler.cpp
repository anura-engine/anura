/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "graphics.hpp"

#include <boost/shared_ptr.hpp>

#include <map>
#include <vector>
#include <sstream>

#include "asserts.hpp"
#include "custom_object_type.hpp"
#include "filesystem.hpp"
#include "foreach.hpp"
#include "formatter.hpp"
#include "frame.hpp"
#include "geometry.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "string_utils.hpp"
#include "surface.hpp"
#include "surface_cache.hpp"
#include "unit_test.hpp"
#include "variant_utils.hpp"
#include "IMG_savepng.h"

void UTILITY_query(const std::vector<std::string>& args);

namespace {
const int TextureImageSize = 1024;

struct animation_area {
	explicit animation_area(variant node) : anim(new frame(node)), is_particle(false)
	{
		width = 0;
		height = 0;
		foreach(const frame::frame_info& f, anim->frame_layout()) {
			width += f.area.w();
			if(f.area.h() > height) {
				height = f.area.h();
			}
		}

		src_image = node["image"].as_string();
		dst_image = -1;
	}
	    
	boost::intrusive_ptr<frame> anim;
	int width, height;

	std::string src_image;

	int dst_image;
	rect dst_area;
	bool is_particle;
};

typedef boost::shared_ptr<animation_area> animation_area_ptr;

bool operator==(const animation_area& a, const animation_area& b)
{
	return a.src_image == b.src_image && a.anim->area() == b.anim->area() && a.anim->pad() == b.anim->pad() && a.anim->num_frames() == b.anim->num_frames() && a.anim->num_frames_per_row() == b.anim->num_frames_per_row();
}

std::set<animation_area_ptr> animation_areas_with_alpha;
bool animation_area_height_compare(animation_area_ptr a, animation_area_ptr b)
{
	if(a->is_particle != b->is_particle) {
		return a->is_particle;
	}

	if(animation_areas_with_alpha.count(a) != animation_areas_with_alpha.count(b)) {
		return animation_areas_with_alpha.count(a) != 0;
	}

	return a->height > b->height;
}

struct output_area {
	explicit output_area(int n) : image_id(n)
	{
		area = rect(0, 0, TextureImageSize, TextureImageSize);
	}
	int image_id;
	rect area;
};

rect use_output_area(const output_area& input, int width, int height, std::vector<output_area>& areas)
{
	ASSERT_LE(width, input.area.w());
	ASSERT_LE(height, input.area.h());
	rect result(input.area.x(), input.area.y(), width, height);
	if(input.area.h() > height) {
		areas.push_back(output_area(input.image_id));
		areas.back().area = rect(input.area.x(), input.area.y() + height, input.area.w(), input.area.h() - height);
	}

	if(input.area.w() > width) {
		areas.push_back(output_area(input.image_id));
		areas.back().area = rect(input.area.x() + width, input.area.y(), input.area.w() - width, height);
	}

	return result;
}

}

namespace graphics {
void set_alpha_for_transparent_colors_in_rgba_surface(SDL_Surface* s, int options=0);
}

namespace {
bool animation_area_has_alpha_channel(animation_area_ptr anim)
{
	using namespace graphics;
	surface surf = graphics::surface_cache::get(anim->src_image);
	if(!surf || surf->format->BytesPerPixel != 4) {
		return false;
	}

	const uint32_t* pixels = reinterpret_cast<const uint32_t*>(surf->pixels);

	for(int f = 0; f != anim->anim->num_frames(); ++f) {
		const frame::frame_info& info = anim->anim->frame_layout()[f];

		const int x = f%anim->anim->num_frames_per_row();
		const int y = f/anim->anim->num_frames_per_row();

		const rect& base_area = anim->anim->area();
		const int xpos = base_area.x() + (base_area.w()+anim->anim->pad())*x;
		const int ypos = base_area.y() + (base_area.h()+anim->anim->pad())*y;
		SDL_Rect blit_src = {xpos + info.x_adjust, ypos + info.y_adjust, info.area.w(), info.area.h()};

		for(int x = 0; x != blit_src.w; ++x) {
			for(int y = 0; y != blit_src.h; ++y) {
				const int index = (blit_src.y + y)*surf->w + (blit_src.x + x);
				const uint32_t pixel = pixels[index];
				const uint32_t mask = (pixels[index]&surf->format->Amask);
				if(mask != 0 && mask != surf->format->Amask) {
					return true;
				}
			}
		}
	}
	
	return false;
}
}

UTILITY(compile_objects)
{
#ifndef IMPLEMENT_SAVE_PNG
	std::cerr
		<< "This build wasn't done with IMPLEMENT_SAVE_PNG defined. "
		<< "Consquently image files will not be written, aborting requested operation."
		<< std::endl;
	return;
#endif

	using graphics::surface;

	int num_output_images = 0;
	std::vector<output_area> output_areas;
	output_areas.push_back(output_area(num_output_images++));

	std::map<variant, std::string> nodes_to_files;

	std::vector<variant> objects;
	std::vector<animation_area_ptr> animation_areas;
	std::map<variant, animation_area_ptr> nodes_to_animation_areas;

	std::vector<variant> animation_containing_nodes;
	std::vector<std::string> no_compile_images;

	variant gui_node = json::parse_from_file("data/gui.cfg");
	animation_containing_nodes.push_back(gui_node);

	std::map<std::string, variant> gui_nodes;
	std::vector<std::string> gui_files;
	module::get_files_in_dir("data/gui", &gui_files);
	foreach(const std::string& gui, gui_files) {
		if(gui[0] == '.') {
			continue;
		}

		gui_nodes[gui] = json::parse_from_file("data/gui/" + gui);
		animation_containing_nodes.push_back(gui_nodes[gui]);
		if(gui_nodes[gui].has_key("no_compile_image")) {
			std::vector<std::string> images = util::split(gui_nodes[gui][variant("no_compile_image")].as_string());
			no_compile_images.insert(no_compile_images.end(), images.begin(), images.end());
		}
	}

	std::vector<const_custom_object_type_ptr> types = custom_object_type::get_all();
	foreach(const_custom_object_type_ptr type, types) {
		const std::string* path = custom_object_type::get_object_path(type->id() + ".cfg");

		//skip any experimental stuff so it isn't compiled
		const std::string Experimental = "experimental";
		if(std::search(path->begin(), path->end(), Experimental.begin(), Experimental.end()) != path->end()) {
			continue;
		}

		std::cerr << "OBJECT: " << type->id() << " -> " << *path << "\n";
		variant obj_node =  json::parse_from_file(*path);
		obj_node = custom_object_type::merge_prototype(obj_node);
		obj_node.remove_attr(variant("prototype"));

		if(obj_node["editor_info"].is_map() && obj_node["editor_info"]["var"].is_list()) {
			std::vector<std::string> names;
			foreach(variant entry, obj_node["editor_info"]["var"].as_list()) {
				names.push_back(entry["name"].as_string());
			}

			if(names.empty() == false) {
				std::map<variant, variant> m;
				if(obj_node["vars"].is_map()) {
					m = obj_node["vars"].as_map();
				}

				foreach(const std::string& name, names) {
					variant v(name);
					if(m.count(v) == 0) {
						m[v] = variant();
					}
				}

				obj_node.add_attr(variant("vars"), variant(&m));
			}
		}

		objects.push_back(obj_node);
		nodes_to_files[obj_node] = "data/compiled/objects/" + type->id() + ".cfg";


		if(obj_node.has_key("no_compile_image")) {
			std::vector<std::string> images = util::split(obj_node["no_compile_image"].as_string());
			no_compile_images.insert(no_compile_images.end(), images.begin(), images.end());
		}

		animation_containing_nodes.push_back(obj_node);

		foreach(variant v, obj_node["particle_system"].as_list()) {
			animation_containing_nodes.push_back(v);
		}

		//add nested objects -- disabled for now until we find bugs in it.
		/*
		for(wml::node::child_iterator i = obj_node->begin_child("object_type"); i != obj_node->end_child("object_type"); ++i) {
			animation_containing_nodes.push_back(i->second);
		}
		*/
	}

	foreach(variant node, animation_containing_nodes) {
		foreach(const variant_pair& p, node.as_map()) {
			std::string attr_name = p.first.as_string();
			if(attr_name != "animation" && attr_name != "framed_gui_element" && attr_name != "section") {
				continue;
			}

			foreach(const variant& v, p.second.as_list()) {

				animation_area_ptr anim(new animation_area(v));
				if(anim->src_image.empty() || v.has_key(variant("palettes")) || std::find(no_compile_images.begin(), no_compile_images.end(), anim->src_image) != no_compile_images.end()) {
					continue;
				}

				animation_areas.push_back(anim);

				foreach(animation_area_ptr area, animation_areas) {
					if(*area == *anim) {
						anim = area;
						break;
					}
				}

				if(attr_name == "particle_system") {
					anim->is_particle = true;
				}

				if(anim != animation_areas.back()) {
					animation_areas.pop_back();
				}

				nodes_to_animation_areas[v] = anim;

				if(animation_area_has_alpha_channel(anim)) {
					animation_areas_with_alpha.insert(anim);
				}
			}
		}
	}

	std::sort(animation_areas.begin(), animation_areas.end(), animation_area_height_compare);
	{
		std::vector<animation_area_ptr> animation_areas_alpha;

	}

	foreach(animation_area_ptr anim, animation_areas) {
		ASSERT_LOG(anim->width <= 1024 && anim->height <= 1024,
		           "Bad animation area " << anim->width << "x" << anim->height << " for " << anim->src_image << ". Must be 1024x1024 or less.");
		int match = -1;
		int match_diff = -1;
		for(int n = 0; n != output_areas.size(); ++n) {
			if(anim->width <= output_areas[n].area.w() && anim->height <= output_areas[n].area.h()) {
				const int diff = output_areas[n].area.w()*output_areas[n].area.h() - anim->width*anim->height;
				if(match == -1 || diff < match_diff) {
					match = n;
					match_diff = diff;
				}
			}
		}

		if(match == -1) {
			match = output_areas.size();
			output_areas.push_back(output_area(num_output_images++));
		}

		output_area match_area = output_areas[match];
		output_areas.erase(output_areas.begin() + match);
		rect area = use_output_area(match_area, anim->width, anim->height, output_areas);
		anim->dst_image = match_area.image_id;
		anim->dst_area = area;
	}

	std::vector<surface> surfaces;
	for(int n = 0; n != num_output_images; ++n) {
		surfaces.push_back(surface(SDL_CreateRGBSurface(0,TextureImageSize,TextureImageSize,32,SURFACE_MASK)));
	}

	foreach(animation_area_ptr anim, animation_areas) {
		foreach(animation_area_ptr other, animation_areas) {
			if(anim == other || anim->dst_image != other->dst_image) {
				continue;
			}

			ASSERT_LOG(rects_intersect(anim->dst_area, other->dst_area) == false, "RECTANGLES CLASH: " << anim->dst_image << " " << anim->dst_area << " vs " << other->dst_area);
		}

		ASSERT_INDEX_INTO_VECTOR(anim->dst_image, surfaces);
		surface dst = surfaces[anim->dst_image];
		surface src = graphics::surface_cache::get(anim->src_image);
		ASSERT_LOG(src.get() != NULL, "COULD NOT LOAD IMAGE: '" << anim->src_image << "'");
		int xdst = 0;
		for(int f = 0; f != anim->anim->num_frames(); ++f) {
			const frame::frame_info& info = anim->anim->frame_layout()[f];

			const int x = f%anim->anim->num_frames_per_row();
			const int y = f/anim->anim->num_frames_per_row();

			const rect& base_area = anim->anim->area();
			const int xpos = base_area.x() + (base_area.w()+anim->anim->pad())*x;
			const int ypos = base_area.y() + (base_area.h()+anim->anim->pad())*y;
			SDL_Rect blit_src = {xpos + info.x_adjust, ypos + info.y_adjust, info.area.w(), info.area.h()};
			SDL_Rect blit_dst = {anim->dst_area.x() + xdst,
			                     anim->dst_area.y(),
								 info.area.w(), info.area.h()};
			xdst += info.area.w();
			ASSERT_GE(blit_dst.x, anim->dst_area.x());
			ASSERT_GE(blit_dst.y, anim->dst_area.y());
			ASSERT_LE(blit_dst.x + blit_dst.w, anim->dst_area.x() + anim->dst_area.w());
			ASSERT_LE(blit_dst.y + blit_dst.h, anim->dst_area.y() + anim->dst_area.h());
			SDL_SetSurfaceBlendMode(src.get(), SDL_BLENDMODE_NONE);
			SDL_BlitSurface(src.get(), &blit_src, dst.get(), &blit_dst);
		}
	}

	for(int n = 0; n != num_output_images; ++n) {
		std::ostringstream fname;
		fname << "images/compiled-" << n << ".png";

		graphics::set_alpha_for_transparent_colors_in_rgba_surface(surfaces[n].get());

		IMG_SavePNG((module::get_module_path() + fname.str()).c_str(), surfaces[n].get(), -1);
	}

	typedef std::pair<variant, animation_area_ptr> anim_pair;
	foreach(const anim_pair& a, nodes_to_animation_areas) {
		variant node = a.first;
		animation_area_ptr anim = a.second;
		std::ostringstream fname;
		fname << "compiled-" << anim->dst_image << ".png";
		node.add_attr_mutation(variant("image"), variant(fname.str()));
		node.remove_attr_mutation(variant("x"));
		node.remove_attr_mutation(variant("y"));
		node.remove_attr_mutation(variant("w"));
		node.remove_attr_mutation(variant("h"));
		node.remove_attr_mutation(variant("pad"));

		const frame::frame_info& first_frame = anim->anim->frame_layout().front();
		
		rect r(anim->dst_area.x() - first_frame.x_adjust, anim->dst_area.y() - first_frame.y_adjust, anim->anim->area().w(), anim->anim->area().h());
		node.add_attr_mutation(variant("rect"), r.write());

		int xpos = anim->dst_area.x();

		std::vector<int> v;
		foreach(const frame::frame_info& f, anim->anim->frame_layout()) {
			ASSERT_EQ(f.area.w() + f.x_adjust + f.x2_adjust, anim->anim->area().w());
			ASSERT_EQ(f.area.h() + f.y_adjust + f.y2_adjust, anim->anim->area().h());
			v.push_back(f.x_adjust);
			v.push_back(f.y_adjust);
			v.push_back(f.x2_adjust);
			v.push_back(f.y2_adjust);
			v.push_back(xpos);
			v.push_back(anim->dst_area.y());
			v.push_back(f.area.w());
			v.push_back(f.area.h());

			xpos += f.area.w();
		}

		std::vector<variant> vs;
		foreach(int n, v) {
			vs.push_back(variant(n));
		}

		node.add_attr_mutation(variant("frame_info"), variant(&vs));
	}

	for(std::map<variant, std::string>::iterator i = nodes_to_files.begin(); i != nodes_to_files.end(); ++i) {
		variant node = i->first;
		module::write_file(i->second, node.write_json());
	}

	module::write_file("data/compiled/gui.cfg", gui_node.write_json());

	for(std::map<std::string, variant>::iterator i = gui_nodes.begin();
	    i != gui_nodes.end(); ++i) {
		module::write_file("data/compiled/gui/" + i->first, i->second.write_json());
	}

	if(sys::file_exists("./compile-objects.cfg")) {
		variant script = json::parse(sys::read_file("./compile-objects.cfg"));
		if(script["query"].is_list()) {
			foreach(variant query, script["query"].as_list()) {
				std::vector<std::string> args;
				foreach(variant arg, query.as_list()) {
					args.push_back(arg.as_string());
				}

				UTILITY_query(args);
			}
		}
	}
}

namespace {

struct SpritesheetCell {
	int begin_col, end_col;
};

struct SpritesheetRow {
	int begin_row, end_row;

	std::vector<SpritesheetCell> cells;
};

struct SpritesheetAnimation {
	std::vector<rect> frames;
	variant node;
	rect target_area;

	int cell_width() const {
		int result = 0;
		for(const rect& r : frames) {
			result = std::max<int>(r.w(), result);
		}

		return result;
	}

	int cell_height() const {
		int result = 0;
		for(const rect& r : frames) {
			result = std::max<int>(r.h(), result);
		}

		return result;
	}

	int height() const {
		return cell_height() + 4;
	}

	int width() const {
		return (cell_width()+3)*frames.size() + 4;
	}
};

bool is_row_blank(graphics::surface surf, const unsigned char* pixels)
{
	for(int x = 0; x < surf->w; ++x) {
		if(pixels[3] > 64) {
			return false;
		}
		pixels += 4;
	}

	return true;
}

bool is_col_blank(graphics::surface surf, const SpritesheetRow& row, int col)
{
	if(col >= surf->w) {
		return true;
	}

	const unsigned char* pixels = (const unsigned char*)surf->pixels;
	pixels += row.begin_row*surf->w*4 + col*4;

	for(int y = row.begin_row; y < row.end_row; ++y) {
		if(pixels[3] > 64) {
			return false;
		}

		pixels += surf->w*4;
	}

	return true;
}

std::vector<SpritesheetRow> get_cells(graphics::surface surf)
{
	std::vector<SpritesheetRow> rows;

	const unsigned char* pixels = (const unsigned char*)surf->pixels;

	int start_row = -1;
	for(int row = 0; row <= surf->h; ++row) {
		const bool blank = row == surf->h || is_row_blank(surf, pixels);
		if(blank) {
			if(start_row != -1) {
				SpritesheetRow new_row;
				new_row.begin_row = start_row;
				new_row.end_row = row;
				rows.push_back(new_row);

				start_row = -1;
			}
		} else {
			if(start_row == -1) {
				start_row = row;
			}
		}

		pixels += surf->w*4;
	}

	for(SpritesheetRow& sprite_row : rows) {
		int start_col = -1;
		for(int col = 0; col <= surf->w; ++col) {
			const bool blank = is_col_blank(surf, sprite_row, col);
			if(blank) {
				if(start_col != -1) {
					SpritesheetCell new_cell = { start_col, col };
					sprite_row.cells.push_back(new_cell);

					start_col = -1;
				}
			} else {
				if(start_col == -1) {
					start_col = col;
				}
			}
		}

		std::cerr << "ROW: " << sprite_row.begin_row << ", " << sprite_row.end_row << " -> " << sprite_row.cells.size() << "\n";
	}

	return rows;
}

void write_pixel_surface(graphics::surface surf, int x, int y, int r, int g, int b, int a)
{
	if(x < 0 || y < 0 || x >= surf->w || y >= surf->h) {
		return;
	}

	unsigned char* pixels = (unsigned char*)surf->pixels;
	pixels += y * surf->w * 4 + x * 4;
	*pixels++ = r;
	*pixels++ = g;
	*pixels++ = b;
	*pixels++ = a;
}

void write_spritesheet_frame(graphics::surface src, const rect& src_area, graphics::surface dst, int target_x, int target_y)
{
	const unsigned char* alpha_colors = graphics::get_alpha_pixel_colors();

	std::vector<unsigned char*> border_pixels;

	for(int xpos = target_x; xpos < target_x + src_area.w() + 2; ++xpos) {
		unsigned char* p = (unsigned char*)dst->pixels + (target_y*dst->w + xpos)*4;
		border_pixels.push_back(p);
		p += (src_area.h()+1)*dst->w*4;
		border_pixels.push_back(p);
	}

	for(int ypos = target_y; ypos < target_y + src_area.h() + 2; ++ypos) {
		unsigned char* p = (unsigned char*)dst->pixels + (ypos*dst->w + target_x)*4;
		border_pixels.push_back(p);
		p += (src_area.w()+1)*4;
		border_pixels.push_back(p);
	}

	for(unsigned char* p : border_pixels) {
		memcpy(p, alpha_colors+3, 3);
		p[3] = 255;
	}
}

bool rect_in_surf_empty(graphics::surface surf, rect area)
{
	const unsigned char* p = (const unsigned char*)surf->pixels;
	p += (area.y()*surf->w + area.x())*4;

	for(int y = 0; y < area.h(); ++y) {
		for(int x = 0; x < area.w(); ++x) {
			if(p[x*4 + 3]) {
				return false;
			}
		}

		p += surf->w*4;
	}

	return true;
}

int goodness_of_fit(graphics::surface surf, rect areaa, rect areab)
{
	if(areaa.h() > areab.h()) {
		std::swap(areaa, areab);
	}

	bool can_slice = true;
	while(areaa.h() < areab.h() && can_slice) {
		can_slice = false;
		if(rect_in_surf_empty(surf, rect(areab.x(), areab.y(), areab.w(), 1))) {
			std::cerr << "SLICE: " << areab << " -> ";
			areab = rect(areab.x(), areab.y()+1, areab.w(), areab.h()-1);
			std::cerr << areab << "\n";
			can_slice = true;
		}

		if(areaa.h() < areab.h() && rect_in_surf_empty(surf, rect(areab.x(), areab.y()+areab.h()-1, areab.w(), 1))) {
			std::cerr << "SLICE: " << areab << " -> ";
			areab = rect(areab.x(), areab.y(), areab.w(), areab.h()-1);
			std::cerr << areab << "\n";
			can_slice = true;
		}

		if(areaa.h() == areab.h()) {
			std::cerr << "SLICED DOWN: " << areab << "\n";
		}
	}

	if(areaa.h() < areab.h() && areab.h() - areaa.h() <= 4) {
		const int diff = areab.h() - areaa.h();
		areab = rect(areab.x(), areab.y() + diff/2, areab.w(), areab.h() - diff);
	}

	if(areaa.w() != areab.w() && areaa.h() == areab.h()) {
		rect a = areaa;
		rect b = areab;
		if(a.w() > b.w()) {
			std::swap(a,b);
		}

		int best_score = INT_MAX;

		for(int xoffset = 0; xoffset < b.w() - a.w(); ++xoffset) {
			rect r(b.x() + xoffset, b.y(), a.w(), b.h());
			const int score = goodness_of_fit(surf, r, a);
			if(score < best_score) {
				best_score = score;
			}
		}

		return best_score;
	}

	if(areaa.w() != areab.w() || areaa.h() != areab.h()) {
		return INT_MAX;
	}

	int errors = 0;
	for(int y = 0; y < areaa.h(); ++y) {
		const int ya = areaa.y() + y;
		const int yb = areab.y() + y;
		for(int x = 0; x < areaa.w(); ++x) {
			const int xa = areaa.x() + x;
			const int xb = areab.x() + x;
			const unsigned char* pa = (const unsigned char*)surf->pixels + (ya*surf->w + xa)*4;
			const unsigned char* pb = (const unsigned char*)surf->pixels + (yb*surf->w + xb)*4;
			if((pa[3] > 32) != (pb[3] > 32)) {
				++errors;
			}
		}
	}

	return errors;
}

int score_offset_fit(graphics::surface surf, const rect& big_area, const rect& lit_area, int offsetx, int offsety)
{
	int score = 0;
	for(int y = 0; y < big_area.h(); ++y) {
		for(int x = 0; x < big_area.w(); ++x) {
			const unsigned char* big_p = (const unsigned char*)surf->pixels + ((big_area.y() + y)*surf->w + (big_area.x() + x))*4;

			const int xadj = x - offsetx;
			const int yadj = y - offsety;

			if(xadj < 0 || yadj < 0 || xadj >= lit_area.w() || yadj >= lit_area.h()) {
				if(big_p[3] >= 32) {
					++score;
				}
				continue;
			}

			const unsigned char* lit_p = (const unsigned char*)surf->pixels + ((lit_area.y() + yadj)*surf->w + (lit_area.x() + xadj))*4;
			if((big_p[3] >= 32) != (lit_p[3] >= 32)) {
				++score;
			}
		}
	}

	return score;
}

void get_best_offset(graphics::surface surf, const rect& big_area, const rect& lit_area, int* xoff, int* yoff)
{
	std::cerr << "CALC BEST OFFSET...\n";
	*xoff = *yoff = 0;
	int best_score = -1;
	for(int y = 0; y <= (big_area.h() - lit_area.h()); ++y) {
		for(int x = 0; x <= (big_area.w() - lit_area.w()); ++x) {
			const int score = score_offset_fit(surf, big_area, lit_area, x, y);
			std::cerr << "OFFSET " << x << ", " << y << " SCORES " << score << "\n";
			if(best_score == -1 || score < best_score) {
				*xoff = x;
				*yoff = y;
				best_score = score;
			}
		}
	}

	std::cerr << "BEST OFFSET: " << *xoff << ", " << *yoff << "\n";
}

int find_distance_to_pixel(graphics::surface surf, const rect& area, int xoffset, int yoffset)
{
	const int SearchDistance = 4;
	int best_distance = SearchDistance+1;
	for(int y = -SearchDistance; y <= SearchDistance; ++y) {
		for(int x = -SearchDistance; x <= SearchDistance; ++x) {
			const int distance = abs(x) + abs(y);
			if(distance >= best_distance) {
				continue;
			}

			int xpos = xoffset + x;
			int ypos = yoffset + y;

			if(xpos >= 0 && ypos >= 0 && xpos < area.w() && ypos < area.h()) {
				const unsigned char* p = (const unsigned char*)surf->pixels + ((area.y() + ypos)*surf->w + (area.x() + xpos))*4;
				if(p[3] >= 32) {
					best_distance = distance;
				}
			}
		}
	}

	return best_distance;
}

int score_spritesheet_area(graphics::surface surf, const rect& area_a, int xoff_a, int yoff_a, const rect& area_b, int xoff_b, int yoff_b, const rect& big_area)
{
	unsigned char default_color[4] = {0,0,0,0};
	int score = 0;
	for(int y = 0; y < big_area.h(); ++y) {
		for(int x = 0; x < big_area.w(); ++x) {
			const int xadj_a = x - xoff_a;
			const int yadj_a = y - yoff_a;

			const int xadj_b = x - xoff_b;
			const int yadj_b = y - yoff_b;

			const unsigned char* pa = default_color;
			const unsigned char* pb = default_color;

			if(xadj_a >= 0 && xadj_a < area_a.w() && yadj_a >= 0 && yadj_a < area_a.h()) {
				pa = (const unsigned char*)surf->pixels + ((area_a.y() + yadj_a)*surf->w + (area_a.x() + xadj_a))*4;
			}

			if(xadj_a >= 0 && xadj_a < area_a.w() && yadj_a >= 0 && yadj_a < area_a.h()) {
				pb = (const unsigned char*)surf->pixels + ((area_b.y() + yadj_b)*surf->w + (area_b.x() + xadj_b))*4;
			}

			if((pa[3] >= 32) != (pb[3] >= 32)) {
				if(pa[3] >= 32) {
					score += find_distance_to_pixel(surf, area_b, xadj_b, yadj_b);
				} else {
					score += find_distance_to_pixel(surf, area_a, xadj_a, yadj_a);
				}

			}
		}
	}

	return score;
}

void flip_surface_area(graphics::surface surf, const rect& area)
{
	for(int y = area.y(); y < area.y() + area.h(); ++y) {
		unsigned int* pixels = (unsigned int*)surf->pixels + y*surf->w + area.x();
		std::reverse(pixels, pixels + area.w());
	}
}

void write_spritesheet_animation(graphics::surface src, const SpritesheetAnimation& anim, graphics::surface dst, bool reorder)
{
	int target_x = anim.target_area.x()+1;
	int target_y = anim.target_area.y()+1;

	const int cell_width = anim.cell_width();
	const int cell_height = anim.cell_height();

	rect biggest_rect = anim.frames.front();

	for(const rect& f : anim.frames) {
		std::cerr << "RECT SIZE: " << f.w() << "," << f.h() << "\n";
		if(f.w()*f.h() > biggest_rect.w()*biggest_rect.h()) {
			biggest_rect = f;
		}
	}

	std::vector<int> xoffsets, yoffsets, new_xoffsets, new_yoffsets;
	for(const rect& f : anim.frames) {
		xoffsets.push_back(0);
		yoffsets.push_back(0);
		get_best_offset(src, biggest_rect, f, &xoffsets.back(), &yoffsets.back());
	}

	std::vector<rect> frames = anim.frames;
	if(reorder) {
		frames.clear();
		frames.push_back(anim.frames.front());
		new_xoffsets.push_back(xoffsets[0]);
		new_yoffsets.push_back(yoffsets[0]);
		while(frames.size() < anim.frames.size()) {
			int best_frame = -1;
			int best_score = INT_MAX;
			for(int n = 0; n < anim.frames.size(); ++n) {
				if(std::count(frames.begin(), frames.end(), anim.frames[n])) {
					continue;
				}

				const int score = score_spritesheet_area(src, frames.back(), new_xoffsets.back(), new_yoffsets.back(), anim.frames[n], xoffsets[n], yoffsets[n], biggest_rect);
				std::cerr << "SCORE: " << anim.frames[n] << " vs " << frames.back() << ": " << n << " -> " << score << "\n";
				if(score < best_score || best_frame == -1) {
					best_score = score;
					best_frame = n;
				}
			}

			std::cerr << "BEST : " << best_frame << ": " << best_score << "\n";

			frames.push_back(anim.frames[best_frame]);
			new_xoffsets.push_back(xoffsets[best_frame]);
			new_yoffsets.push_back(yoffsets[best_frame]);
		}
	}

	for(const rect& f : frames) {
		int xoff = 0, yoff = 0;
		get_best_offset(src, biggest_rect, f, &xoff, &yoff);

		write_spritesheet_frame(src, f, dst, target_x, target_y);

		SDL_Rect src_rect = { f.x(), f.y(), f.w(), f.h() };
		SDL_Rect dst_rect = { target_x+1 + xoff, target_y+1 + yoff, f.w(), f.h() };

		SDL_SetSurfaceBlendMode(src.get(), SDL_BLENDMODE_NONE);
		SDL_BlitSurface(src.get(), &src_rect, dst.get(), &dst_rect);

		flip_surface_area(dst, rect(target_x, target_y, cell_width, cell_height));

		target_x += cell_width + 3;
	}
}

}

COMMAND_LINE_UTILITY(bake_spritesheet)
{
	std::deque<std::string> argv(args.begin(), args.end());
	while(argv.empty() == false) {
		std::string arg = argv.front();
		argv.pop_front();
		std::string cfg_fname = module::map_file(arg);
		variant node;
		try {
			node = json::parse(sys::read_file(cfg_fname));
		} catch(json::parse_error& e) {
			ASSERT_LOG(false, "Parse error parsing " << arg << " -> " << cfg_fname << ": " << e.error_message());
		}

		variant baking_info = node["animation_baking"];
		ASSERT_LOG(baking_info.is_map(), "baking info not found");

		graphics::surface surf = graphics::surface_cache::get(baking_info["source_image"].as_string());
		ASSERT_LOG(surf.get(), "No surface found");

		std::cerr << "SURFACE SIZE: " << surf->w << "x" << surf->h << "\n";

		std::cerr << "DEST SURFACE: " << module::map_file("images/" + baking_info["dest_image"].as_string()) << "\n";

		ASSERT_LOG(surf->format->BytesPerPixel == 4, "Incorrect bpp: " << surf->format->BytesPerPixel);

		std::vector<SpritesheetRow> rows = get_cells(surf);
		unsigned char* pixels = (unsigned char*)surf->pixels;
		for(const SpritesheetRow& row : rows) {
			for(const SpritesheetCell& cell : row.cells) {
				const int x1 = cell.begin_col - 1;
				const int x2 = cell.end_col;
				const int y1 = row.begin_row - 1;
				const int y2 = row.end_row;

				for(int x = x1; x <= x2; ++x) {
					write_pixel_surface(surf, x, y1, 255, 255, 255, 255);
					write_pixel_surface(surf, x, y2, 255, 255, 255, 255);
				}

				for(int y = y1; y <= y2; ++y) {
					write_pixel_surface(surf, x1, y, 255, 255, 255, 255);
					write_pixel_surface(surf, x2, y, 255, 255, 255, 255);
				}
			}
		}
/*
		typedef std::map<int, std::vector<std::pair<int,int> > > ScoresMap;

		std::map<std::pair<int,int>, ScoresMap> all_scores;

		for(int y = 0; y < rows.size(); ++y) {
			const SpritesheetRow& row = rows[y];
			for(int x = 0; x < row.cells.size(); ++x) {
				const SpritesheetCell& cell = row.cells[x];
				ScoresMap scores;

				for(int yy = 0; yy < rows.size(); ++yy) {
					const SpritesheetRow& r = rows[yy];
					for(int xx = 0; xx < row.cells.size(); ++xx) {
						const SpritesheetCell& c = row.cells[xx];
						if(xx == x && yy == y) {
							continue;
						}

						const rect areaa(cell.begin_col, row.begin_row, cell.end_col - cell.begin_col, row.end_row - row.begin_row);
						const rect areab(c.begin_col, r.begin_row, c.end_col - c.begin_col, r.end_row - r.begin_row);
						const int score = goodness_of_fit(surf, areaa, areab);
						std::cerr << "SCORE: [" << y << "," << x << "] -> [" << yy << "," << xx << "]: " << score << "\n";
						scores[score].resize(scores[score].size()+1);
						scores[score].back().first = yy;
						scores[score].back().second = xx;
					}
				}


				auto itor = scores.begin();
				std::vector<std::pair<int, int> > v = itor->second;
				if(v.size() <= 1) {
					++itor;
					v.push_back(itor->second.front());
				}

				std::cerr << "BEST SCORES FOR [" << y << "," << x << "] " << (cell.end_col - cell.begin_col) << "x" << (row.end_row - row.begin_row) << ": [" << v[0].first << "," << v[0].second << "], [" << v[1].first << "," << v[1].second << "]\n";

				all_scores[std::pair<int,int>(y, x)] = scores;
			}
		}

		for(int y = 0; y < rows.size(); ++y) {
			const SpritesheetRow& row = rows[y];
			for(int x = 0; x < row.cells.size(); ++x) {
				const SpritesheetCell& cell = row.cells[x];

				std::set<std::pair<int,int> > seen;
				seen.insert(std::pair<int,int>(y,x));

				std::vector<std::pair<int,int> > sequence;
				sequence.push_back(std::pair<int,int>(y,x));

				for(;;) {
					std::pair<int,int> value(-1,-1);
					bool found = false;
					const ScoresMap& scores = all_scores[sequence.back()];
					for(auto i = scores.begin(); i != scores.end() && !found; ++i) {
						for(auto j = i->second.begin(); j != i->second.end(); ++j) {
							if(!seen.count(*j)) {
								if(i->first < 1000) {
									value = *j;
								}
								found = true;
								break;
							}
						}

						if(found) {
							break;
						}
					}

					if(value.first == -1) {
						break;
					}

					seen.insert(value);
					sequence.push_back(value);
				}

				std::cerr << "RECOMMENDED SEQUENCE: ";
				for(auto p : sequence) {
					std::cerr << "[" << p.first << "," << p.second << "], ";
				}

				std::cerr << "\n";
			}
		}
*/
		const int TargetTextureSize = 4096;
		std::vector<rect> available_space;
		available_space.push_back(rect(0, 0, TargetTextureSize, TargetTextureSize));

		std::vector<SpritesheetAnimation> animations;
		for(const variant& anim : baking_info["animations"].as_list()) {
			SpritesheetAnimation new_anim;
			new_anim.node = anim;
			std::vector<variant> frames = anim["frames"].as_list();
			for(const variant& fr : frames) {
				std::vector<int> loc = fr.as_list_int();
				assert(loc.size() == 2);
				ASSERT_LOG(loc[0] < rows.size(), "Invalid animation cell: " << loc[0] << "/" << rows.size());
				ASSERT_LOG(loc[1] < rows[loc[0]].cells.size(), "Invalid animation cell: " << loc[1] << "/" << rows[loc[0]].cells.size());

				const SpritesheetRow& r = rows[loc[0]];
				const SpritesheetCell& c = r.cells[loc[1]];

				const rect area(c.begin_col, r.begin_row, c.end_col - c.begin_col, r.end_row - r.begin_row);
				new_anim.frames.push_back(area);
			}

			int best = -1;
			int best_score = -1;
			for(int n = 0; n != available_space.size(); ++n) {
				const rect& area = available_space[n];
				if(new_anim.width() <= area.w() && new_anim.height() <= area.h()) {
					int score = area.w()*area.h();
					fprintf(stderr, "MATCH: %dx%d %d\n", area.w(), area.h(), score);
					if(best == -1 || score < best_score) {
						best = n;
						best_score = score;
					}
					break;
				}
			}

			ASSERT_LOG(best != -1, "Could not find fit for animation " << new_anim.width() << "x" << new_anim.height() << ": " << animations.size());

			new_anim.target_area = rect(available_space[best].x(), available_space[best].y(), new_anim.width(), new_anim.height());

			const rect right_area(new_anim.target_area.x2(), new_anim.target_area.y(), available_space[best].w() - new_anim.target_area.w(), new_anim.target_area.h());
			const rect bottom_area(new_anim.target_area.x(), new_anim.target_area.y2(), available_space[best].w(), available_space[best].h() - new_anim.target_area.h());

			available_space.push_back(right_area);
			available_space.push_back(bottom_area);
			fprintf(stderr, "DIVIDE: %dx%d %dx%d\n", right_area.w(), right_area.h(), bottom_area.w(), bottom_area.h());

			available_space.erase(available_space.begin() + best);

			animations.push_back(new_anim);

			fprintf(stderr, "FIT ANIM: %d, %d, %d, %d\n", new_anim.target_area.x(), new_anim.target_area.y(), new_anim.target_area.w(), new_anim.target_area.h());
		}

		graphics::surface target_surf(SDL_CreateRGBSurface(0,TargetTextureSize,TargetTextureSize,32,SURFACE_MASK));
		const unsigned char* alpha_colors = graphics::get_alpha_pixel_colors();
		unsigned char* target_pixels = (unsigned char*)target_surf->pixels;
		for(int n = 0; n < target_surf->w*target_surf->h; ++n) {
			memcpy(target_pixels, alpha_colors, 3);
			target_pixels[3] = 255;
			target_pixels += 4;
		}

		std::vector<variant> anim_nodes;

		for(const SpritesheetAnimation& anim : animations) {
			write_spritesheet_animation(surf, anim, target_surf, anim.node[variant("auto_adjust")].as_bool(false));

			std::map<variant, variant> node = anim.node.as_map();
			node.erase(variant("frames"));
			rect area(anim.target_area.x()+2, anim.target_area.y()+2, anim.cell_width(), anim.cell_height());
			node[variant("rect")] = area.write();
			node[variant("image")] = baking_info["dest_image"];
			node[variant("frames")] = variant(static_cast<int>(anim.frames.size()));
			node[variant("pad")] = variant(3);
			anim_nodes.push_back(variant(&node));
		}

		node.add_attr(variant("animation"), variant(&anim_nodes));

		IMG_SavePNG((module::get_module_path() + "/images/" + baking_info["dest_image"].as_string()).c_str(), target_surf.get(), -1);

		sys::write_file(cfg_fname, node.write_json());
	}
}

//this is a template utility that can be modified to provide a nice utility
//for manipulating images.
COMMAND_LINE_UTILITY(manipulate_image_template)
{
	using namespace graphics;
	for(auto img : args) {
		surface s = surface_cache::get(img);
		uint8_t* p = (uint8_t*)s->pixels;
		for(int i = 0; i != s->w*s->h; ++i) {
			p[3] = p[0];
			p[0] = p[1] = p[2] = 255;
			p += 4;
		}

		IMG_SavePNG((module::get_module_path() + "/images/" + img).c_str(), s.get(), -1);
	}
}
