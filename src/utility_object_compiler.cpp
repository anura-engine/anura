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

#ifdef __linux__
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include <deque>
#include <map>
#include <vector>
#include <sstream>

#include <boost/filesystem.hpp>

#include "kre/SurfaceSDL.hpp"
#include "kre/SurfaceScale.hpp"

#include "SDL_image.h"

#include "Surface.hpp"

#include "asserts.hpp"
#include "custom_object_type.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "frame.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "string_utils.hpp"
#include "surface_cache.hpp"
#include "surface_utils.hpp"
#include "unit_test.hpp"
#include "variant_utils.hpp"

void UTILITY_query(const std::vector<std::string>& args);

namespace 
{
	const int TextureImageSize = 1024;

	struct animation_area 
	{
		explicit animation_area(variant node) 
			: anim(new Frame(node)), 
			  is_particle(false)
		{
			width = 0;
			height = 0;
			for(const auto& f : anim->frameLayout()) {
				width += f.area.w();
				if(f.area.h() > height) {
					height = f.area.h();
				}
			}

			src_image = node["image"].as_string();
			dst_image = -1;
		}
	    
		ffl::IntrusivePtr<Frame> anim;
		int width, height;

		std::string src_image;

		int dst_image;
		rect dst_area;
		bool is_particle;
	};

	typedef std::shared_ptr<animation_area> animation_area_ptr;

	bool operator==(const animation_area& a, const animation_area& b)
	{
		return a.src_image == b.src_image && a.anim->area() == b.anim->area() && a.anim->pad() == b.anim->pad() && a.anim->numFrames() == b.anim->numFrames() && a.anim->numFramesPerRow() == b.anim->numFramesPerRow();
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

namespace 
{
	bool animation_area_has_alpha_channel(animation_area_ptr anim)
	{
		using namespace KRE;
		auto surf = graphics::SurfaceCache::get(anim->src_image);
		if(!surf || surf->getPixelFormat()->bytesPerPixel() != 4) {
			return false;
		}

		const uint32_t* pixels = reinterpret_cast<const uint32_t*>(surf->pixels());

		for(int f = 0; f != anim->anim->numFrames(); ++f) {
			const auto& info = anim->anim->frameLayout()[f];

			const int x = f%anim->anim->numFramesPerRow();
			const int y = f/anim->anim->numFramesPerRow();

			const rect& base_area = anim->anim->area();
			const int xpos = base_area.x() + (base_area.w()+anim->anim->pad())*x;
			const int ypos = base_area.y() + (base_area.h()+anim->anim->pad())*y;
			rect blit_src(xpos + info.x_adjust, ypos + info.y_adjust, info.area.w(), info.area.h());

			for(int x = 0; x != blit_src.w(); ++x) {
				for(int y = 0; y != blit_src.h(); ++y) {
					const int index = (blit_src.y() + y)*surf->width() + (blit_src.x() + x);
					const uint32_t pixel = pixels[index];
					const uint32_t mask = (pixels[index]&surf->getPixelFormat()->getAlphaMask());
					if(mask != 0 && mask != surf->getPixelFormat()->getAlphaMask()) {
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
	using namespace KRE;

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
	for(const std::string& gui : gui_files) {
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

	std::vector<ConstCustomObjectTypePtr> types = CustomObjectType::getAll();
	for(ConstCustomObjectTypePtr type : types) {
		const std::string* path = CustomObjectType::getObjectPath(type->id() + ".cfg");

		//skip any experimental stuff so it isn't compiled
		const std::string Experimental = "experimental";
		if(std::search(path->begin(), path->end(), Experimental.begin(), Experimental.end()) != path->end()) {
			continue;
		}

		LOG_INFO("OBJECT: " << type->id() << " -> " << *path);
		variant obj_node =  json::parse_from_file(*path);
		obj_node = CustomObjectType::mergePrototype(obj_node);
		obj_node.remove_attr(variant("prototype"));

		if(obj_node["editor_info"].is_map() && obj_node["editor_info"]["var"].is_list()) {
			std::vector<std::string> names;
			for(variant entry : obj_node["editor_info"]["var"].as_list()) {
				names.push_back(entry["name"].as_string());
			}

			if(names.empty() == false) {
				std::map<variant, variant> m;
				if(obj_node["vars"].is_map()) {
					m = obj_node["vars"].as_map();
				}

				for(const std::string& name : names) {
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

		for(variant v : obj_node["particle_system"].as_list()) {
			animation_containing_nodes.push_back(v);
		}

		//add nested objects -- disabled for now until we find bugs in it.
		/*
		for(wml::node::child_iterator i = obj_node->begin_child("object_type"); i != obj_node->end_child("object_type"); ++i) {
			animation_containing_nodes.push_back(i->second);
		}
		*/
	}

	for(variant node : animation_containing_nodes) {
		for(const variant_pair& p : node.as_map()) {
			std::string attr_name = p.first.as_string();
			if(attr_name != "animation" && attr_name != "FramedGuiElement" && attr_name != "section") {
				continue;
			}

			for(const variant& v : p.second.as_list()) {

				animation_area_ptr anim(new animation_area(v));
				if(anim->src_image.empty() || v.has_key(variant("palettes")) || std::find(no_compile_images.begin(), no_compile_images.end(), anim->src_image) != no_compile_images.end()) {
					continue;
				}

				animation_areas.push_back(anim);

				for(animation_area_ptr area : animation_areas) {
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

	for(animation_area_ptr anim : animation_areas) {
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
			match = static_cast<int>(output_areas.size());
			output_areas.push_back(output_area(num_output_images++));
		}

		output_area match_area = output_areas[match];
		output_areas.erase(output_areas.begin() + match);
		rect area = use_output_area(match_area, anim->width, anim->height, output_areas);
		anim->dst_image = match_area.image_id;
		anim->dst_area = area;
	}

	std::vector<SurfacePtr> surfaces;
	for(int n = 0; n != num_output_images; ++n) {
		surfaces.emplace_back(Surface::create(TextureImageSize, TextureImageSize, PixelFormat::PF::PIXELFORMAT_ARGB8888));
	}

	for(animation_area_ptr anim : animation_areas) {
		for(animation_area_ptr other : animation_areas) {
			if(anim == other || anim->dst_image != other->dst_image) {
				continue;
			}

			ASSERT_LOG(rects_intersect(anim->dst_area, other->dst_area) == false, "RECTANGLES CLASH: " << anim->dst_image << " " << anim->dst_area << " vs " << other->dst_area);
		}

		ASSERT_INDEX_INTO_VECTOR(anim->dst_image, surfaces);
		SurfacePtr dst = surfaces[anim->dst_image];
		SurfacePtr src = graphics::SurfaceCache::get(anim->src_image);
		ASSERT_LOG(src.get() != nullptr, "COULD NOT LOAD IMAGE: '" << anim->src_image << "'");
		int xdst = 0;
		for(int f = 0; f != anim->anim->numFrames(); ++f) {
			const auto& info = anim->anim->frameLayout()[f];

			const int x = f%anim->anim->numFramesPerRow();
			const int y = f/anim->anim->numFramesPerRow();

			const rect& base_area = anim->anim->area();
			const int xpos = base_area.x() + (base_area.w()+anim->anim->pad())*x;
			const int ypos = base_area.y() + (base_area.h()+anim->anim->pad())*y;
			rect blit_src(xpos + info.x_adjust, ypos + info.y_adjust, info.area.w(), info.area.h());
			rect blit_dst(anim->dst_area.x() + xdst, anim->dst_area.y(), info.area.w(), info.area.h());
			xdst += info.area.w();
			ASSERT_GE(blit_dst.x(), anim->dst_area.x());
			ASSERT_GE(blit_dst.y(), anim->dst_area.y());
			ASSERT_LE(blit_dst.x2(), anim->dst_area.x() + anim->dst_area.w());
			ASSERT_LE(blit_dst.y2(), anim->dst_area.y() + anim->dst_area.h());
			src->setBlendMode(Surface::BlendMode::BLEND_MODE_NONE);
			dst->blitTo(src, blit_src, blit_dst);
		}
	}

	for(int n = 0; n != num_output_images; ++n) {
		std::ostringstream fname;
		fname << "images/compiled-" << n << ".png";

		graphics::set_alpha_for_transparent_colors_in_rgba_surface(surfaces[n]);

		surfaces[n]->savePng(module::get_module_path() + fname.str());
	}

	typedef std::pair<variant, animation_area_ptr> anim_pair;
	for(const anim_pair& a : nodes_to_animation_areas) {
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

		const auto& first_frame = anim->anim->frameLayout().front();
		
		rect r(anim->dst_area.x() - first_frame.x_adjust, anim->dst_area.y() - first_frame.y_adjust, anim->anim->area().w(), anim->anim->area().h());
		node.add_attr_mutation(variant("rect"), r.write());

		int xpos = anim->dst_area.x();

		std::vector<int> v;
		for(const auto& f : anim->anim->frameLayout()) {
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
		for(int n : v) {
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
			for(variant query : script["query"].as_list()) {
				std::vector<std::string> args;
				for(variant arg : query.as_list()) {
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
	rect targetArea;

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
		return static_cast<int>((cell_width() + 3) * frames.size() + 4);
	}
};

bool is_row_blank(KRE::SurfacePtr surf, const unsigned char* pixels)
{
	for(int x = 0; x < surf->width(); ++x) {
		if(pixels[3] > 64) {
			return false;
		}
		pixels += 4;
	}

	return true;
}

bool is_col_blank(KRE::SurfacePtr surf, const SpritesheetRow& row, unsigned col)
{
	if(static_cast<int>(col) >= surf->width()) {
		return true;
	}

	const unsigned char* pixels = reinterpret_cast<const unsigned char*>(surf->pixels());
	pixels += row.begin_row*surf->width() * 4 + col * 4;

	for(int y = row.begin_row; y < row.end_row; ++y) {
		if(pixels[3] > 64) {
			return false;
		}

		pixels += surf->width() * 4;
	}

	return true;
}

std::vector<SpritesheetRow> get_cells(KRE::SurfacePtr surf)
{
	std::vector<SpritesheetRow> rows;

	const unsigned char* pixels = reinterpret_cast<const unsigned char*>(surf->pixels());

	int start_row = -1;
	for(int row = 0; row <= surf->height(); ++row) {
		const bool blank = row == surf->height() || is_row_blank(surf, pixels);
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

		pixels += surf->rowPitch();
	}

	for(SpritesheetRow& sprite_row : rows) {
		int start_col = -1;
		for(int col = 0; col <= surf->width(); ++col) {
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

		LOG_INFO("ROW: " << sprite_row.begin_row << ", " << sprite_row.end_row << " -> " << sprite_row.cells.size());
	}

	return rows;
}

void write_pixel_surface(KRE::SurfacePtr surf, int x, int y, int r, int g, int b, int a)
{
	if(x < 0 || y < 0 || x >= surf->width() || y >= surf->height()) {
		return;
	}

	KRE::SurfaceLock lck(surf);
	unsigned char* pixels = reinterpret_cast<unsigned char*>(surf->pixelsWriteable());
	pixels += y * surf->width() * 4 + x * 4;
	*pixels++ = r;
	*pixels++ = g;
	*pixels++ = b;
	*pixels++ = a;
}

void write_spritesheet_frame(KRE::SurfacePtr src, const rect& src_area, KRE::SurfacePtr dst, int target_x, int target_y)
{
	const unsigned char* alpha_colors = graphics::get_alpha_pixel_colors();

	std::vector<unsigned char*> border_pixels;

	for(int xpos = target_x; xpos < target_x + src_area.w() + 2; ++xpos) {
		unsigned char* p = reinterpret_cast<unsigned char*>(dst->pixelsWriteable()) + (target_y*dst->width() + xpos)*4;
		border_pixels.push_back(p);
		p += (src_area.h()+1)*dst->width()*4;
		border_pixels.push_back(p);
	}

	for(int ypos = target_y; ypos < target_y + src_area.h() + 2; ++ypos) {
		unsigned char* p = reinterpret_cast<unsigned char*>(dst->pixelsWriteable()) + (ypos*dst->width() + target_x)*4;
		border_pixels.push_back(p);
		p += (src_area.w()+1)*4;
		border_pixels.push_back(p);
	}

	for(unsigned char* p : border_pixels) {
		memcpy(p, alpha_colors+3, 3);
		p[3] = 255;
	}
}

bool rect_in_surf_empty(KRE::SurfacePtr surf, rect area)
{
	const unsigned char* p = reinterpret_cast<const unsigned char*>(surf->pixels());
	p += (area.y()*surf->width() + area.x())*4;

	for(int y = 0; y < area.h(); ++y) {
		for(int x = 0; x < area.w(); ++x) {
			if(p[x*4 + 3]) {
				return false;
			}
		}

		p += surf->width() * 4;
	}

	return true;
}

int goodness_of_fit(KRE::SurfacePtr surf, rect areaa, rect areab)
{
	if(areaa.h() > areab.h()) {
		std::swap(areaa, areab);
	}

	bool can_slice = true;
	while(areaa.h() < areab.h() && can_slice) {
		can_slice = false;
		if(rect_in_surf_empty(surf, rect(areab.x(), areab.y(), areab.w(), 1))) {
			std::ostringstream ss;
			ss << "SLICE: " << areab << " -> ";
			areab = rect(areab.x(), areab.y()+1, areab.w(), areab.h()-1);
			ss << areab;
			LOG_INFO(ss.str());
			can_slice = true;
		}

		if(areaa.h() < areab.h() && rect_in_surf_empty(surf, rect(areab.x(), areab.y()+areab.h()-1, areab.w(), 1))) {
			std::ostringstream ss;
			ss << "SLICE: " << areab << " -> ";
			areab = rect(areab.x(), areab.y(), areab.w(), areab.h()-1);
			ss << areab;
			LOG_INFO(ss.str());
			can_slice = true;
		}

		if(areaa.h() == areab.h()) {
			LOG_INFO("SLICED DOWN: " << areab);
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

		int best_score = std::numeric_limits<int>::max();

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
		return std::numeric_limits<int>::max();
	}

	int errors = 0;
	for(int y = 0; y < areaa.h(); ++y) {
		const int ya = areaa.y() + y;
		const int yb = areab.y() + y;
		for(int x = 0; x < areaa.w(); ++x) {
			const int xa = areaa.x() + x;
			const int xb = areab.x() + x;
			const unsigned char* pa = reinterpret_cast<const unsigned char*>(surf->pixels()) + (ya*surf->width() + xa)*4;
			const unsigned char* pb = reinterpret_cast<const unsigned char*>(surf->pixels()) + (yb*surf->width() + xb)*4;
			if((pa[3] > 32) != (pb[3] > 32)) {
				++errors;
			}
		}
	}

	return errors;
}

int score_offset_fit(KRE::SurfacePtr surf, const rect& big_area, const rect& lit_area, int offsetx, int offsety)
{
	int score = 0;
	for(int y = 0; y < big_area.h(); ++y) {
		for(int x = 0; x < big_area.w(); ++x) {
			const unsigned char* big_p = reinterpret_cast<const unsigned char*>(surf->pixels()) + ((big_area.y() + y)*surf->width() + (big_area.x() + x))*4;

			const int xadj = x - offsetx;
			const int yadj = y - offsety;

			if(xadj < 0 || yadj < 0 || xadj >= lit_area.w() || yadj >= lit_area.h()) {
				if(big_p[3] >= 32) {
					++score;
				}
				continue;
			}

			const unsigned char* lit_p = reinterpret_cast<const unsigned char*>(surf->pixels()) + ((lit_area.y() + yadj)*surf->width() + (lit_area.x() + xadj))*4;
			if((big_p[3] >= 32) != (lit_p[3] >= 32)) {
				++score;
			}
		}
	}

	return score;
}

void get_best_offset(KRE::SurfacePtr surf, const rect& big_area, const rect& lit_area, int* xoff, int* yoff)
{
	LOG_INFO("CALC BEST OFFSET...");
	*xoff = *yoff = 0;
	int best_score = -1;
	for(int y = 0; y <= (big_area.h() - lit_area.h()); ++y) {
		for(int x = 0; x <= (big_area.w() - lit_area.w()); ++x) {
			const int score = score_offset_fit(surf, big_area, lit_area, x, y);
			LOG_INFO("OFFSET " << x << ", " << y << " SCORES " << score);
			if(best_score == -1 || score < best_score) {
				*xoff = x;
				*yoff = y;
				best_score = score;
			}
		}
	}

	LOG_INFO("BEST OFFSET: " << *xoff << ", " << *yoff);
}

int find_distance_to_pixel(KRE::SurfacePtr surf, const rect& area, int xoffset, int yoffset)
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
				const unsigned char* p = reinterpret_cast<const unsigned char*>(surf->pixels()) + ((area.y() + ypos)*surf->width() + (area.x() + xpos))*4;
				if(p[3] >= 32) {
					best_distance = distance;
				}
			}
		}
	}

	return best_distance;
}

int score_spritesheet_area(KRE::SurfacePtr surf, const rect& area_a, int xoff_a, int yoff_a, const rect& area_b, int xoff_b, int yoff_b, const rect& big_area)
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
				pa = reinterpret_cast<const unsigned char*>(surf->pixels()) + ((area_a.y() + yadj_a)*surf->width() + (area_a.x() + xadj_a))*4;
			}

			if(xadj_a >= 0 && xadj_a < area_a.w() && yadj_a >= 0 && yadj_a < area_a.h()) {
				pb = reinterpret_cast<const unsigned char*>(surf->pixels()) + ((area_b.y() + yadj_b)*surf->width() + (area_b.x() + xadj_b))*4;
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

void flip_surface_area(KRE::SurfacePtr surf, const rect& area)
{
	for(int y = area.y(); y < area.y() + area.h(); ++y) {
		unsigned int* pixels = reinterpret_cast<unsigned int*>(surf->pixelsWriteable()) + y*surf->width() + area.x();
		std::reverse(pixels, pixels + area.w());
	}
}

void write_spritesheet_animation(KRE::SurfacePtr src, const SpritesheetAnimation& anim, KRE::SurfacePtr dst, bool reorder)
{
	int target_x = anim.targetArea.x()+1;
	int target_y = anim.targetArea.y()+1;

	const int cell_width = anim.cell_width();
	const int cell_height = anim.cell_height();

	rect biggest_rect = anim.frames.front();

	for(const rect& f : anim.frames) {
		LOG_INFO("RECT SIZE: " << f.w() << "," << f.h());
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
			int best_score = std::numeric_limits<int>::max();
			for(unsigned n = 0; n < anim.frames.size(); ++n) {
				if(std::count(frames.begin(), frames.end(), anim.frames[n])) {
					continue;
				}

				const int score = score_spritesheet_area(src, frames.back(), new_xoffsets.back(), new_yoffsets.back(), anim.frames[n], xoffsets[n], yoffsets[n], biggest_rect);
				LOG_INFO("SCORE: " << anim.frames[n] << " vs " << frames.back() << ": " << n << " -> " << score);
				if(score < best_score || best_frame == -1) {
					best_score = score;
					best_frame = n;
				}
			}

			LOG_INFO("BEST : " << best_frame << ": " << best_score);

			frames.push_back(anim.frames[best_frame]);
			new_xoffsets.push_back(xoffsets[best_frame]);
			new_yoffsets.push_back(yoffsets[best_frame]);
		}
	}

	for(const rect& f : frames) {
		int xoff = 0, yoff = 0;
		get_best_offset(src, biggest_rect, f, &xoff, &yoff);

		write_spritesheet_frame(src, f, dst, target_x, target_y);

		rect src_rect(f.x(), f.y(), f.w(), f.h());
		rect dst_rect(target_x+1 + xoff, target_y+1 + yoff, f.w(), f.h());

		src->setBlendMode(KRE::Surface::BlendMode::BLEND_MODE_NONE);
		dst->blitTo(src, src_rect, dst_rect);

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
		} catch(json::ParseError& e) {
			ASSERT_LOG(false, "Parse error parsing " << arg << " -> " << cfg_fname << ": " << e.errorMessage());
		}

		variant baking_info = node["animation_baking"];
		ASSERT_LOG(baking_info.is_map(), "baking info not found");

		KRE::SurfacePtr surf = graphics::SurfaceCache::get(baking_info["source_image"].as_string());
		ASSERT_LOG(surf.get(), "No surface found");

		LOG_INFO("SURFACE SIZE: " << surf->width() << "x" << surf->height());

		LOG_INFO("DEST SURFACE: " << module::map_file("images/" + baking_info["dest_image"].as_string()));

		ASSERT_LOG(surf->getPixelFormat()->bytesPerPixel() == 4, "Incorrect bpp: " << surf->getPixelFormat()->bytesPerPixel());

		std::vector<SpritesheetRow> rows = get_cells(surf);
		unsigned char* pixels = reinterpret_cast<unsigned char*>(surf->pixelsWriteable());
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
						LOG_INFO("SCORE: [" << y << "," << x << "] -> [" << yy << "," << xx << "]: " << score);
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

				LOG_INFO("BEST SCORES FOR [" << y << "," << x << "] " << (cell.end_col - cell.begin_col) << "x" << (row.end_row - row.begin_row) << ": [" << v[0].first << "," << v[0].second << "], [" << v[1].first << "," << v[1].second << "]");

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

				std::ostringstream ss;
				ss << "RECOMMENDED SEQUENCE: ";
				for(auto p : sequence) {
					ss << "[" << p.first << "," << p.second << "], ";
				}
				LOG_INFO(ss.str());
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
				ASSERT_LOG(static_cast<unsigned>(loc[0]) < rows.size(), "Invalid animation cell: " << loc[0] << "/" << rows.size());
				ASSERT_LOG(static_cast<unsigned>(loc[1]) < rows[loc[0]].cells.size(), "Invalid animation cell: " << loc[1] << "/" << rows[loc[0]].cells.size());

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
					LOG_INFO("MATCH: " << area.w() << "x" << area.h() << " " << score);
					if(best == -1 || score < best_score) {
						best = n;
						best_score = score;
					}
					break;
				}
			}

			ASSERT_LOG(best != -1, "Could not find fit for animation " << new_anim.width() << "x" << new_anim.height() << ": " << animations.size());

			new_anim.targetArea = rect(available_space[best].x(), available_space[best].y(), new_anim.width(), new_anim.height());

			const rect right_area(new_anim.targetArea.x2(), new_anim.targetArea.y(), available_space[best].w() - new_anim.targetArea.w(), new_anim.targetArea.h());
			const rect bottom_area(new_anim.targetArea.x(), new_anim.targetArea.y2(), available_space[best].w(), available_space[best].h() - new_anim.targetArea.h());

			available_space.push_back(right_area);
			available_space.push_back(bottom_area);
			LOG_INFO("DIVIDE: " << right_area.w() << "x" << right_area.h() << " " << bottom_area.w() << "x" << bottom_area.h());

			available_space.erase(available_space.begin() + best);

			animations.push_back(new_anim);

			LOG_INFO("FIT ANIM: " << new_anim.targetArea.x() << ", " << new_anim.targetArea.y() << ", " << new_anim.targetArea.w() << ", " << new_anim.targetArea.h());
		}

		auto target_surf = KRE::Surface::create(TargetTextureSize, TargetTextureSize, KRE::PixelFormat::PF::PIXELFORMAT_ARGB8888);
		const unsigned char* alpha_colors = graphics::get_alpha_pixel_colors();
		unsigned char* target_pixels = reinterpret_cast<unsigned char*>(target_surf->pixelsWriteable());
		for(int n = 0; n < target_surf->width() * target_surf->height(); ++n) {
			memcpy(target_pixels, alpha_colors, 3);
			target_pixels[3] = 255;
			target_pixels += 4;
		}

		std::vector<variant> anim_nodes;

		for(const SpritesheetAnimation& anim : animations) {
			write_spritesheet_animation(surf, anim, target_surf, anim.node[variant("auto_adjust")].as_bool(false));

			std::map<variant, variant> node = anim.node.as_map();
			node.erase(variant("frames"));
			rect area(anim.targetArea.x()+2, anim.targetArea.y()+2, anim.cell_width(), anim.cell_height());
			node[variant("rect")] = area.write();
			node[variant("image")] = baking_info["dest_image"];
			node[variant("frames")] = variant(static_cast<int>(anim.frames.size()));
			node[variant("pad")] = variant(3);
			anim_nodes.push_back(variant(&node));
		}

		node.add_attr(variant("animation"), variant(&anim_nodes));

		target_surf->savePng(module::get_module_path() + "/images/" + baking_info["dest_image"].as_string());
		sys::write_file(cfg_fname, node.write_json());
	}
}

namespace 
{
	KRE::SurfacePtr getAndScaleImage(const std::string& img, const std::string& algo, int scale)
	{
		using namespace KRE;
		if(scale == 100) {
			SurfacePtr s = graphics::SurfaceCache::get(img);
			return s;
		} else {
			SurfacePtr s = graphics::SurfaceCache::get(img);
			if(algo == "bicubic") {
				return KRE::scale::bicubic(s, scale);
			} else if(algo == "bilinear") {
				return KRE::scale::bilinear(s, scale);
			} else if(algo == "nearest") {
				return KRE::scale::nearest_neighbour(s, scale);
			} else if(algo == "epx") {
				return KRE::scale::epx(s);
			} else {
				ASSERT_LOG(false, "Unrecognised image algorithm '" << algo << "'.");
				return nullptr;
			}
		}
	}
}

COMMAND_LINE_UTILITY(set_png_rgb_for_alpha_zero)
{
	std::deque<std::string> argv(args.begin(), args.end());
	for(auto s : argv) {
		SDL_Surface* surf = IMG_Load(s.c_str());
		ASSERT_LOG(surf, "Could not load image: " << s);

		uint8_t* pixels = reinterpret_cast<uint8_t*>(surf->pixels);

		for(int n = 0; n != surf->w*surf->h; ++n) {
			if(pixels[3] == 0) {
				pixels[0] = pixels[1] = pixels[2] = 0;
				pixels[3] = 4;
			}
			pixels += 4;
		}

		IMG_SavePNG(surf, s.c_str());
	}
}

COMMAND_LINE_UTILITY(build_spritesheet_from_images)
{
	using namespace KRE;

	std::deque<std::string> argv(args.begin(), args.end());

	std::vector<std::vector<SurfacePtr>> surfaces;
	surfaces.resize(surfaces.size()+1);

	int scale = 100;
	int alpha_threshold = 16;
	std::string algo = "bicubic";
	std::deque<std::string> image_files;
	for(auto itor = argv.begin(); itor != argv.end(); ++itor) {
		if(*itor == "--scale" && itor+1 != argv.end()) {
			++itor;
			scale = atoi(itor->c_str());
		} else if(*itor == "--alg" && itor+1 != argv.end()) {
			++itor;
			algo = *itor;
		} else if(*itor == "--alpha") {
			++itor;
			alpha_threshold = atoi(itor->c_str());
		} else {
			image_files.emplace_back(*itor);
		}
	}

	int row_width = 3;
	int sheet_height = 3;

	int hpad = -1, tpad = -1, bpad = -1;

	std::vector<int> cell_widths;
	std::vector<int> row_heights;
	cell_widths.push_back(0);
	row_heights.push_back(0);
	
	for(auto itor = image_files.begin(); itor != image_files.end(); ) {
		auto path = *itor;
		if(path.empty() == false && path[0] != '-' && sys::is_directory(path)) {
			const int index = itor - image_files.begin();

			std::vector<std::string> files;
			sys::get_files_in_dir(path, &files);

			std::vector<std::string> png;
			for(auto s : files) {
				if(s.size() > 4 && std::equal(s.end()-4,s.end(),".png")) {
					png.push_back(path + "/" + s);
				}
			}

			image_files.erase(image_files.begin() + index);
			image_files.insert(image_files.begin() + index, png.begin(), png.end());

			itor = image_files.begin() + index;
			
		} else {
			++itor;
		}
	}

	int images_per_row = 1024;

	for(auto itor = image_files.begin(); itor != image_files.end(); ++itor) {
		auto img = *itor;
		if(img.size() <= 4 || !std::equal(img.end()-4,img.end(),".png")) {
			continue;
		}

		SurfacePtr s = getAndScaleImage(img, algo, scale);
		ASSERT_LOG(s != nullptr, "No image: " << img);

		const uint8_t* p = (const uint8_t*)s->pixels();
		const uint8_t* end_p = p + s->width()*s->height()*4;

		int top_pad = 0;
		for(int i = 0; i < s->height(); ++i) {
			const uint8_t* top = p + i*4*s->width();

			bool all_clear = true;
			for(int j = 0; j < s->width(); ++j) {
				if(top[j*4+3] > alpha_threshold) {
					all_clear = false;
					break;
				}
			}

			if(!all_clear) {
				break;
			}

			++top_pad;
		}

		int bot_pad = 0;
		for(int i = 0; i < s->height() && top_pad+bot_pad < s->height(); ++i) {
			const uint8_t* bot = p + (s->height()-i-1)*4*s->width();

			bool all_clear = true;
			for(int j = 0; j < s->width(); ++j) {
				if(bot[j*4+3] > alpha_threshold) {
					all_clear = false;
					break;
				}
			}

			if(!all_clear) {
				break;
			}

			++bot_pad;
		}

		int hor_pad = 0;
		for(int i = 0; i < s->width()/2; ++i) {
			const uint8_t* left = p + i*4;
			const uint8_t* right = p + (s->width()-i-1)*4;
			bool all_clear = true;
			for(int j = 0; j < s->height(); ++j) {
				ASSERT_LOG(left + j*(s->width()*4)+3 < end_p, "Off end");
				ASSERT_LOG(right + j*(s->width()*4)+3 < end_p, "Off end");
				if(left[j*(s->width()*4)+3] > alpha_threshold || right[j*(s->width()*4)+3] > alpha_threshold) {
					all_clear = false;
					break;
				}
			}

			if(!all_clear) {
				break;
			}

			++hor_pad;
		}

		fprintf(stderr, "PAD: %d %d/%d\n", hor_pad, top_pad, bot_pad);

		if(bot_pad < bpad || bpad == -1) {
			bpad = bot_pad;
		}

		if(top_pad < tpad || tpad == -1) {
			tpad = top_pad;
		}

		if(hor_pad < hpad || hpad == -1) {
			hpad = hor_pad;
		}
	}

	int image_num = 0;

	for(auto itor = image_files.begin(); itor != image_files.end(); ++itor) {
		auto img = *itor;
		if(img == "--newrow" || image_num == images_per_row) {
			surfaces.resize(surfaces.size()+1);
			cell_widths.push_back(0);
			row_heights.push_back(0);
			row_width = 3;
			sheet_height += 3;
			image_num = 0;

			if(img == "--newrow") {
				continue;
			}
		} else if(img == "--row") {
			++itor;
			ASSERT_LOG(itor != image_files.end(), "row needs arg");
			images_per_row = atoi(itor->c_str());
			continue;
		} else if(img == "--scale") {
			++itor;
			continue;
		} else if(img == "--alg") {
			++itor;
			continue;
		}

		SurfacePtr s = getAndScaleImage(img, algo, scale);
		ASSERT_LOG(s != nullptr, "No image: " << img);
		surfaces.back().push_back(s);

		const int s_w = s->width()-hpad*2;
		row_width += s_w + 3;

		if(s_w > cell_widths.back()) {
			cell_widths.back() = s_w;
		}

		const int s_h = s->height()-tpad-bpad;

		if(s_h > row_heights.back()) {
			sheet_height += s_h - row_heights.back();
			row_heights.back() = s_h;
		}

		++image_num;
	}

	int sheet_width = 0;
	for(int nrow = 0; nrow != surfaces.size(); ++nrow) {
		const int row_width = static_cast<int>(3 + (3+cell_widths[nrow]) * surfaces[nrow].size());
		if(row_width > sheet_width) {
			sheet_width = row_width;
		}
	}

	SurfacePtr sheet = Surface::create(sheet_width, sheet_height, PixelFormat::PF::PIXELFORMAT_ARGB8888);

	int ypos = 2;

	int row_index = 0;
	for(auto row : surfaces) {
		int xpos = 2;
		int max_height = 0;
		for(auto src : row) {
			const int src_w = src->width()-hpad*2;
			const int src_h = src->height()-tpad-bpad;
			rect blit_src(hpad, tpad, src_w, src_h);
			rect blit_dst(xpos, ypos, src_w, src_h);

			printf("x: %d, y: %d, w: %d, h: %d,\n", 2, 2, src_w, src_h);

			rect rect_top(xpos-1, ypos-1, src_w+2, 1);
			rect rect_bot(xpos-1, ypos + src_h, src_w+2, 1);
			rect rect_left(xpos-1, ypos, 1, src_h);
			rect rect_right(xpos + src_w, ypos, 1, src_h);

			src->setBlendMode(Surface::BlendMode::BLEND_MODE_NONE);
			sheet->blitTo(src, blit_src, blit_dst);

			if(blit_src.h() > max_height) {
				max_height = blit_src.h();
			}

			Color transparent = sheet->getPixelFormat()->mapRGB(0xf9, 0x30, 0x3d);
			sheet->fillRect(rect_top, transparent);
			sheet->fillRect(rect_bot, transparent);
			sheet->fillRect(rect_left, transparent);
			sheet->fillRect(rect_right, transparent);

			xpos += cell_widths[row_index] + 3;
		}

		ypos += max_height + 3;
		++row_index;
	}
	sheet->savePng("sheet.png");
}

COMMAND_LINE_UTILITY(make_image_wrap)
{
	using namespace graphics;
	for(auto img : args) {
		auto s = SurfaceCache::get(img);
		{
			// It's good practice to lock the surface before modifying pixels.
			// Plus we assert if the surface requires locking and we don't do it.
			KRE::SurfaceLock lck(s);
			uint8_t* p = reinterpret_cast<uint8_t*>(s->pixelsWriteable());
			if(s->getPixelFormat()->bytesPerPixel() != 4) {
				LOG_INFO("File '" << img << "' is not in a 32-bit format");
				continue;
			}
			if(s->width() * s->getPixelFormat()->bytesPerPixel() != s->rowPitch()) {
				LOG_INFO("File '" << img << "' row pitch won't work with a simple loop, skipping.");
				continue;
			}

			const int BorderWidth = 128;

			for(int y = 0; y != s->height(); ++y) {
				uint8_t* begin_line = p + y*s->rowPitch();
				uint8_t* end_line = begin_line + s->rowPitch()-4;

				int target[4];
				for(int i = 0; i != 4; ++i) {
					target[i] = (int(begin_line[i]) + int(end_line[i]))/2;
				}

				for(int x = 0; x < BorderWidth; ++x) {

					const double mix_ratio = double(x)/double(BorderWidth);

					for(int i = 0; i != 3; ++i) {
						begin_line[i] = 240;
						end_line[i] = 240;
					}

					for(int i = 3; i != 4; ++i) {
						begin_line[i] = int(target[i]*(1.0 - mix_ratio) + begin_line[i]*mix_ratio);
						end_line[i] = int(target[i]*(1.0 - mix_ratio) + end_line[i]*mix_ratio);
					}
					
					begin_line += 4;
					end_line -= 4;
				}
			}
		}
		s->savePng(module::get_module_path() + "/" + img);
	}
}

namespace {
	struct WesnothAnim {
		WesnothAnim() : width(0), height(0) {}
		std::vector<std::string> images;
		int width, height;
		std::vector<SDL_Surface*> surfaces;
	};
}

COMMAND_LINE_UTILITY(generate_wesnoth_spritesheet)
{
	using namespace graphics;

	std::string output;

	std::string obj_type = "unit_avatar";
	std::string prototype = "unit_avatar";
	std::string obj_dir = "units";
	std::string img_prefix = "";
	std::string prefix_override;

	std::deque<std::string> argv;
	
	for(auto s : args) {
		if(s == "--effect") {
			obj_type = "halo_effect";
			prototype = "halo_effect";
			obj_dir = "effects";
		} else if(s.substr(0, 9) == "--prefix=") {
			prefix_override = s.substr(9);
		} else if(s.substr(0, 12) == "--unit-type=") {
			obj_type = "unit_avatar_" + s.substr(12);
			img_prefix = s.substr(12);
		} else if(s[s.size()-1] == '*') {
			s.resize(s.size()-1);
			std::string dir = s;
			while(dir.empty() == false && dir[dir.size()-1] != '/') {
				dir.resize(dir.size()-1);
			}

			if(dir.empty()) {
				dir = ".";
			}

			std::vector<std::string> files;
			sys::get_files_in_dir(dir, &files);

			std::string prefix(s.begin()+dir.size(), s.end());

			for(std::string f : files) {
				if(f.size() >= prefix.size() && std::equal(f.begin(), f.begin()+prefix.size(), prefix.begin())) {
					std::string path = dir + f;
					argv.push_back(path);
				}
			}
		} else {
			argv.push_back(s);
		}
	}

	auto itor = argv.begin();
	while(itor != argv.end()) {
		if(*itor == "--output") {
			++itor;
			output = *itor;
			++itor;
		} else {
			ASSERT_LOG(itor->empty() == false && *itor->begin() != '-', "Illegal argument: " << *itor);
			++itor;
		}
	}

	std::string prefix = argv.front();
	if(prefix.size() <= 4) {
		fprintf(stderr, "Error: image too short\n");
		return;
	}

	prefix.resize(prefix.size()-4);

	for(auto img : argv) {
		if(img.size() > prefix.size()) {
			prefix.resize(img.size());
		}

		while(!std::equal(prefix.begin(), prefix.end(), img.begin())) {
			prefix.resize(prefix.size()-1);
		}
	}

	while(prefix.empty() == false && strchr("_-", prefix[prefix.size()-1])) {
		prefix.resize(prefix.size()-1);
	}

	if(prefix_override.empty() == false) {
		const char* pos = strstr(prefix.c_str(), prefix_override.c_str());
		ASSERT_LOG(pos, "--prefix is not a substring of all images");

		const int index = pos - prefix.c_str();
		prefix.resize(index + prefix_override.size());
	}

	fprintf(stderr, "Using prefix: %s\n", prefix.c_str());

	if(output.empty()) {
		output = prefix;
		while(std::find(output.begin(), output.end(), '/') != output.end()) {
			output.erase(output.begin(), std::find(output.begin(), output.end(), '/')+1);
		}

		std::replace(output.begin(), output.end(), '-', '_');
	}

	std::map<std::string, WesnothAnim > anims;

	for(auto img : argv) {
		std::string anim = img;

		anim.resize(anim.size()-4);
		anim.erase(anim.begin(), anim.begin() + prefix.size());

		while(!anim.empty() && anim[0] == '-') {
			anim.erase(anim.begin(), anim.begin()+1);
		}

		while(!anim.empty() && (isdigit(anim[anim.size()-1]) || anim[anim.size()-1] == '-')) {
			anim.resize(anim.size()-1);
		}

		anims[anim].images.push_back(img);
	}

	for(auto& p : anims) {
		std::sort(p.second.images.begin(), p.second.images.end(), [](std::string a, std::string b) {
			a.resize(a.size()-4);
			b.resize(b.size()-4);

			const char* ap = a.c_str()+a.size() - 1;
			const char* bp = b.c_str()+b.size() - 1;
			while(ap != a.c_str() && isdigit(*ap)) {
				--ap;
			}

			++ap;

			while(bp != a.c_str() && isdigit(*bp)) {
				--bp;
			}

			++bp;

			const int anum = atoi(ap);
			const int bnum = atoi(bp);

			return anum < bnum;
		});
	}

	for(auto& p : anims) {
		fprintf(stderr, "ANIM: %s\n", p.first.c_str());
		for(auto& s : p.second.images) {
			SDL_Surface* surf = IMG_Load(s.c_str());
			fprintf(stderr, "  %s -> %p\n", s.c_str(), surf);
			ASSERT_LOG(surf, "Could not load image: " << s);

			p.second.surfaces.push_back(surf);
			if(surf->w > p.second.width) {
				p.second.width = surf->w;
			}

			if(surf->h > p.second.height) {
				p.second.height = surf->h;
			}
		}
	}

	int sheet_width = 3, sheet_height = 3;

	for(auto& p : anims) {
		sheet_height += 3 + p.second.height;
		int new_width = 3 + (p.second.width+3)*static_cast<int>(p.second.surfaces.size());
		if(new_width > sheet_width) {
			sheet_width = new_width;
		}
	}

	fprintf(stderr, "Creating sheet: %dx%d\n", sheet_width, sheet_height);
	
	using namespace KRE;
	SurfacePtr sheet_surf = Surface::create(sheet_width, sheet_height, PixelFormat::PF::PIXELFORMAT_ARGB8888);
	SDL_Surface* sheet = dynamic_cast<KRE::SurfaceSDL*>(sheet_surf.get())->get();

	variant_builder node;
	node.add("id", obj_type + "_" + output);
	std::vector<variant> proto;
	proto.push_back(variant(prototype));
	node.add("prototype", variant(&proto));

	std::vector<variant> animation_nodes;

	int ypos = 3;
	for(auto& p : anims) {
		int xpos = 3;

		variant_builder anim_node;
		std::string name = p.first;
		if(name.empty()) {
			name = "stand";
		}

		static const std::string reversible[] = { "n", "s", "stand" };

		for(int n = 0; n != sizeof(reversible)/sizeof(*reversible); ++n) {
			if(reversible[n] == name) {
				anim_node.add("reverse", variant::from_bool(true));
				break;
			}
		}

		anim_node.add("id", name);
		anim_node.add("scale", 1);
		anim_node.add("pad", 3);
		anim_node.add("image", obj_dir + "/" + img_prefix + output + ".png");
		anim_node.add("frames", static_cast<int>(p.second.surfaces.size()));
		std::vector<variant> sprite_rect;
		sprite_rect.push_back(variant(xpos));
		sprite_rect.push_back(variant(ypos));
		sprite_rect.push_back(variant(xpos+p.second.width-1));
		sprite_rect.push_back(variant(ypos+p.second.height-1));
		anim_node.add("rect", variant(&sprite_rect));
		anim_node.add("duration", 6);

		for(auto s : p.second.surfaces) {
			Uint32 alpha_rgb = SDL_MapRGB(s->format, 0x3D, 0x30, 0xF9);
			SDL_Rect top = {xpos-1, ypos-1, p.second.width+2, 1};
			SDL_FillRect(sheet, &top, alpha_rgb);

			SDL_Rect bot = {xpos-1, ypos+p.second.height, p.second.width+2, 1};
			SDL_FillRect(sheet, &bot, alpha_rgb);

			SDL_Rect left = {xpos-1, ypos-1, 1, p.second.height+1};
			SDL_FillRect(sheet, &left, alpha_rgb);
			SDL_Rect right = {xpos+p.second.width, ypos-1, 1, p.second.height+1};
			SDL_FillRect(sheet, &right, alpha_rgb);

			int xadj = 0, yadj = 0;
			if(p.second.width > s->w) {
				xadj = (p.second.width - s->w)/2;
			}

			if(p.second.height > s->h) {
				yadj = (p.second.height - s->h)/2;
			}

			SDL_Rect dst = {xpos+xadj, ypos+yadj, p.second.width, p.second.height};
			SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);
			SDL_BlitSurface(s, nullptr, sheet, &dst);
			xpos += p.second.width + 3;
		}

		animation_nodes.push_back(anim_node.build());

		ypos += p.second.height + 3;
	}

	node.add("animation", variant(&animation_nodes));

	IMG_SavePNG(sheet, (std::string("modules/wesnoth2/images/" + obj_dir + "/") + img_prefix + output + ".png").c_str());

	std::string data = node.build().write_json();
	sys::write_file("modules/wesnoth2/data/objects/" + obj_dir + "/" + obj_type + "_" + output + ".cfg", data);

/*
	for(auto img : args) {

		auto s = SurfaceCache::get(img);
		{
			// It's good practice to lock the surface before modifying pixels.
			// Plus we assert if the surface requires locking and we don't do it.
			KRE::SurfaceLock lck(s);
			uint8_t* p = reinterpret_cast<uint8_t*>(s->pixelsWriteable());
			if(s->getPixelFormat()->bytesPerPixel() != 4) {
				LOG_INFO("File '" << img << "' is not in a 32-bit format");
				continue;
			}
			if(s->width() * s->getPixelFormat()->bytesPerPixel() != s->rowPitch()) {
				LOG_INFO("File '" << img << "' row pitch won't work with a simple loop, skipping.");
				continue;
			}

		}
		s->savePng(module::get_module_path() + "/" + img);
	}*/
}

//this is a template utility that can be modified to provide a nice utility
//for manipulating images.
COMMAND_LINE_UTILITY(manipulate_image_template)
{
	using namespace graphics;
	for(auto img : args) {
		auto s = SurfaceCache::get(img);
		{
			// It's good practice to lock the surface before modifying pixels.
			// Plus we assert if the surface requires locking and we don't do it.
			KRE::SurfaceLock lck(s);
			uint8_t* p = reinterpret_cast<uint8_t*>(s->pixelsWriteable());
			if(s->getPixelFormat()->bytesPerPixel() != 4) {
				LOG_INFO("File '" << img << "' is not in a 32-bit format");
				continue;
			}
			if(s->width() * s->getPixelFormat()->bytesPerPixel() != s->rowPitch()) {
				LOG_INFO("File '" << img << "' row pitch won't work with a simple loop, skipping.");
				continue;
			}
			for(int i = 0; i != s->width()*s->height(); ++i) {
				p[3] = p[0];
				p[0] = p[1] = p[2] = 255;
				p += 4;
			}
		}
		s->savePng(module::get_module_path() + "/" + img);
	}
}

COMMAND_LINE_UTILITY(format_json)
{
	std::string in_file;
	std::string out_file;

	for(auto it = args.begin(); it != args.end(); ++it) {
		if(*it == "-i") {
			++it;
			ASSERT_LOG(it != args.end(), "No input argument was given, though -i was specified.");
			in_file = *it;
		} else {
			out_file = *it;
		}
	}

	ASSERT_LOG(!in_file.empty(), "No input file given.");
	ASSERT_LOG(!out_file.empty(), "No output file given.");
	variant v = json::parse_from_file_or_die(in_file);
	sys::write_file(out_file, v.write_json());
}


namespace 
{
	void process_terrain_folder(variant_builder& res, 
		const std::vector<std::string>& names, 
		const std::string& file_name, 
		const std::string& base_folder, 
		bool keep_borders, 
		bool add_file_name=false)
	{
		LOG_INFO("Process base folder: " << base_folder);
		std::vector<std::string> filenames;
		std::vector<std::string> base_filenames;

		std::vector<std::string> base_folder_files;
		std::vector<std::string> base_folder_dirs;
		sys::get_files_in_dir(base_folder, &base_folder_files, &base_folder_dirs);
		if(add_file_name) {
			for(const auto& dir : base_folder_dirs) {
				process_terrain_folder(res, names, file_name.empty() ? dir : file_name + '-' + dir, base_folder + '/' + dir, keep_borders, add_file_name);
			}
		}

		using namespace boost::filesystem;

		if(!names.empty()) {
			for(const auto& f : base_folder_files) {
				for(const auto& base_name : names) {
					if(f.size() >= base_name.size() && f.substr(0, base_name.size()) == base_name) {
						path p(base_folder);
						p /= f;
						filenames.emplace_back(p.generic_string());
						base_filenames.emplace_back(f);
					}
				}
			}
		} else {
			// use all files in directory.
			for(const auto& f : base_folder_files) {
				path p(base_folder);
				p /= f;
				filenames.emplace_back(p.generic_string());
				base_filenames.emplace_back(f);
			}
		}

		if(base_filenames.empty()) {
			return;
		}

		using namespace KRE;

		std::vector<rect> outr;
		std::vector<std::array<int, 4>> borders;	
		auto s = Surface::packImages(filenames, &outr, keep_borders ? nullptr : &borders);
		ASSERT_LOG(s != nullptr, "Couldn't fit all the images into a single spritesheet.");
		s->savePng(file_name + ".png");

		auto rect_it = outr.cbegin();
		auto border_it = borders.cbegin();
		for(const auto& f : base_filenames) {
			variant_builder entry;
			entry.add("rect", rect_it->write());
			if(!keep_borders && ((*border_it)[0] != 0 || (*border_it)[1] != 0 || (*border_it)[2] != 0 || (*border_it)[3] != 0)) {
				for(int n = 0; n != 4; ++n) {
					entry.add("border", (*border_it)[n]);
				}
			}
			if(add_file_name) {
				entry.add("image", file_name + ".png");
			}
			if(keep_borders) {
				const int yp = rect_it->y1()/rect_it->h();
				const int xp = rect_it->x1()/rect_it->w();
				char yc = '0';
				char xc = '0';
				if(yp < 10) {
					yc = yp + '0';
				} else {
					yc = yp + 'A' - 10;
				}
				if(xp < 10) {
					xc = xp + '0';
				} else {
					xc = xp + 'A' - 10;
				}
				std::stringstream sheet_pos;
				sheet_pos << yc << xc;
				entry.add("sheet_pos", sheet_pos.str());
			}

			std::string output_f = file_name.empty() ? f : file_name + '-' + f;
			auto pos = output_f.rfind('.');
			if(pos != std::string::npos) {
				res.add(output_f.substr(0, pos), entry.build());
			} else {
				res.add(output_f, entry.build());
			}

			++rect_it;
			if(!keep_borders) {
				++border_it;
			}
		}
		LOG_INFO("Finished: " << base_folder);
	}
}

COMMAND_LINE_UTILITY(generate_terrain_spritesheet)
{
	// argv is a list of base names to concatenate together to make a spritesheet
	std::vector<std::string> names;
	// base folder to use.
	std::string base_folder;

	bool keep_borders = false;
	bool directory_process = false;

	for(auto it = args.begin(); it != args.end(); ++it) {
		if(*it == "--base") {
			++it;
			ASSERT_LOG(it != args.end(), "No base folder was given, though --base was specified.");
			base_folder = *it;
		} else if(*it == "--keep-borders") {
			keep_borders = true;
		} else if(*it == "-d") {
			directory_process = true;
		} else {
			names.emplace_back(*it);
		}
	}

	ASSERT_LOG(!base_folder.empty(), "No base folder was given. Use --base <folder> to specify.");	
	LOG_DEBUG("Base Folder: " << base_folder);

	variant_builder res;
	if(directory_process) {
		ASSERT_LOG(sys::is_directory(base_folder), "base folder wasn't a directory.");

		process_terrain_folder(res, names, "", base_folder, keep_borders, true);
	} else {
		process_terrain_folder(res, names, "temp", base_folder, keep_borders);
	}
	auto v = res.build();
	std::stringstream ss;
	v.write_json_pretty(ss, "\t");
	sys::write_file("terrain-file-data.json", ss.str());
}

namespace {
const int TileDim = 32;
using namespace KRE;

static const unsigned char alpha_colors[] = {
	0x6f, 0x6d, 0x51,
	0xf9, 0x30, 0x3d,
};

bool is_tile_empty(SurfacePtr surf, const unsigned char* pixels, int tile_x, int tile_y)
{
	for(int x = 0; x != TileDim; ++x) {
		for(int y = 0; y != TileDim; ++y) {
			const int px = tile_x*TileDim + x;
			const int py = tile_y*TileDim + y;

			const unsigned char* p = pixels + py*surf->width()*4 + px*4;

//			fprintf(stderr, "%d, %d: %02x %02x %02x %02x\n", px, py, (unsigned int)p[0], (unsigned int)p[1], (unsigned int)p[2], (unsigned int)p[3]);

			bool is_alpha = memcmp(p, alpha_colors, 3) == 0 || memcmp(p, alpha_colors+3, 3) == 0;
			if(!is_alpha) {
				return false;
			}
		}
	}

	return true;
}

void blit_tile(SurfacePtr surf, unsigned char* pixels, int src_tile_x, int src_tile_y, int dst_tile_x, int dst_tile_y, int rotate)
{
	for(int x = 0; x != TileDim; ++x) {
		for(int y = 0; y != TileDim; ++y) {
			const int px = src_tile_x*TileDim + x;
			const int py = src_tile_y*TileDim + y;
			const unsigned char* p = pixels + py*surf->width()*4 + px*4;
			bool is_alpha = memcmp(p, alpha_colors, 3) == 0 || memcmp(p, alpha_colors+3, 3) == 0;
			if(is_alpha) {
				continue;
			}

			int dst_x = x;
			int dst_y = y;
			if(rotate == 1) {
				dst_y = x;
				dst_x = TileDim - y - 1;
			} else if(rotate == 2) {
				dst_y = TileDim - y - 1;
				dst_x = TileDim - x - 1;
			} else if(rotate == 3) {
				dst_y = TileDim - x - 1;
				dst_x = y;
			}

			const int dpx = dst_tile_x*TileDim + dst_x;
			const int dpy = dst_tile_y*TileDim + dst_y;

			unsigned char* dp = pixels + dpy*surf->width()*4 + dpx*4;

			memcpy(dp, p, 4);
		}
	}
	
}

}

COMMAND_LINE_UTILITY(generate_tilesheet)
{
	using namespace KRE;

	std::deque<std::string> argv(args.begin(), args.end());

	ASSERT_LOG(!argv.empty(), "Must specify name of tilesheet");
	std::string fname = argv.front();
	argv.pop_front();

	SurfacePtr surf = graphics::SurfaceCache::get(fname);
	ASSERT_LOG(surf->getPixelFormat()->bytesPerPixel() == 4, "Incorrect bpp: " << (int)surf->getPixelFormat()->bytesPerPixel());
	if(surf->width() != 512 || surf->width() < 64) {
		ASSERT_LOG(false, "Input tilesheet must be 512 pixels wide and at least 64 pixels tall");
	}

	KRE::SurfaceLock lck(surf);
	unsigned char* pixels = reinterpret_cast<unsigned char*>(surf->pixelsWriteable());

	if(is_tile_empty(surf, pixels, 2, 0)) {
		blit_tile(surf, pixels, 1, 0, 2, 0, 3);
	}

	if(is_tile_empty(surf, pixels, 3, 0)) {
		blit_tile(surf, pixels, 1, 0, 3, 0, 2);
	}

	if(is_tile_empty(surf, pixels, 4, 0)) {
		blit_tile(surf, pixels, 1, 0, 4, 0, 1);
	}

	if(is_tile_empty(surf, pixels, 5, 0)) {
		blit_tile(surf, pixels, 1, 0, 5, 0, 0);
		blit_tile(surf, pixels, 2, 0, 5, 0, 0);
		blit_tile(surf, pixels, 3, 0, 5, 0, 0);
	}

	if(is_tile_empty(surf, pixels, 6, 0)) {
		blit_tile(surf, pixels, 1, 0, 6, 0, 0);
		blit_tile(surf, pixels, 4, 0, 6, 0, 0);
		blit_tile(surf, pixels, 3, 0, 6, 0, 0);
	}

	if(is_tile_empty(surf, pixels, 7, 0)) {
		blit_tile(surf, pixels, 1, 0, 7, 0, 0);
		blit_tile(surf, pixels, 2, 0, 7, 0, 0);
		blit_tile(surf, pixels, 4, 0, 7, 0, 0);
	}

	if(is_tile_empty(surf, pixels, 8, 0)) {
		blit_tile(surf, pixels, 2, 0, 8, 0, 0);
		blit_tile(surf, pixels, 3, 0, 8, 0, 0);
		blit_tile(surf, pixels, 4, 0, 8, 0, 0);
	}

	if(is_tile_empty(surf, pixels, 9, 0)) {
		blit_tile(surf, pixels, 1, 0, 9, 0, 0);
		blit_tile(surf, pixels, 2, 0, 9, 0, 0);
	}

	if(is_tile_empty(surf, pixels, 10, 0)) {
		blit_tile(surf, pixels, 1, 0, 10, 0, 0);
		blit_tile(surf, pixels, 4, 0, 10, 0, 0);
	}

	if(is_tile_empty(surf, pixels, 11, 0)) {
		blit_tile(surf, pixels, 2, 0, 11, 0, 0);
		blit_tile(surf, pixels, 3, 0, 11, 0, 0);
	}

	if(is_tile_empty(surf, pixels, 12, 0)) {
		blit_tile(surf, pixels, 3, 0, 12, 0, 0);
		blit_tile(surf, pixels, 4, 0, 12, 0, 0);
	}

	if(is_tile_empty(surf, pixels, 13, 0)) {
		blit_tile(surf, pixels, 1, 0, 13, 0, 0);
		blit_tile(surf, pixels, 3, 0, 13, 0, 0);
	}

	if(is_tile_empty(surf, pixels, 14, 0)) {
		blit_tile(surf, pixels, 2, 0, 14, 0, 0);
		blit_tile(surf, pixels, 4, 0, 14, 0, 0);
	}

	if(is_tile_empty(surf, pixels, 15, 0)) {
		blit_tile(surf, pixels, 1, 0, 15, 0, 0);
		blit_tile(surf, pixels, 2, 0, 15, 0, 0);
		blit_tile(surf, pixels, 3, 0, 15, 0, 0);
		blit_tile(surf, pixels, 4, 0, 15, 0, 0);
	}

	if(is_tile_empty(surf, pixels, 1, 1)) {
		blit_tile(surf, pixels, 0, 1, 1, 1, 3);
	}

	if(is_tile_empty(surf, pixels, 2, 1)) {
		blit_tile(surf, pixels, 0, 1, 2, 1, 1);
	}

	if(is_tile_empty(surf, pixels, 3, 1)) {
		blit_tile(surf, pixels, 0, 1, 3, 1, 2);
	}


	for(int x = 0; x != 32; ++x) {
		fprintf(stderr, "%d ", is_tile_empty(surf, pixels, x, 0) ? 1 : 0);
	}
	fprintf(stderr, "\n");

	surf->savePng("sheet.png"); //module::get_module_path() + fname.str());

}
