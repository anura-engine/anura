/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#ifndef FRAME_HPP_INCLUDED
#define FRAME_HPP_INCLUDED

#include <boost/array.hpp>

#include <string>
#include <vector>

#include "formula.hpp"
#include "geometry.hpp"
#include "obj_reader.hpp"
#include "solid_map_fwd.hpp"
#include "raster.hpp"
#include "variant.hpp"
#include <glm/glm.hpp>

namespace graphics {
class blit_queue;
}

class frame : public game_logic::formula_callable
{
public:
	//exception thrown when there's a loading error.
	struct error {};

	struct collision_area {
		std::string name;
		rect area;

		//if this flag is set, then the entire area is considered to
		//collide, rather than just the pixels that have non-zero alpha.
		bool no_alpha_check;
	};

	static void build_patterns(variant obj_variant);

	static void set_color_palette(unsigned int palettes);

	explicit frame(variant node);
	~frame();

	//ID of the frame. Not unique, but is the name of the element the frame
	//came from. Useful to tell what kind of frame it is.
	const std::string& id() const { return id_; }
	const variant& variant_id() const { return variant_id_; }
	const std::string& image_name() const { return image_; }

	//play a sound. 'object' is just the address of the object playing the
	//sound, useful if the sound is later cancelled.
	void play_sound(const void* object=NULL) const;
	bool is_alpha(int x, int y, int time, bool face_right) const;

	//Low level interface to alpha information.
	std::vector<bool>::const_iterator get_alpha_itor(int x, int y, int time, bool face_right) const;
	const std::vector<bool>& get_alpha_buf() const { return alpha_; }

	void draw_into_blit_queue(graphics::blit_queue& blit, int x, int y, bool face_right=true, bool upside_down=false, int time=0) const;
	void draw(int x, int y, bool face_right=true, bool upside_down=false, int time=0, GLfloat rotate=0) const;
	void draw(int x, int y, bool face_right, bool upside_down, int time, GLfloat rotate, GLfloat scale) const;
	void draw(int x, int y, const rect& area, bool face_right=true, bool upside_down=false, int time=0, GLfloat rotate=0) const;
	void draw3(int time, GLint va, GLint tc) const;

	struct CustomPoint {
		CustomPoint() : pos(0) {}
		GLfloat pos;
		point offset;
	};

	void draw_custom(int x, int y, const std::vector<CustomPoint>& points, const rect* area, bool face_right, bool upside_down, int time, GLfloat rotate) const;

	void draw_custom(int x, int y, const GLfloat* xy, const GLfloat* uv, int nelements, bool face_right, bool upside_down, int time, GLfloat rotate, int cycle) const;
	void set_image_as_solid();
	const_solid_info_ptr solid() const { return solid_; }
	int collide_x() const { return collide_rect_.x()*scale_; }
	int collide_y() const { return collide_rect_.y()*scale_; }
	int collide_w() const { return collide_rect_.w()*scale_; }
	int collide_h() const { return collide_rect_.h()*scale_; }
	int hit_x() const { return hit_rect_.x()*scale_; }
	int hit_y() const { return hit_rect_.y()*scale_; }
	int hit_w() const { return hit_rect_.w()*scale_; }
	int hit_h() const { return hit_rect_.h()*scale_; }
	int platform_x() const { return platform_rect_.x()*scale_; }
	int platform_y() const { return platform_rect_.y()*scale_; }
	int platform_w() const { return platform_rect_.w()*scale_; }
	bool has_platform() const { return platform_rect_.w() > 0; }
	int feet_x() const { return feet_x_*scale_; }
	int feet_y() const { return feet_y_*scale_; }
	int accel_x() const { return accel_x_; }
	int accel_y() const { return accel_y_; }
	int velocity_x() const { return velocity_x_; }
	int velocity_y() const { return velocity_y_; }
	int width() const { return img_rect_.w()*scale_; }
	int height() const { return img_rect_.h()*scale_; }
	int duration() const;
	bool hit(int time_in_frame) const;
	const graphics::texture& img() const { return texture_; }
	const rect& area() const { return img_rect_; }
	int num_frames() const { return nframes_; }
	int num_frames_per_row() const { return nframes_per_row_ > 0 && nframes_per_row_ < nframes_ ? nframes_per_row_ : nframes_; }
	int pad() const { return pad_; }
	int blur() const { return blur_; }
	bool rotate_on_slope() const { return rotate_on_slope_; }
	int damage() const { return damage_; }

	const std::string* get_event(int time_in_frame) const;

	const std::vector<collision_area>& collision_areas() const { return collision_areas_; }
	bool collision_areas_inside_frame() const { return collision_areas_inside_frame_; }

	int enter_event_id() const { return enter_event_id_; }
	int end_event_id() const { return end_event_id_; }
	int leave_event_id() const { return leave_event_id_; }
	int process_event_id() const { return process_event_id_; }

	struct frame_info {
		frame_info() : x_adjust(0), y_adjust(0), x2_adjust(0), y2_adjust(0), draw_rect_init(false)
		{}
		int x_adjust, y_adjust, x2_adjust, y2_adjust;
		rect area;

		mutable bool draw_rect_init;
		mutable GLfloat draw_rect[4];
	};

	const std::vector<frame_info>& frame_layout() const { return frames_; }

	point pivot(const std::string& name, int time_in_frame) const;
	int frame_number(int time_in_frame) const;
private:

	void get_rect_in_texture(int time, GLfloat* output_rect, const frame_info*& info) const;
	void get_rect_in_frame_number(int nframe, GLfloat* output_rect, const frame_info*& info) const;
	std::string id_, image_;

	//ID as a variant, useful to be able to get a variant of the ID
	//very efficiently.
	variant variant_id_;

	//ID's used to signal events that occur on this animation.
	int enter_event_id_, end_event_id_, leave_event_id_, process_event_id_;
	graphics::texture texture_;
	const_solid_info_ptr solid_;
	rect collide_rect_;
	rect hit_rect_;
	rect img_rect_;

	std::vector<frame_info> frames_;

	rect platform_rect_;
	std::vector<int> hit_frames_;
	int platform_x_, platform_y_, platform_w_;
	int feet_x_, feet_y_;
	int accel_x_, accel_y_;
	int velocity_x_, velocity_y_;
	int nframes_;
	int nframes_per_row_;
	int frame_time_;
	bool reverse_frame_;
	bool play_backwards_;
	float scale_;
	int pad_;
	int rotate_;
	int blur_;
	bool rotate_on_slope_;
	int damage_;

	std::vector<int> event_frames_;
	std::vector<std::string> event_names_;
	std::vector <std::string> sounds_;

	std::vector<collision_area> collision_areas_;
	bool collision_areas_inside_frame_;

	void build_alpha_from_frame_info();
	void build_alpha();
	std::vector<bool> alpha_;
	bool force_no_alpha_;

	bool no_remove_alpha_borders_;

	std::vector<int> palettes_recognized_;
	int current_palette_;

	struct pivot_schedule {
		std::string name;
		std::vector<point> points;
	};

	std::vector<pivot_schedule> pivots_;

	void set_palettes(unsigned int palettes);

	variant get_value(const std::string& key) const;

	bool back_face_culling_;
	struct draw_data_3d
	{
		size_t num_vertices;
		size_t vertex_count;
		size_t vertex_offset;
		size_t texture_offset;
		size_t normal_offset;
		size_t vbo_cnt;
		obj::mtl_data mtl;
		graphics::texture tex_a;
		graphics::texture tex_d;
		graphics::texture tex_s;
	};
	std::vector<draw_data_3d> dd3d_array_;
	graphics::vbo_array vbo_array_;
};

#endif
