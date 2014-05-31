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
#include <assert.h>

#include <algorithm>
#include <iostream>

#include <boost/lexical_cast.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "asserts.hpp"
#include "fbo_scene.hpp"
#include "foreach.hpp"
#include "frame.hpp"
#include "level.hpp"
#include "module.hpp"
#include "object_events.hpp"
#include "obj_reader.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "rectangle_rotator.hpp"
#include "solid_map.hpp"
#include "sound.hpp"
#include "string_utils.hpp"
#include "surface_cache.hpp"
#include "surface_formula.hpp"
#include "surface_palette.hpp"
#include "texture.hpp"
#include "variant_utils.hpp"

PREF_FLOAT(global_frame_scale, 2.0, "Sets the global frame scales for all frames in all animations");

namespace {

	std::set<frame*>& palette_frames() {
		static std::set<frame*>* instance = new std::set<frame*>;
		return *instance;
}

unsigned int current_palette_mask = 0;
}

void frame::build_patterns(variant obj_variant)
{
	using namespace graphics;

	if(!obj_variant["animation"].is_list()) {
		return;
	}

	static const std::string ImagesPath = "./images/";

	std::vector<variant> items = obj_variant["animation"].as_list();
	for(variant item : items) {
		variant pattern = item["image_pattern"];
		if(!pattern.is_string()) {
			continue;
		}

		const std::string path = ImagesPath + pattern.as_string();

		std::string dir;
		std::vector<std::string> files;
		module::get_files_matching_wildcard(path, &dir, &files);

		assert(dir.size() > ImagesPath.size() && std::equal(ImagesPath.begin(), ImagesPath.end(), dir.c_str()));

		dir.erase(dir.begin(), dir.begin() + ImagesPath.size());

		ASSERT_LOG(files.empty() == false, pattern.debug_location() << ": Could not find any images matching path: " << pattern.as_string());

		std::sort(files.begin(), files.end());

		std::vector<surface> surfaces;
		for(const std::string& fname : files) {
			surfaces.push_back(surface_cache::get_no_cache(dir + "/" + fname));

			ASSERT_LOG(surfaces.back()->w == surfaces.front()->w &&
			           surfaces.back()->h == surfaces.front()->h,
					   pattern.debug_location() << ": All images in image pattern must be the same size: " << fname);
			ASSERT_LOG(surfaces.back()->w <= 2048 && surfaces.back()->h <= 2048, "Image too large: " << fname);
		}

		int frames_per_row = files.size();
		int total_width = surfaces.front()->w*surfaces.size();
		int total_height = surfaces.front()->h;
		while(total_width > 2048) {
			frames_per_row = frames_per_row/2 + frames_per_row%2;
			total_width /= 2;
			total_height *= 2;
		}

		ASSERT_LOG(total_height <= 2048, pattern.debug_location() << ": Animation too large: cannot fit in 2048x2048: " << pattern.as_string());

		const int texture_width = texture::next_power_of_2(total_width);
		const int texture_height = texture::next_power_of_2(total_height);

		surface sheet(SDL_CreateRGBSurface(0, texture_width, texture_height, 32, SURFACE_MASK));

		for(int n = 0; n != surfaces.size(); ++n) {
			surface src = surfaces[n];
			SDL_Rect blit_src = {0, 0, surfaces.front()->w, surfaces.front()->h};
			const int xframe = n%frames_per_row;
			const int yframe = n/frames_per_row;
			SDL_Rect blit_dst = {xframe*surfaces.front()->w, yframe*surfaces.front()->h, blit_src.w, blit_src.h};
			SDL_SetSurfaceBlendMode(src.get(), SDL_BLENDMODE_NONE);
			SDL_BlitSurface(src.get(), &blit_src, sheet.get(), &blit_dst);
		}

		graphics::texture tex = graphics::texture::get_no_cache(sheet);
		boost::intrusive_ptr<texture_object> tex_obj(new texture_object(tex));

		std::vector<variant> area;
		area.push_back(variant(0));
		area.push_back(variant(0));
		area.push_back(variant(surfaces.front()->w-1));
		area.push_back(variant(surfaces.front()->h-1));

		item.add_attr_mutation(variant("fbo"), variant(tex_obj.get()));
		item.add_attr_mutation(variant("image"), variant("fbo"));
		item.add_attr_mutation(variant("rect"), variant(&area));
		item.add_attr_mutation(variant("frames_per_row"), variant(frames_per_row));
		item.add_attr_mutation(variant("frames"), variant(surfaces.size()));
		item.add_attr_mutation(variant("pad"), variant(0));
	}
}

frame::frame(variant node)
   : id_(node["id"].as_string()),
     variant_id_(id_),
     enter_event_id_(get_object_event_id("enter_" + id_ + "_anim")),
	 end_event_id_(get_object_event_id("end_" + id_ + "_anim")),
	 leave_event_id_(get_object_event_id("leave_" + id_ + "_anim")),
	 process_event_id_(get_object_event_id("process_" + id_)),
	 solid_(solid_info::create(node)),
     collide_rect_(node.has_key("collide") ? rect(node["collide"]) :
	               rect(node["collide_x"].as_int(),
                        node["collide_y"].as_int(),
                        node["collide_w"].as_int(),
                        node["collide_h"].as_int())),
	 hit_rect_(node.has_key("hit") ? rect(node["hit"]) :
	               rect(node["hit_x"].as_int(),
				        node["hit_y"].as_int(),
				        node["hit_w"].as_int(),
				        node["hit_h"].as_int())),
	 platform_rect_(node.has_key("platform") ? rect(node["platform"]) :
	                rect(node["platform_x"].as_int(),
	                     node["platform_y"].as_int(),
	                     node["platform_w"].as_int(), 1)),
	 img_rect_(node.has_key("rect") ? rect(node["rect"]) :
	           rect(node["x"].as_int(),
	                node["y"].as_int(),
	                node["w"].as_int(),
	                node["h"].as_int())),
	 feet_x_(node["feet_x"].as_int(img_rect_.w()/2)),
	 feet_y_(node["feet_y"].as_int(img_rect_.h()/2)),
	 accel_x_(node["accel_x"].as_int(INT_MIN)),
	 accel_y_(node["accel_y"].as_int(INT_MIN)),
	 velocity_x_(node["velocity_x"].as_int(INT_MIN)),
	 velocity_y_(node["velocity_y"].as_int(INT_MIN)),
	 nframes_(node["frames"].as_int(1)),
	 nframes_per_row_(node["frames_per_row"].as_int(-1)),
	 frame_time_(node["duration"].as_int(-1)),
	 reverse_frame_(node["reverse"].as_bool()),
	 play_backwards_(node["play_backwards"].as_bool()),
	 scale_(node["scale"].as_decimal(decimal(g_global_frame_scale)).as_float()),
	 pad_(node["pad"].as_int()),
	 rotate_(node["rotate"].as_int()),
	 blur_(node["blur"].as_int()),
	 rotate_on_slope_(node["rotate_on_slope"].as_bool()),
	 damage_(node["damage"].as_int()),
	 sounds_(util::split(node["sound"].as_string_default())),
	 force_no_alpha_(node["force_no_alpha"].as_bool(false)),
	 no_remove_alpha_borders_(node["no_remove_alpha_borders"].as_bool(false)),
	 collision_areas_inside_frame_(true),
	 current_palette_(-1), 
	 back_face_culling_(node["cull"].as_bool(false))
{
	if(node.has_key("obj") == false) {
		image_ = node["image"].as_string();
		if(node.has_key("fbo")) {
			texture_ = node["fbo"].convert_to<texture_object>()->texture();
		} else {
			texture_ = graphics::texture::get(image_, node["image_formula"].as_string_default());
		}
	}

	std::vector<std::string> hit_frames = util::split(node["hit_frames"].as_string_default());
	foreach(const std::string& f, hit_frames) {
		hit_frames_.push_back(boost::lexical_cast<int>(f));
	}

	const std::string& events = node["events"].as_string_default();
	if(!events.empty()) {
		//events are in the format time0:time1:...:timen:event0,time0:time1:...:timen:event1,...
		std::vector<std::string> event_vector = util::split(events);
		std::map<int, std::string> event_map;
		foreach(const std::string& e, event_vector) {
			std::vector<std::string> time_event = util::split(e, ':');
			if(time_event.size() < 2) {
				continue;
			}

			const std::string& event = time_event.back();

			for(unsigned int n = 0; n < time_event.size() - 1; ++n) {
				const int time = atoi(time_event[n].c_str());
				event_map[time] = event;
			}
		}

		typedef std::pair<int,std::string> event_pair;
		foreach(const event_pair& p, event_map) {
			event_frames_.push_back(p.first);
			event_names_.push_back(p.second);
		}
	}

	static const std::string AreaPostfix = "_area";
	foreach(const variant_pair& val, node.as_map()) {
		const std::string& attr = val.first.as_string();
		if(attr.size() <= AreaPostfix.size() || std::equal(AreaPostfix.begin(), AreaPostfix.end(), attr.end() - AreaPostfix.size()) == false || attr == "solid_area" ||attr == "platform_area") {
			continue;
		}

		const std::string area_id = std::string(attr.begin(), attr.end() - AreaPostfix.size());

		variant value = val.second;

		bool solid = false;
		rect r;
		if(value.is_null()) {
			continue;
		} else if(value.is_string() && value.as_string() == "all") {
			r = rect(0, 0, width(), height());
		} else if(value.is_list()) {
			std::vector<int> v;
			foreach(const variant& var, value.as_list()) {
				if(var.is_int()) {
					v.push_back(var.as_int());
				} else if(var.is_string() && var.as_string() == "solid") {
					solid = true;
				} else if(var.is_string() && var.as_string() == "all") {
					r = rect(0, 0, width(), height());
				} else {
					ASSERT_LOG(false, "Unrecognized attribute for '" << attr << "': " << value.to_debug_string());
				}
			}

			if(v.empty() == false) {
				r = rect(v);
				r = rect(r.x()*scale_, r.y()*scale_, r.w()*scale_, r.h()*scale_);
			}
		}

		collision_area area = { area_id, r, solid };
		collision_areas_.push_back(area);

		if(solid && (r.x() < 0 || r.y() < 0 || r.x2() > width() || r.y2() > height())) {
			collision_areas_inside_frame_ = false;
		}
	}

	if(node.has_key("frame_info")) {
		ASSERT_LOG(node.has_key("obj"), "'frame_info' and 'obj' attributes don't play well together.");
		std::vector<int> values = node["frame_info"].as_list_int();
		int num_values = values.size();

		ASSERT_GT(num_values, 0);
		ASSERT_EQ(num_values%8, 0);
		ASSERT_LE(num_values, 1024);
		const int* i = &values[0];
		const int* i2 = &values[0] + num_values;
		while(i != i2) {
			frame_info info;
			info.x_adjust = *i++;
			info.y_adjust = *i++;
			info.x2_adjust = *i++;
			info.y2_adjust = *i++;
			const int x = *i++;
			const int y = *i++;
			const int w = *i++;
			const int h = *i++;
			info.area = rect(x, y, w, h);
			frames_.push_back(info);
			ASSERT_EQ(intersection_rect(info.area, rect(0, 0, texture_.width(), texture_.height())), info.area);
			ASSERT_EQ(w + (info.x_adjust + info.x2_adjust), img_rect_.w());
			ASSERT_EQ(h + (info.y_adjust + info.y2_adjust), img_rect_.h());

		}

		ASSERT_EQ(frames_.size(), nframes_);

		build_alpha_from_frame_info();
	} else {
		build_alpha();
	}

	std::vector<std::string> palettes = parse_variant_list_or_csv_string(node["palettes"]);
	foreach(const std::string& p, palettes) {
		palettes_recognized_.push_back(graphics::get_palette_id(p));
	}

	if(palettes_recognized_.empty() == false) {
		palette_frames().insert(this);
		if(current_palette_mask) {
			set_palettes(current_palette_mask);
		}
	}

	foreach(const variant_pair& value, node.as_map()) {
		static const std::string PivotPrefix = "pivot_";
		const std::string& attr = value.first.as_string();
		if(attr.size() > PivotPrefix.size() && std::equal(PivotPrefix.begin(), PivotPrefix.end(), attr.begin())) {
			pivot_schedule schedule;
			schedule.name = std::string(attr.begin() + PivotPrefix.size(), attr.end());

			std::vector<int> values = value.second.as_list_int();

			ASSERT_LOG(values.size()%2 == 0, "PIVOT POINTS IN INCORRECT FORMAT, ODD NUMBER OF INTEGERS");
			const int num_points = values.size()/2;

			int repeat = std::max<int>(1, (nframes_*frame_time_)/std::max<int>(1, num_points));
			for(int n = 0; n != num_points; ++n) {
				point p(values[n*2], values[n*2+1]);
				for(int m = 0; m != repeat; ++m) {
					schedule.points.push_back(p);
				}
			}

			if(reverse_frame_) {
				std::vector<point> v = schedule.points;
				std::reverse(v.begin(), v.end());
				schedule.points.insert(schedule.points.end(), v.begin(), v.end());
			}

			if(schedule.points.empty() == false) {
				pivots_.push_back(schedule);
			}
		}
	}

	if(node.has_key("obj")) {
		if(node["obj"].is_string()) {
			std::vector<obj::obj_data> odata;
			obj::load_obj_file(node["obj"].as_string(), odata);
			ASSERT_LOG(!odata.empty(), "No data read from .obj file: " << node["obj"].as_string());

			const size_t vbo_cnt = odata.size();
			vbo_array_ = graphics::vbo_array(new GLuint[vbo_cnt], graphics::vbo_deleter(vbo_cnt));
			glGenBuffers(vbo_cnt, &vbo_array_[0]);

			int bufcnt = 0;
			for(auto o : odata) {
				ASSERT_LOG(o.face_vertices.size() == o.face_normals.size(), "Number of vertices != number of normals: " << o.face_vertices.size()/3 << " != " << o.face_normals.size()/3);
				ASSERT_LOG(o.face_vertices.size()/3 == o.face_uvs.size()/2, "Number of vertices != number of uv co-ords: " << o.face_vertices.size()/3 << " != " << o.face_uvs.size()/2);
				draw_data_3d dd3d;
				dd3d.num_vertices = 3;
				dd3d.vertex_count = o.face_vertices.size()/3;
				dd3d.vertex_offset = 0;
				dd3d.texture_offset = o.face_vertices.size() * sizeof(GLfloat);
				dd3d.normal_offset = (o.face_uvs.size() + o.face_vertices.size()) * sizeof(GLfloat);
				dd3d.mtl = o.mtl;
				dd3d.vbo_cnt = bufcnt;

				if(dd3d.mtl.tex_ambient.empty() == false) {
					dd3d.tex_a = graphics::texture::get(dd3d.mtl.tex_ambient);
				}
				if(dd3d.mtl.tex_diffuse.empty() == false) {
					dd3d.tex_d = graphics::texture::get(dd3d.mtl.tex_diffuse);
				}
				if(dd3d.mtl.tex_specular.empty() == false) {
					dd3d.tex_s = graphics::texture::get(dd3d.mtl.tex_specular);
				}

				glBindBuffer(GL_ARRAY_BUFFER, vbo_array_[bufcnt]);
				const size_t data_size = (o.face_vertices.size() + o.face_normals.size() + o.face_uvs.size()) * sizeof(GLfloat);
				glBufferData(GL_ARRAY_BUFFER, data_size, NULL, GL_STATIC_DRAW);
				glBufferSubData(GL_ARRAY_BUFFER, dd3d.vertex_offset, o.face_vertices.size() * sizeof(GLfloat), &o.face_vertices[0]);
				glBufferSubData(GL_ARRAY_BUFFER, dd3d.texture_offset, o.face_uvs.size() * sizeof(GLfloat), &o.face_uvs[0]);
				glBufferSubData(GL_ARRAY_BUFFER, dd3d.normal_offset, o.face_normals.size() * sizeof(GLfloat), &o.face_normals[0]);
				glBindBuffer(GL_ARRAY_BUFFER, 0);

				dd3d_array_.push_back(dd3d);
				++bufcnt;
			}
		} else {
			ASSERT_LOG(node["obj"].is_list(), "Attribute 'obj' must either be a a string or list of strings.");
			// XXX
		}
	} else {
		draw_data_3d dd3d;
		dd3d.normal_offset = 0;
		dd3d.vertex_offset = 0;
		std::vector<GLfloat> vertex_data;
		const int vbo_cnt = 1;
		vbo_array_ = graphics::vbo_array(new GLuint[vbo_cnt], graphics::vbo_deleter(vbo_cnt));
		glGenBuffers(vbo_cnt, &vbo_array_[0]);

		if(node.has_key("vertices")) {
			const variant& vertices = node["vertices"];
			ASSERT_LOG(vertices.is_list(), "Attribute 'vertices' must be a list type.");

			dd3d.num_vertices = vertices[0].num_elements();
			for(int n = 0; n != vertices.num_elements(); ++n) {
				ASSERT_LOG(vertices[n].is_list(), "Each element of the 'vertices' list must be a list.");
				ASSERT_LOG(vertices[n].num_elements() == dd3d.num_vertices, "Each element in 'vertices' list must have same number of co-ordinates.");
				for(int m = 0; m != vertices[n].num_elements(); ++m) {
					vertex_data.push_back(GLfloat(vertices[n][m].as_decimal().as_float()));
				}
			}

		} else {
			vertex_data.push_back(0);	vertex_data.push_back(0);	vertex_data.push_back(0);
			vertex_data.push_back(1);	vertex_data.push_back(0);	vertex_data.push_back(0);
			vertex_data.push_back(1);	vertex_data.push_back(1);	vertex_data.push_back(0);

			vertex_data.push_back(1);	vertex_data.push_back(1);	vertex_data.push_back(0);
			vertex_data.push_back(0);	vertex_data.push_back(1);	vertex_data.push_back(0);
			vertex_data.push_back(0);	vertex_data.push_back(0);	vertex_data.push_back(0);
			dd3d.num_vertices = 3;
		}

		std::vector<GLfloat> texcoords;
		if(node.has_key("texcoords")) {
			const variant& tc = node["texcoords"];
			ASSERT_LOG(tc.is_list(), "Attribute 'texcoords' must be a list type.");
			for(int n = 0; n != tc.num_elements(); ++n) {
				ASSERT_LOG(tc[n].is_list(), "Each element of the 'texcoords' list must be a list.");
				for(int m = 0; m != tc[n].num_elements(); ++m) {
					texcoords.push_back(GLfloat(tc[n][m].as_decimal().as_float()));
				}
			}
		} else {

			for(int t = 0; t < nframes_; ++t) {
				const frame_info* info = NULL;
				GLfloat rect[4];
				get_rect_in_texture(frame_time_ > 0 ? t * frame_time_ : t, &rect[0], info);
				rect[0] = texture_.translate_coord_x(rect[0]);
				rect[1] = texture_.translate_coord_y(rect[1]);
				rect[2] = texture_.translate_coord_x(rect[2]);
				rect[3] = texture_.translate_coord_y(rect[3]);

				texcoords.push_back(rect[2]); texcoords.push_back(rect[3]);
				texcoords.push_back(rect[0]); texcoords.push_back(rect[3]);
				texcoords.push_back(rect[0]); texcoords.push_back(rect[1]);
	
				texcoords.push_back(rect[0]); texcoords.push_back(rect[1]);
				texcoords.push_back(rect[2]); texcoords.push_back(rect[1]);
				texcoords.push_back(rect[2]); texcoords.push_back(rect[3]);
			}
		}

		const size_t data_size = (vertex_data.size() + texcoords.size()) * sizeof(GLfloat);
		dd3d.vertex_count = vertex_data.size()/dd3d.num_vertices;
		dd3d.vbo_cnt = 0;
		glBindBuffer(GL_ARRAY_BUFFER, vbo_array_[0]);
		glBufferData(GL_ARRAY_BUFFER, data_size, NULL, GL_STATIC_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_data.size()*sizeof(GLfloat), &vertex_data[0]);
		dd3d.texture_offset = vertex_data.size()*sizeof(GLfloat);
		dd3d.tex_a = texture_;

		glBufferSubData(GL_ARRAY_BUFFER, dd3d.texture_offset, texcoords.size()*sizeof(GLfloat), &texcoords[0]);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		dd3d_array_.push_back(dd3d);
	}
}

frame::~frame()
{
	if(palettes_recognized_.empty() == false) {
		palette_frames().erase(this);
	}
}

void frame::set_palettes(unsigned int palettes)
{
	if(current_palette_ >= 0 && (1 << current_palette_) == palettes) {
		return;
	}

	int npalette = 0;
	while(palettes) {
		if((palettes&1) && std::count(palettes_recognized_.begin(), palettes_recognized_.end(), npalette)) {
			break;
		}
		++npalette;
		palettes >>= 1;
	}

	if(palettes == 0) {
		if(current_palette_ != -1) {
			texture_ = graphics::texture::get(image_);
			current_palette_ = -1;
		}
		return;
	}

	texture_ = graphics::texture::get_palette_mapped(image_, npalette);
	current_palette_ = npalette;
}

void frame::set_color_palette(unsigned int palettes)
{
	current_palette_mask = palettes;
	for(std::set<frame*>::iterator i = palette_frames().begin(); i != palette_frames().end(); ++i) {
		(*i)->set_palettes(palettes);
	}
}

void frame::set_image_as_solid()
{
	solid_ = solid_info::create_from_texture(texture_, img_rect_);
}

void frame::play_sound(const void* object) const
{
	if (sounds_.empty() == false){
		int randomNum = rand()%sounds_.size();  //like a 1d-size die
		if(sounds_[randomNum].empty() == false) {
			sound::play(sounds_[randomNum], object);
		}
	}
}

void frame::build_alpha_from_frame_info()
{
	if(!texture_.valid()) {
		return;
	}

	alpha_.resize(nframes_*img_rect_.w()*img_rect_.h(), true);
	for(int n = 0; n < nframes_; ++n) {
		const rect& area = frames_[n].area;
		int dst_index = frames_[n].y_adjust*img_rect_.w()*nframes_ + n*img_rect_.w() + frames_[n].x_adjust;
		for(int y = 0; y != area.h(); ++y) {
			ASSERT_INDEX_INTO_VECTOR(dst_index, alpha_);
			std::vector<bool>::iterator dst = alpha_.begin() + dst_index;

			ASSERT_LT(area.x(), texture_.width());
			ASSERT_LE(area.x() + area.w(), texture_.width());
			ASSERT_LT(area.y() + y, texture_.height());
			std::vector<bool>::const_iterator src = texture_.get_alpha_row(area.x(), area.y() + y);

			std::copy(src, src + area.w(), dst);
			
			dst_index += img_rect_.w()*nframes_;
		}
	}

	if(force_no_alpha_) {
		const int nsize = alpha_.size();
		alpha_.clear();
		alpha_.resize(nsize, false);
		return;
	}
}

void frame::build_alpha()
{
	frames_.resize(nframes_);
	if(!texture_.valid()) {
		return;
	}

	alpha_.resize(nframes_*img_rect_.w()*img_rect_.h(), true);

	for(int n = 0; n < nframes_; ++n) {
		const int current_col = (nframes_per_row_ > 0) ? (n% nframes_per_row_) : n;
		const int current_row = (nframes_per_row_ > 0) ? (n/nframes_per_row_) : 0;
		const int xbase = img_rect_.x() + current_col*(img_rect_.w()+pad_);
		const int ybase = img_rect_.y() + current_row*(img_rect_.h()+pad_);

		if(xbase < 0 || ybase < 0 || xbase + img_rect_.w() > texture_.width() ||
		   ybase + img_rect_.h() > texture_.height()) {
			std::cerr << "IMAGE RECT FOR FRAME '" << id_ << "' #" << n << ": " << img_rect_.x() << " + " << current_col << " * (" << img_rect_.w() << "+" << pad_ << ") IS INVALID: " << xbase << ", " << ybase << ", " << (xbase + img_rect_.w()) << ", " << (ybase + img_rect_.h()) << " / " << texture_.width() << "," << texture_.height() << "\n";
			throw error();
		}

		for(int y = 0; y != img_rect_.h(); ++y) {
			const int dst_index = y*img_rect_.w()*nframes_ + n*img_rect_.w();
			ASSERT_INDEX_INTO_VECTOR(dst_index, alpha_);

			std::vector<bool>::iterator dst = alpha_.begin() + dst_index;

			std::vector<bool>::const_iterator src = texture_.get_alpha_row(xbase, ybase + y);
			std::copy(src, src + img_rect_.w(), dst);
		}

		//now calculate if the actual frame we should be using for drawing
		//is smaller than the outer rectangle, so we can save on drawing space
		frame_info& f = frames_[n];
		f.area = rect(xbase, ybase, img_rect_.w(), img_rect_.h());

		if(no_remove_alpha_borders_) {
			continue;
		}
		
		int top;
		for(top = 0; top != img_rect_.h(); ++top) {
			const std::vector<bool>::const_iterator a = texture_.get_alpha_row(xbase, ybase + top);
			if(std::find(a, a + img_rect_.w(), false) != a + img_rect_.w()) {
				break;
			}
		}

		int bot;
		for(bot = img_rect_.h(); bot > 0; --bot) {
			const std::vector<bool>::const_iterator a = texture_.get_alpha_row(xbase, ybase + bot-1);
			if(std::find(a, a + img_rect_.w(), false) != a + img_rect_.w()) {
				break;
			}
		}

		int left;
		for(left = 0; left < img_rect_.w(); ++left) {
			std::vector<bool>::const_iterator a = texture_.get_alpha_row(xbase + left, ybase);

			bool has_opaque = false;
			for(int n = 0; n != img_rect_.h(); ++n) {
				if(!*a) {
					has_opaque = true;
				}
				if(n+1 != img_rect_.h()) {
					a += texture_.width();
				}
			}

			if(has_opaque) {
				break;
			}
		}

		int right;
		for(right = img_rect_.w(); right > 0; --right) {
			std::vector<bool>::const_iterator a = texture_.get_alpha_row(xbase + right-1, ybase);

			bool has_opaque = false;
			for(int n = 0; n != img_rect_.h(); ++n) {
				if(!*a) {
					has_opaque = true;
				}

				if(n+1 != img_rect_.h()) {
					a += texture_.width();
				}
			}

			if(has_opaque) {
				break;
			}
		}

		if(right < left) {
			right = left;
		}

		if(bot < top) {
			bot = top;
		}

		f.x_adjust = left;
		f.y_adjust = top;
		f.x2_adjust = img_rect_.w() - right;
		f.y2_adjust = img_rect_.h() - bot;
		f.area = rect(xbase + left, ybase + top, right - left, bot - top);
		ASSERT_EQ(f.area.w() + f.x_adjust + f.x2_adjust, img_rect_.w());
		ASSERT_EQ(f.area.h() + f.y_adjust + f.y2_adjust, img_rect_.h());
	}

	if(force_no_alpha_) {
		const int nsize = alpha_.size();
		alpha_.clear();
		alpha_.resize(nsize, false);
		return;
	}
}

bool frame::is_alpha(int x, int y, int time, bool face_right) const
{
	std::vector<bool>::const_iterator itor = get_alpha_itor(x, y, time, face_right);
	if(itor == alpha_.end()) {
		return true;
	} else {
		return *itor;
	}
}

std::vector<bool>::const_iterator frame::get_alpha_itor(int x, int y, int time, bool face_right) const
{
	if(alpha_.empty()) {
		return alpha_.end();
	}

	if(face_right == false) {
		x = width() - x - 1;
	}

	if(x < 0 || y < 0 || x >= width() || y >= height()) {
		return alpha_.end();
	}

	x /= scale_;
	y /= scale_;

	const int nframe = frame_number(time);
	x += nframe*img_rect_.w();
	
	const int index = y*img_rect_.w()*nframes_ + x;
	ASSERT_INDEX_INTO_VECTOR(index, alpha_);
	return alpha_.begin() + index;
}

void frame::draw_into_blit_queue(graphics::blit_queue& blit, int x, int y, bool face_right, bool upside_down, int time) const
{
	const frame_info* info = NULL;
	GLfloat rect[4];
	get_rect_in_texture(time, &rect[0], info);

	x += (face_right ? info->x_adjust : info->x2_adjust)*scale_;
	y += (info->y_adjust)*scale_;
	const int w = info->area.w()*scale_*(face_right ? 1 : -1);
	const int h = info->area.h()*scale_*(upside_down ? -1 : 1);

	rect[0] = texture_.translate_coord_x(rect[0]);
	rect[1] = texture_.translate_coord_y(rect[1]);
	rect[2] = texture_.translate_coord_x(rect[2]);
	rect[3] = texture_.translate_coord_y(rect[3]);

	blit.set_texture(texture_.get_id());


	blit.add(x, y, rect[0], rect[1]);
	blit.add(x + w, y, rect[2], rect[1]);
	blit.add(x, y + h, rect[0], rect[3]);
	blit.add(x + w, y + h, rect[2], rect[3]);
}

void frame::draw(int x, int y, bool face_right, bool upside_down, int time, GLfloat rotate) const
{
	const frame_info* info = NULL;
	GLfloat rect[4];
	get_rect_in_texture(time, &rect[0], info);

	x += (face_right ? info->x_adjust : info->x2_adjust)*scale_;
	y += info->y_adjust*scale_;
	const int w = info->area.w()*scale_*(face_right ? 1 : -1);
	const int h = info->area.h()*scale_*(upside_down ? -1 : 1);

	gles2::active_shader()->shader()->set_sprite_area(rect);

	if(rotate == 0) {
		//if there is no rotation, then we can make a much simpler call
		graphics::queue_blit_texture(texture_, x, y, w, h, rect[0], rect[1], rect[2], rect[3]);
		graphics::flush_blit_texture();
		return;
	}

	graphics::queue_blit_texture(texture_, x, y, w, h, rotate, rect[0], rect[1], rect[2], rect[3]);
	graphics::flush_blit_texture();
}

void frame::draw(int x, int y, bool face_right, bool upside_down, int time, GLfloat rotate, GLfloat scale) const
{
	const frame_info* info = NULL;
	GLfloat rect[4];
	get_rect_in_texture(time, &rect[0], info);

	x += (face_right ? info->x_adjust : info->x2_adjust)*scale_;
	y += info->y_adjust*scale_;
	const int w = info->area.w()*scale_*scale*(face_right ? 1 : -1);
	const int h = info->area.h()*scale_*scale*(upside_down ? -1 : 1);

	//adjust x,y to accomodate scaling so that we scale from the center.
	const int width_delta = img_rect_.w()*scale_*scale - img_rect_.w()*scale_;
	const int height_delta = img_rect_.h()*scale_*scale - img_rect_.h()*scale_;
	x -= width_delta/2;
	y -= height_delta/2;

	gles2::active_shader()->shader()->set_sprite_area(rect);

	if(rotate == 0) {
		//if there is no rotation, then we can make a much simpler call
		graphics::queue_blit_texture(texture_, x, y, w, h, rect[0], rect[1], rect[2], rect[3]);
		graphics::flush_blit_texture();
		return;
	}

	graphics::queue_blit_texture(texture_, x, y, w, h, rotate, rect[0], rect[1], rect[2], rect[3]);
	graphics::flush_blit_texture();
}

void frame::draw(int x, int y, const rect& area, bool face_right, bool upside_down, int time, GLfloat rotate) const
{
	const frame_info* info = NULL;
	GLfloat rect[4];
	get_rect_in_texture(time, &rect[0], info);

	const int x_adjust = area.x();
	const int y_adjust = area.y();
	const int w_adjust = area.w() - img_rect_.w();
	const int h_adjust = area.h() - img_rect_.h();

	const int w = info->area.w()*scale_*(face_right ? 1 : -1);
	const int h = info->area.h()*scale_*(upside_down ? -1 : 1);

	rect[0] += GLfloat(x_adjust)/GLfloat(texture_.width());
	rect[1] += GLfloat(y_adjust)/GLfloat(texture_.height());
	rect[2] += GLfloat(x_adjust + w_adjust)/GLfloat(texture_.width());
	rect[3] += GLfloat(y_adjust + h_adjust)/GLfloat(texture_.height());

	//the last 4 params are the rectangle of the single, specific frame
	graphics::blit_texture(texture_, x, y, (w + w_adjust*scale_)*(face_right ? 1 : -1), (h + h_adjust*scale_)*(upside_down ? -1 : 1), rotate + (face_right ? rotate_ : -rotate_),
	                       rect[0], rect[1], rect[2], rect[3]);
}

#if defined(USE_ISOMAP)
void frame::draw3(int time, GLint va, GLint tc) const
{
	const int nframe = frame_number(time);

	glEnable(GL_DEPTH_TEST);
	if(back_face_culling_) {
		glEnable(GL_CULL_FACE);
	}
	glEnableVertexAttribArray(va);
	glEnableVertexAttribArray(tc);

	for(auto& dd3d : dd3d_array_) {
		size_t tex_unit = GL_TEXTURE0;
		if(dd3d.tex_a.valid()) {
			glActiveTexture(tex_unit);
			dd3d.tex_a.set_as_current_texture();
			++tex_unit;
		}
		if(dd3d.tex_d.valid()) {
			glActiveTexture(tex_unit);
			if(tex_unit == GL_TEXTURE0) {
				dd3d.tex_d.set_as_current_texture();
			} else {
				glBindTexture(GL_TEXTURE_2D, dd3d.tex_d.get_id());
			}
			++tex_unit;
		}
		if(dd3d.tex_s.valid()) {
			glActiveTexture(tex_unit);
			if(tex_unit == GL_TEXTURE0) {
				dd3d.tex_s.set_as_current_texture();
			} else {
				glBindTexture(GL_TEXTURE_2D, dd3d.tex_s.get_id());
			}
			++tex_unit;
		}

		glBindBuffer(GL_ARRAY_BUFFER, vbo_array_[dd3d.vbo_cnt]);
		glVertexAttribPointer(va, 
			dd3d.num_vertices,  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,					// stride
			0					// array buffer offset
		);
	
		glVertexAttribPointer(tc, 
			2,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,					// stride
			reinterpret_cast<const GLfloat*>(dd3d.texture_offset + sizeof(GLfloat)*nframe*12)	// array buffer offset
		);
		glDrawArrays(GL_TRIANGLES, 0, dd3d.vertex_count);
	}
	glDisableVertexAttribArray(va);
	glDisableVertexAttribArray(tc);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if(back_face_culling_) {
		glDisable(GL_CULL_FACE);
	}
	glDisable(GL_DEPTH_TEST);
}
#endif

void frame::draw_custom(int x, int y, const std::vector<CustomPoint>& points, const rect* area, bool face_right, bool upside_down, int time, GLfloat rotate) const
{
	texture_.set_as_current_texture();

	const frame_info* info = NULL;
	GLfloat rect[4];
	get_rect_in_texture(time, &rect[0], info);
	rect[0] = texture_.translate_coord_x(rect[0]);
	rect[1] = texture_.translate_coord_y(rect[1]);
	rect[2] = texture_.translate_coord_x(rect[2]);
	rect[3] = texture_.translate_coord_y(rect[3]);

	x += (face_right ? info->x_adjust : info->x2_adjust)*scale_;
	y += info->y_adjust*scale_;
	int w = info->area.w()*scale_*(face_right ? 1 : -1);
	int h = info->area.h()*scale_*(upside_down ? -1 : 1);

	if(w < 0) {
		std::swap(rect[0], rect[2]);
		w *= -1;
	}

	if(h < 0) {
		std::swap(rect[1], rect[3]);
		h *= -1;
	}

	if(area != NULL) {
		const int x_adjust = area->x();
		const int y_adjust = area->y();
		const int w_adjust = area->w() - img_rect_.w();
		const int h_adjust = area->h() - img_rect_.h();

		rect[0] += GLfloat(x_adjust)/GLfloat(texture_.width());
		rect[1] += GLfloat(y_adjust)/GLfloat(texture_.height());
		rect[2] += GLfloat(x_adjust + w_adjust)/GLfloat(texture_.width());
		rect[3] += GLfloat(y_adjust + h_adjust)/GLfloat(texture_.height());

		w += w_adjust*scale_;
		h += h_adjust*scale_;
	}

	std::vector<GLfloat> tcqueue;
	std::vector<GLshort> vqueue;

	const GLfloat center_x = x + GLfloat(w)/2.0;
	const GLfloat center_y = y + GLfloat(h)/2.0;

	glPushMatrix();
	glTranslatef(center_x, center_y, 0.0);
	glRotatef(rotate,0.0,0.0,1.0);

	foreach(const CustomPoint& p, points) {
		GLfloat pos = p.pos;

		if(pos > 4.0) {
			pos = 4.0;
		}

		int side = static_cast<int>(pos);
		GLfloat f = pos - static_cast<GLfloat>(side);
		if(side >= 4) {
			side = 0;
		}

		GLshort xpos, ypos;
		GLfloat u, v;
		switch(side) {
		case 0:
			u = rect[0] + (rect[2] - rect[0])*f;
			v = rect[1];
			xpos = GLfloat(x) + GLfloat(w)*f;
			ypos = y;
			break;
		case 2:
			u = rect[2] - (rect[2] - rect[0])*f;
			v = rect[3];
			xpos = GLfloat(x + w) - GLfloat(w)*f;
			ypos = y + h;
			break;
		case 1:
			u = rect[2];
			v = rect[1] + (rect[3] - rect[1])*f;
			xpos = x + w;
			ypos = GLfloat(y) + GLfloat(h)*f;
			break;
		case 3:
			u = rect[0];
			v = rect[3] - (rect[3] - rect[1])*f;
			xpos = x;
			ypos = GLfloat(y + h) - GLfloat(h)*f;
			break;
		default:
			ASSERT_LOG(false, "ILLEGAL CUSTOM FRAME POSITION: " << side);
			break;
		}

		xpos += p.offset.x;
		ypos += p.offset.y;

		vqueue.push_back(xpos - center_x);
		vqueue.push_back(ypos - center_y);

		tcqueue.push_back(u);
		tcqueue.push_back(v);
	}

	ASSERT_LOG(vqueue.size() > 4, "ILLEGAL CUSTOM BLIT: " << vqueue.size());
#if defined(USE_SHADERS)
	{
		gles2::active_shader()->prepare_draw();
		gles2::active_shader()->shader()->vertex_array(2, GL_SHORT, 0, 0, &vqueue.front());
		gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, GL_FALSE, 0, &tcqueue.front());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, tcqueue.size()/2);
	}
#else
	glVertexPointer(2, GL_SHORT, 0, &vqueue.front());
	glTexCoordPointer(2, GL_FLOAT, 0, &tcqueue.front());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, tcqueue.size()/2);
#endif
	glPopMatrix();
}

PREF_BOOL(debug_custom_draw, false, "Show debug visualization of custom drawing");

void frame::draw_custom(int x, int y, const GLfloat* xy, const GLfloat* uv, int nelements, bool face_right, bool upside_down, int time, GLfloat rotate, int cycle) const
{
	texture_.set_as_current_texture();

	const frame_info* info = NULL;
	GLfloat rect[4];
	get_rect_in_texture(time, &rect[0], info);
	rect[0] = texture_.translate_coord_x(rect[0]);
	rect[1] = texture_.translate_coord_y(rect[1]);
	rect[2] = texture_.translate_coord_x(rect[2]);
	rect[3] = texture_.translate_coord_y(rect[3]);

	x += (face_right ? info->x_adjust : info->x2_adjust)*scale_;
	y += info->y_adjust*scale_;

	int w = info->area.w()*scale_*(face_right ? 1 : -1);
	int h = info->area.h()*scale_*(upside_down ? -1 : 1);

	if(w < 0) {
		std::swap(rect[0], rect[2]);
		w *= -1;
	}

	if(h < 0) {
		std::swap(rect[1], rect[3]);
		h *= -1;
	}
	

	std::vector<GLfloat> tcqueue;
	std::vector<GLshort> vqueue;

	const GLfloat center_x = x + GLfloat(w)/2.0;
	const GLfloat center_y = y + GLfloat(h)/2.0;

	glPushMatrix();
//	glTranslatef(center_x, center_y, 0.0);
//	glRotatef(rotate,0.0,0.0,1.0);

	for(int n = 0; n < nelements; ++n) {
		vqueue.push_back(x + w*xy[0]);
		vqueue.push_back(y + h*xy[1]);

		tcqueue.push_back(rect[0] + (rect[2]-rect[0])*uv[0]);
		tcqueue.push_back(rect[1] + (rect[3]-rect[1])*uv[1]);

		xy += 2;
		uv += 2;
	}

#if defined(USE_SHADERS)
	{
		GLfloat draw_area[] = {GLfloat(x), GLfloat(y), GLfloat(x+w), GLfloat(y+h)};
		if(face_right) {
			std::swap(draw_area[0], draw_area[2]);
		}
		gles2::active_shader()->prepare_draw();
		gles2::active_shader()->shader()->set_sprite_area(rect);
		gles2::active_shader()->shader()->set_draw_area(draw_area);
		gles2::active_shader()->shader()->set_cycle(cycle);
		gles2::active_shader()->shader()->vertex_array(2, GL_SHORT, 0, 0, &vqueue.front());
		gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, GL_FALSE, 0, &tcqueue.front());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, tcqueue.size()/2);
	}

	if(g_debug_custom_draw) {
		static graphics::texture tex = graphics::texture::get("white2x2.png");
		tex.set_as_current_texture();

		glColor4f(1.0,1.0,1.0,1.0);
		gles2::active_shader()->prepare_draw();
		gles2::active_shader()->shader()->vertex_array(2, GL_SHORT, 0, 0, &vqueue.front());
		gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, GL_FALSE, 0, &tcqueue.front());
		glDrawArrays(GL_LINE_STRIP, 0, vqueue.size()/2);
	}
#else
	glVertexPointer(2, GL_SHORT, 0, &vqueue.front());
	glTexCoordPointer(2, GL_FLOAT, 0, &tcqueue.front());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, tcqueue.size()/2);
#endif

	glPopMatrix();
}

void frame::get_rect_in_texture(int time, GLfloat* output_rect, const frame_info*& info) const
{
	//picks out a single frame to draw from a whole animation, based on time
	get_rect_in_frame_number(frame_number(time), output_rect, info);
}

void frame::get_rect_in_frame_number(int nframe, GLfloat* output_rect, const frame_info*& info_result) const
{
	const frame_info& info = frames_[nframe];
	info_result = &info;

	if(info.draw_rect_init) {
		memcpy(output_rect, info.draw_rect, sizeof(*output_rect)*4);
		return;
	}

	const int current_col = (nframes_per_row_ > 0) ? (nframe % nframes_per_row_) : nframe ;
	const int current_row = (nframes_per_row_ > 0) ? (nframe/nframes_per_row_) : 0 ;

	//a tiny amount we subtract from the right/bottom side of the texture,
	//to avoid rounding errors in floating point going over the edge.
	//This seems like a kludge but I don't know of a better way to do it. :(
	const GLfloat TextureEpsilon = 0.1;

	output_rect[0] = GLfloat(info.area.x() + TextureEpsilon)/GLfloat(texture_.width());
	output_rect[1] = GLfloat(info.area.y() + TextureEpsilon) / GLfloat(texture_.height());
	output_rect[2] = GLfloat(info.area.x() + info.area.w() - TextureEpsilon)/GLfloat(texture_.width());
	output_rect[3] = GLfloat(info.area.y() + info.area.h() - TextureEpsilon)/GLfloat(texture_.height());

	memcpy(info.draw_rect, output_rect, sizeof(*output_rect)*4);
	info.draw_rect_init = true;
}

int frame::duration() const
{
	return (nframes_ + (reverse_frame_ ? nframes_ : 0))*frame_time_;
}

bool frame::hit(int time_in_frame) const
{
	if(hit_frames_.empty()) {
		return false;
	}

	return std::find(hit_frames_.begin(), hit_frames_.end(), frame_number(time_in_frame)) != hit_frames_.end();
}

int frame::frame_number(int time) const
{
	if(play_backwards_){
		int frame_num = nframes_-1;
		if(frame_time_ > 0 && nframes_ >= 1) {
			if(time >= duration()) {
				if(reverse_frame_){
					frame_num = nframes_-1;
				}else{	
					frame_num = 0;
				}
			} else {
				frame_num = nframes_-1 - time/frame_time_;
			}
			
			//if we are in reverse now
			if(frame_num < 0) {
				frame_num = -frame_num - 1;
			}
		}
		
		return frame_num;
	} else {
		int frame_num = 0;
		if(frame_time_ > 0 && nframes_ >= 1) {
			if(time >= duration()) {
				frame_num = nframes_-1;
			} else {
				frame_num = time/frame_time_;
			}
			
			//if we are in reverse now
			if(frame_num >= nframes_) {
				frame_num = nframes_ - 1 - (frame_num - nframes_);
			}
		}
		
		return frame_num;
	}
}

const std::string* frame::get_event(int time_in_frame) const
{
	if(event_frames_.empty()) {
		return NULL;
	}

	std::vector<int>::const_iterator i = std::find(event_frames_.begin(), event_frames_.end(), time_in_frame);
	if(i == event_frames_.end()) {
		return NULL;
	}

	return &event_names_[i - event_frames_.begin()];
}

point frame::pivot(const std::string& name, int time_in_frame) const
{
	if(time_in_frame < 0) {
		return point(feet_x(),feet_y());
	}

	foreach(const pivot_schedule& s, pivots_) {
		if(s.name != name) {
			continue;
		}

		if(time_in_frame >= s.points.size()) {
			return s.points.back();
		}

		return s.points[time_in_frame];
	}

	return point(feet_x(),feet_y()); //default is to pivot around feet.
}

variant frame::get_value(const std::string& key) const
{
	return variant();
}
