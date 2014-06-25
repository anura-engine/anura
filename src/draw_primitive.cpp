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

#if defined(USE_SHADERS)
#include <assert.h>
#include <math.h>
#include <glm/gtc/type_precision.hpp>

#include <boost/array.hpp>

#include <vector>

#include "graphics.hpp"
#include "asserts.hpp"
#include "color_utils.hpp"
#include "draw_primitive.hpp"
#include "foreach.hpp"
#include "geometry.hpp"
#include "gles2.hpp"
#include "level.hpp"
#include "raster.hpp"
#include "shaders.hpp"
#include "texture.hpp"

namespace graphics
{

using namespace gles2;

namespace
{

class rect_primitive : public draw_primitive
{
public:
	explicit rect_primitive(const variant& v);
private:
	void init();

	void handle_draw() const;
#ifdef USE_ISOMAP
	void handle_draw(const lighting_ptr& lighting, const camera_callable_ptr& camera) const;
#endif

	variant get_value(const std::string& key) const;
	void set_value(const std::string& key, const variant& value);

	rect area_;
	graphics::color color_;
	shader_program_ptr shader_;
	mutable std::vector<GLfloat> varray_;
};

rect_primitive::rect_primitive(const variant& v)
	: draw_primitive(v), area_(v["area"]), color_(v["color"]), 
     shader_(gles2::get_simple_shader())
{
	if(v.has_key("shader")) {
		shader_.reset(new shader_program(v["shader"].as_string()));
	}

	init();
}

void rect_primitive::init()
{
	varray_.clear();
	varray_.push_back(area_.x());
	varray_.push_back(area_.y());
	varray_.push_back(area_.x2());
	varray_.push_back(area_.y());
	varray_.push_back(area_.x());
	varray_.push_back(area_.y2());
	varray_.push_back(area_.x2());
	varray_.push_back(area_.y2());
}

void rect_primitive::handle_draw() const
{
	color_.set_as_current_color();

	gles2::manager gles2_manager(shader_);
	gles2::active_shader()->prepare_draw();
	gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray_.front());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, varray_.size()/2);

	glColor4f(1.0, 1.0, 1.0, 1.0);
}

#ifdef USE_ISOMAP
void rect_primitive::handle_draw(const lighting_ptr& lighting, const camera_callable_ptr& camera) const
{
}
#endif

variant rect_primitive::get_value(const std::string& key) const
{
	return draw_primitive::get_value(key);
}

void rect_primitive::set_value(const std::string& key, const variant& value)
{
	draw_primitive::set_value(key, value);
}

typedef boost::array<GLfloat, 2> FPoint;

class circle_primitive : public draw_primitive
{
public:
	explicit circle_primitive(const variant& v);

private:
	void init();

	void handle_draw() const;
#ifdef USE_ISOMAP
	void handle_draw(const lighting_ptr& lighting, const camera_callable_ptr& camera) const;
#endif
	variant get_value(const std::string& key) const;
	void set_value(const std::string& key, const variant& value);

	FPoint center_;
	float radius_;
	float y_radius_;
	float stroke_width_;

	graphics::color color_;
	graphics::color stroke_color_;

	shader_program_ptr shader_;

	mutable std::vector<GLfloat> varray_;
};

circle_primitive::circle_primitive(const variant& v)
   : draw_primitive(v),
     radius_(v["radius"].as_decimal().as_float()),
     y_radius_(v["y_radius"].as_decimal(decimal(radius_)).as_float()),
	 stroke_width_(0.0),
     shader_(gles2::get_simple_shader())
{
	if(v.has_key("shader")) {
		shader_.reset(new shader_program(v["shader"].as_string()));
	}

	center_[0] = v["x"].as_decimal().as_float();
	center_[1] = v["y"].as_decimal().as_float();

	if(v.has_key("color")) {
		color_ = color(v["color"]);
	} else {
		color_ = color(200, 0, 0, 255);
	}

	if(v.has_key("stroke_color")) {
		stroke_color_ = color(v["stroke_color"]);
		stroke_width_ = v["stroke_width"].as_decimal().as_float();
	}


	init();
}

void circle_primitive::init()
{
	varray_.clear();
	varray_.push_back(center_[0]);
	varray_.push_back(center_[1]);
	for(double angle = 0; angle < 3.1459*2.0; angle += 0.1) {
		const double xpos = center_[0] + radius_*cos(angle);
		const double ypos = center_[1] + y_radius_*sin(angle);
		varray_.push_back(xpos);
		varray_.push_back(ypos);
	}

	//repeat the first coordinate to complete the circle.
	varray_.push_back(varray_[2]);
	varray_.push_back(varray_[3]);

}

#ifdef USE_ISOMAP
void circle_primitive::handle_draw(const lighting_ptr& lighting, const camera_callable_ptr& camera) const
{
}
#endif

void circle_primitive::handle_draw() const
{
	gles2::manager gles2_manager(shader_);

	if(color_.a() > 0) {
		color_.set_as_current_color();

		gles2::active_shader()->prepare_draw();
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray_.front());
		glDrawArrays(GL_TRIANGLE_FAN, 0, varray_.size()/2);
	}

	if(stroke_color_.a() > 0) {
		glLineWidth(stroke_width_);
		stroke_color_.set_as_current_color();

		gles2::active_shader()->prepare_draw();
        gles2::active_shader()->shader()->disable_vertex_attrib(-1);
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray_[2]);
        glDrawArrays(GL_LINE_LOOP, 0, (varray_.size()-2)/2);
	}

	glColor4f(1.0, 1.0, 1.0, 1.0);
	
}

variant circle_primitive::get_value(const std::string& key) const
{
	return variant();
}

void circle_primitive::set_value(const std::string& key, const variant& value)
{
}

class arrow_primitive : public draw_primitive
{
public:
	explicit arrow_primitive(const variant& v);

private:

	void handle_draw() const;
#ifdef USE_ISOMAP
	void handle_draw(const lighting_ptr& lighting, const camera_callable_ptr& camera) const;
#endif

	variant get_value(const std::string& key) const;
	void set_value(const std::string& key, const variant& value);

	void set_points(const variant& points);

	void curve(const FPoint& p1, const FPoint& p2, const FPoint& p3, std::vector<FPoint>* out) const;

	std::vector<FPoint> points_;
	GLfloat granularity_;
	int arrow_head_length_;
	GLfloat arrow_head_width_;
	graphics::color color_;
	int fade_in_length_;

	GLfloat width_base_, width_head_;

	mutable std::vector<GLfloat> uvarray_;
	mutable std::vector<GLfloat> varray_;
	mutable std::vector<unsigned char> carray_;

	texture texture_;
	GLfloat texture_scale_;

	void calculate_draw_arrays() const;
};

arrow_primitive::arrow_primitive(const variant& v)
  : draw_primitive(v),
    granularity_(v["granularity"].as_decimal(decimal(0.005)).as_float()),
    arrow_head_length_(v["arrow_head_length"].as_int(10)),
    arrow_head_width_(v["arrow_head_width"].as_decimal(decimal(2.0)).as_float()),
	fade_in_length_(v["fade_in_length"].as_int(50)),
	width_base_(v["width_base"].as_decimal(decimal(12.0)).as_float()),
	width_head_(v["width_head"].as_decimal(decimal(5.0)).as_float())
{
	if(v.has_key("texture")) {
		texture_ = texture::get(v["texture"].as_string());
		texture_scale_ = v["texture_scale"].as_decimal(decimal(1.0)).as_float();
	}

	if(v.has_key("color")) {
		color_ = color(v["color"]);
	} else {
		color_ = color(200, 0, 0, 255);
	}

	set_points(v["points"]);
}

void arrow_primitive::calculate_draw_arrays() const
{
	if(!varray_.empty()) {
		return;
	}

	std::vector<FPoint> path;

	for(int n = 1; n < points_.size()-1; ++n) {
		std::vector<FPoint> new_path;
		curve(points_[n-1], points_[n], points_[n+1], &new_path);

		if(path.empty()) {
			path.swap(new_path);
		} else {
			assert(path.size() >= new_path.size());
			const int overlap = path.size()/2;
			for(int n = 0; n != overlap; ++n) {
				const float ratio = float(n)/float(overlap);
				FPoint& value = path[(path.size() - overlap) + n];
				FPoint new_value = new_path[n];
				value[0] = value[0]*(1.0-ratio) + new_value[0]*ratio;
				value[1] = value[1]*(1.0-ratio) + new_value[1]*ratio;
			}

			path.insert(path.end(), new_path.begin() + overlap, new_path.end());
		}
	}

	const GLfloat PathLength = path.size()-1;

	std::vector<FPoint> left_path, right_path;
	for(int n = 0; n < path.size()-1; ++n) {
		const FPoint& p = path[n];
		const FPoint& next = path[n+1];

		FPoint direction;
		for(int m = 0; m != 2; ++m) {
			direction[m] = next[m] - p[m];
		}

		const GLfloat vector_length = sqrt(direction[0]*direction[0] + direction[1]*direction[1]);
		if(vector_length == 0.0) {
			continue;
		}

		FPoint unit_direction;
		for(int m = 0; m != 2; ++m) {
			unit_direction[m] = direction[m]/vector_length;
		}
		
		FPoint normal_direction_left, normal_direction_right;
		normal_direction_left[0] = -unit_direction[1];
		normal_direction_left[1] = unit_direction[0];
		normal_direction_right[0] = unit_direction[1];
		normal_direction_right[1] = -unit_direction[0];

		const GLfloat ratio = n/PathLength;

		GLfloat arrow_width = width_base_ - (width_base_-width_head_)*ratio;

		const int time_until_end = path.size()-2 - n;
		if(time_until_end < arrow_head_length_) {
			arrow_width = arrow_head_width_*time_until_end;
		}

		FPoint left, right;
		for(int m = 0; m != 2; ++m) {
			left[m] = p[m] + normal_direction_left[m]*arrow_width;
			right[m] = p[m] + normal_direction_right[m]*arrow_width;
		}

		left_path.push_back(left);
		right_path.push_back(right);
	}

	for(int n = 0; n != left_path.size(); ++n) {
		varray_.push_back(left_path[n][0]);
		varray_.push_back(left_path[n][1]);
		varray_.push_back(right_path[n][0]);
		varray_.push_back(right_path[n][1]);

		uvarray_.push_back(n*texture_scale_);
		uvarray_.push_back(0.0);
		uvarray_.push_back(n*texture_scale_);
		uvarray_.push_back(1.0);

		for(int m = 0; m != 2; ++m) {
			carray_.push_back(color_.r());
			carray_.push_back(color_.g());
			carray_.push_back(color_.b());
			if(n < fade_in_length_) {
				carray_.push_back(int((GLfloat(color_.a())*GLfloat(n)*(255.0/GLfloat(fade_in_length_)))/255.0));
			} else {
				carray_.push_back(color_.a());
			}
		}
	}
}

#ifdef USE_ISOMAP
void arrow_primitive::handle_draw(const lighting_ptr& lighting, const camera_callable_ptr& camera) const
{
}
#endif

void arrow_primitive::handle_draw() const
{
	if(points_.size() < 3) {
		return;
	}

	calculate_draw_arrays();

	gles2::manager gles2_manager(texture_.valid() ? gles2::get_texcol_shader() : gles2::get_simple_col_shader());

	if(texture_.valid()) {
		glActiveTexture(GL_TEXTURE0);
		texture_.set_as_current_texture();
		gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, GL_FALSE, 0, &uvarray_[0]);
	}

	gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, GL_FALSE, 0, &varray_[0]);
	gles2::active_shader()->shader()->color_array(4, GL_UNSIGNED_BYTE, GL_TRUE, 0, &carray_[0]);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, varray_.size()/2);
}

variant arrow_primitive::get_value(const std::string& key) const
{
	if(key == "points") {
		std::vector<variant> result;
		foreach(const FPoint& p, points_) {
			std::vector<variant> pos;
			pos.push_back(variant(static_cast<int>(p[0])));
			pos.push_back(variant(static_cast<int>(p[1])));
			result.push_back(variant(&pos));
		}

		return variant(&result);
	}
	ASSERT_LOG(false, "ILLEGAL KEY IN ARROW: " << key);
	return variant();
}

void arrow_primitive::set_value(const std::string& key, const variant& value)
{
	if(key == "points") {
		set_points(value);
	} else if(key == "color") {
		color_ = graphics::color(value);
	} else if(key == "granularity") {
		granularity_ = value.as_decimal().as_float();
	} else if(key == "arrow_head_length") {
		arrow_head_length_ = value.as_int();
	} else if(key == "arrow_head_width") {
		arrow_head_width_ = value.as_decimal().as_float();
	} else if(key == "fade_in_length") {
		fade_in_length_ = value.as_int();
	} else if(key == "width_base") {
		width_base_ = value.as_decimal().as_float();
	} else if(key == "width_head") {
		width_head_ = value.as_decimal().as_float();
	} else {
		ASSERT_LOG(false, "ILLEGAL KEY IN ARROW: " << key);
	}

	varray_.clear();
	carray_.clear();
}

void arrow_primitive::set_points(const variant& points)
{
	ASSERT_LOG(points.is_list(), "arrow points is not a list: " << points.debug_location());

	points_.clear();

	for(int n = 0; n != points.num_elements(); ++n) {
		variant p = points[n];
		ASSERT_LOG(p.is_list() && p.num_elements() == 2, "arrow points in invalid format: " << points.debug_location() << " : " << p.write_json());
		FPoint point;
		point[0] = p[0].as_int();
		point[1] = p[1].as_int();
		points_.push_back(point);
	}
}

void arrow_primitive::curve(const FPoint& p0, const FPoint& p1, const FPoint& p2, std::vector<FPoint>* out) const
{
	for(float t = 0.0; t < 1.0 - granularity_; t += granularity_) {
		FPoint p;
		for(int n = 0; n != 2; ++n) {
			//formula for a bezier curve.
			p[n] = (1-t)*(1-t)*p0[n] + 2*(1-t)*t*p1[n] + t*t*p2[n];
		}

		out->push_back(p);
	}
}

class wireframe_box_primitive : public draw_primitive
{
public:
	explicit wireframe_box_primitive(const variant& v);

private:
	DECLARE_CALLABLE(wireframe_box_primitive);

	void init();

	void handle_draw() const;
#ifdef USE_ISOMAP
	void handle_draw(const lighting_ptr& lighting, const camera_callable_ptr& camera) const;
#endif

	glm::vec3 b1_;
	glm::vec3 b2_;

	graphics::color color_;

	program_ptr shader_;

	std::vector<GLfloat> varray_;
	
	GLuint u_mvp_matrix_;
	GLuint a_position_;
	GLuint u_color_;

	glm::vec3 translation_;
	glm::vec3 rotation_;
	glm::vec3 scale_;
};

wireframe_box_primitive::wireframe_box_primitive(const variant& v)
	: draw_primitive(v), scale_(glm::vec3(1.0f))
{
	if(v.has_key("points")) {
		ASSERT_LOG(v["points"].is_list() && v["points"].num_elements() == 2, "'points' must be a list of two elements.");
		b1_ = variant_to_vec3(v["points"][0]);
		b2_ = variant_to_vec3(v["points"][1]);
	} else {
		ASSERT_LOG(v.has_key("point1") && v.has_key("point2"), "Must specify 'points' or 'point1' and 'point2' attributes.");
		b1_ = variant_to_vec3(v["point1"]);
		b2_ = variant_to_vec3(v["point2"]);
	}
	if(v.has_key("color")) {
		color_ = color(v["color"]);
	} else {
		color_ = color(200, 0, 0, 255);
	}
	if(v.has_key("translation")) {
		translation_ = variant_to_vec3(v["translation"]);
	}
	if(v.has_key("scale")) {
		scale_ = variant_to_vec3(v["scale"]);
	}

	if(v.has_key("shader")) {
		shader_ = shader_program::get_global(v["shader"].as_string())->shader();
	} else {
		shader_ = shader_program::get_global("line_3d")->shader();
	}
	u_mvp_matrix_ = shader_->get_fixed_uniform("mvp_matrix");
	u_color_ = shader_->get_fixed_uniform("color");
	a_position_ = shader_->get_fixed_attribute("vertex");
	ASSERT_LOG(u_mvp_matrix_ != -1, "Error getting mvp_matrix uniform");
	ASSERT_LOG(u_color_ != -1, "Error getting color uniform");
	ASSERT_LOG(a_position_ != -1, "Error getting vertex attribute");

	init();
}

void wireframe_box_primitive::init()
{
	if(b1_.x > b2_.x) {
		std::swap(b1_.x, b2_.x);
	}
	if(b1_.y > b2_.y) {
		std::swap(b1_.y, b2_.y);
	}
	if(b1_.z > b2_.z) {
		std::swap(b1_.z, b2_.z);
	}

	varray_.clear();
	varray_.push_back(b1_.x); varray_.push_back(b1_.y); varray_.push_back(b1_.z); varray_.push_back(b2_.x); varray_.push_back(b1_.y); varray_.push_back(b1_.z); 
	varray_.push_back(b1_.x); varray_.push_back(b1_.y); varray_.push_back(b1_.z); varray_.push_back(b1_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z); 
	varray_.push_back(b1_.x); varray_.push_back(b1_.y); varray_.push_back(b1_.z); varray_.push_back(b1_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z); 

	varray_.push_back(b2_.x); varray_.push_back(b2_.y); varray_.push_back(b2_.z); varray_.push_back(b2_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z); 
	varray_.push_back(b2_.x); varray_.push_back(b2_.y); varray_.push_back(b2_.z); varray_.push_back(b1_.x); varray_.push_back(b2_.y); varray_.push_back(b2_.z); 
	varray_.push_back(b2_.x); varray_.push_back(b2_.y); varray_.push_back(b2_.z); varray_.push_back(b2_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z); 

	varray_.push_back(b1_.x); varray_.push_back(b2_.y); varray_.push_back(b2_.z); varray_.push_back(b1_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z); 
	varray_.push_back(b1_.x); varray_.push_back(b2_.y); varray_.push_back(b2_.z); varray_.push_back(b1_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z); 

	varray_.push_back(b2_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z); varray_.push_back(b1_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z); 
	varray_.push_back(b2_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z); varray_.push_back(b2_.x); varray_.push_back(b1_.y); varray_.push_back(b1_.z); 

	varray_.push_back(b2_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z); varray_.push_back(b1_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z); 
	varray_.push_back(b2_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z); varray_.push_back(b2_.x); varray_.push_back(b1_.y); varray_.push_back(b1_.z); 
}

void wireframe_box_primitive::handle_draw() const
{
}

#ifdef USE_ISOMAP
void wireframe_box_primitive::handle_draw(const lighting_ptr& lighting, const camera_callable_ptr& camera) const
{
	shader_save_context save;
	glUseProgram(shader_->get());

	glm::mat4 model = glm::translate(glm::mat4(), translation_) 
		* glm::translate(glm::mat4(), glm::vec3((b2_.x - b1_.x)/2.0f,(b2_.y - b1_.y)/2.0f,(b2_.z - b1_.z)/2.0f))
		* glm::scale(glm::mat4(), scale_)
		* glm::translate(glm::mat4(), glm::vec3((b1_.x - b2_.x)/2.0f,(b1_.y - b2_.y)/2.0f,(b1_.z - b2_.z)/2.0f));
	glm::mat4 mvp = camera->projection_mat() * camera->view_mat() * model;
	glUniformMatrix4fv(u_mvp_matrix_, 1, GL_FALSE, glm::value_ptr(mvp));

	glUniform4f(u_color_, color_.r()/255.0f, color_.g()/255.0f, color_.b()/255.0f, color_.a()/255.0f);

	glEnableVertexAttribArray(a_position_);
	glVertexAttribPointer(a_position_, 3, GL_FLOAT, GL_FALSE, 0, &varray_[0]);
	glDrawArrays(GL_LINES, 0, varray_.size()/3);
	glDisableVertexAttribArray(a_position_);
}
#endif

BEGIN_DEFINE_CALLABLE(wireframe_box_primitive, draw_primitive)
	DEFINE_FIELD(color, "[int,int,int,int]")
		return obj.color_.write();
	DEFINE_SET_FIELD_TYPE("[int,int,int,int]|string")
		obj.color_ = graphics::color(value);
	DEFINE_FIELD(points, "[[decimal,decimal,decimal],[decimal,decimal,decimal]]")
		std::vector<variant> v;
		v.push_back(vec3_to_variant(obj.b1_));
		v.push_back(vec3_to_variant(obj.b2_));
		return variant(&v);
	DEFINE_SET_FIELD
		ASSERT_LOG(value.is_list() && value.num_elements() == 2, "'points' must be a list of two elements.");
		obj.b1_ = variant_to_vec3(value[0]);
		obj.b2_ = variant_to_vec3(value[1]);
		obj.init();
	DEFINE_FIELD(point1, "[decimal,decimal,decimal]")
		return vec3_to_variant(obj.b1_);
	DEFINE_SET_FIELD
		obj.b1_ = variant_to_vec3(value);
		obj.init();
	DEFINE_FIELD(point1, "[decimal,decimal,decimal]")
		return vec3_to_variant(obj.b2_);
	DEFINE_SET_FIELD
		obj.b2_ = variant_to_vec3(value);
		obj.init();
	DEFINE_FIELD(translation, "[decimal,decimal,decimal]")
		return vec3_to_variant(obj.translation_);
	DEFINE_SET_FIELD
		obj.translation_ = variant_to_vec3(value);
	DEFINE_FIELD(scale, "[decimal,decimal,decimal]")
		return vec3_to_variant(obj.scale_);
	DEFINE_SET_FIELD
		obj.scale_ = variant_to_vec3(value);
END_DEFINE_CALLABLE(wireframe_box_primitive)

}

class box_primitive : public draw_primitive
{
public:
	explicit box_primitive(const variant& v)
		: draw_primitive(v), scale_(glm::vec3(1.0f))
	{
		if(v.has_key("points")) {
			ASSERT_LOG(v["points"].is_list() && v["points"].num_elements() == 2, "'points' must be a list of two elements.");
			b1_ = variant_to_vec3(v["points"][0]);
			b2_ = variant_to_vec3(v["points"][1]);
		} else {
			ASSERT_LOG(v.has_key("point1") && v.has_key("point2"), "Must specify 'points' or 'point1' and 'point2' attributes.");
			b1_ = variant_to_vec3(v["point1"]);
			b2_ = variant_to_vec3(v["point2"]);
		}
		if(v.has_key("color")) {
			color_ = color(v["color"]);
		} else {
			color_ = color(200, 0, 0, 255);
		}
		if(v.has_key("translation")) {
			translation_ = variant_to_vec3(v["translation"]);
		}
		if(v.has_key("scale")) {
			scale_ = variant_to_vec3(v["scale"]);
		}

		if(v.has_key("shader")) {
			shader_ = shader_program::get_global(v["shader"].as_string())->shader();
		} else {
			shader_ = shader_program::get_global("line_3d")->shader();
		}
		u_mvp_matrix_ = shader_->get_fixed_uniform("mvp_matrix");
		u_color_ = shader_->get_fixed_uniform("color");
		a_position_ = shader_->get_fixed_attribute("vertex");
		ASSERT_LOG(u_mvp_matrix_ != -1, "Error getting mvp_matrix uniform");
		ASSERT_LOG(u_color_ != -1, "Error getting color uniform");
		ASSERT_LOG(a_position_ != -1, "Error getting vertex attribute");

		init();
	}
	virtual ~box_primitive()
	{}

private:
	DECLARE_CALLABLE(box_primitive);

	void init()
	{
		if(b1_.x > b2_.x) {
			std::swap(b1_.x, b2_.x);
		}
		if(b1_.y > b2_.y) {
			std::swap(b1_.y, b2_.y);
		}
		if(b1_.z > b2_.z) {
			std::swap(b1_.z, b2_.z);
		}

		varray_.clear();
		varray_.push_back(b1_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z);
		varray_.push_back(b2_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z);
		varray_.push_back(b2_.x); varray_.push_back(b2_.y); varray_.push_back(b2_.z);

		varray_.push_back(b2_.x); varray_.push_back(b2_.y); varray_.push_back(b2_.z);
		varray_.push_back(b1_.x); varray_.push_back(b2_.y); varray_.push_back(b2_.z);
		varray_.push_back(b1_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z);

		varray_.push_back(b2_.x); varray_.push_back(b2_.y); varray_.push_back(b2_.z);
		varray_.push_back(b2_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z);
		varray_.push_back(b2_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z);

		varray_.push_back(b2_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z);
		varray_.push_back(b2_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z);
		varray_.push_back(b2_.x); varray_.push_back(b1_.y); varray_.push_back(b1_.z);

		varray_.push_back(b2_.x); varray_.push_back(b2_.y); varray_.push_back(b2_.z);
		varray_.push_back(b2_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z);
		varray_.push_back(b1_.x); varray_.push_back(b2_.y); varray_.push_back(b2_.z);

		varray_.push_back(b1_.x); varray_.push_back(b2_.y); varray_.push_back(b2_.z);
		varray_.push_back(b2_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z);
		varray_.push_back(b1_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z);

		varray_.push_back(b2_.x); varray_.push_back(b1_.y); varray_.push_back(b1_.z);
		varray_.push_back(b1_.x); varray_.push_back(b1_.y); varray_.push_back(b1_.z);
		varray_.push_back(b1_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z);

		varray_.push_back(b1_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z);
		varray_.push_back(b2_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z);
		varray_.push_back(b2_.x); varray_.push_back(b1_.y); varray_.push_back(b1_.z);

		varray_.push_back(b1_.x); varray_.push_back(b2_.y); varray_.push_back(b2_.z);
		varray_.push_back(b1_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z);
		varray_.push_back(b1_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z);

		varray_.push_back(b1_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z);
		varray_.push_back(b1_.x); varray_.push_back(b2_.y); varray_.push_back(b1_.z);
		varray_.push_back(b1_.x); varray_.push_back(b1_.y); varray_.push_back(b1_.z);

		varray_.push_back(b2_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z);
		varray_.push_back(b1_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z);
		varray_.push_back(b2_.x); varray_.push_back(b1_.y); varray_.push_back(b1_.z);

		varray_.push_back(b2_.x); varray_.push_back(b1_.y); varray_.push_back(b1_.z);
		varray_.push_back(b1_.x); varray_.push_back(b1_.y); varray_.push_back(b2_.z);
		varray_.push_back(b1_.x); varray_.push_back(b1_.y); varray_.push_back(b1_.z);
	}

	void handle_draw() const
	{}

#ifdef USE_ISOMAP
	void handle_draw(const lighting_ptr& lighting, const camera_callable_ptr& camera) const
	{
		shader_save_context save;
		glUseProgram(shader_->get());

		glm::mat4 model = glm::translate(glm::mat4(), translation_) 
			* glm::translate(glm::mat4(), glm::vec3((b2_.x - b1_.x)/2.0f,(b2_.y - b1_.y)/2.0f,(b2_.z - b1_.z)/2.0f))
			* glm::scale(glm::mat4(), scale_)
			* glm::translate(glm::mat4(), glm::vec3((b1_.x - b2_.x)/2.0f,(b1_.y - b2_.y)/2.0f,(b1_.z - b2_.z)/2.0f));
		glm::mat4 mvp = camera->projection_mat() * camera->view_mat() * model;
		glUniformMatrix4fv(u_mvp_matrix_, 1, GL_FALSE, glm::value_ptr(mvp));

		glUniform4f(u_color_, color_.r()/255.0f, color_.g()/255.0f, color_.b()/255.0f, color_.a()/255.0f);

		glEnableVertexAttribArray(a_position_);
		glVertexAttribPointer(a_position_, 3, GL_FLOAT, GL_FALSE, 0, &varray_[0]);
		glDrawArrays(GL_TRIANGLES, 0, varray_.size()/3);
		glDisableVertexAttribArray(a_position_);
	}
#endif

	glm::vec3 b1_;
	glm::vec3 b2_;

	graphics::color color_;

	program_ptr shader_;

	std::vector<GLfloat> varray_;
	
	GLuint u_mvp_matrix_;
	GLuint a_position_;
	GLuint u_color_;

	glm::vec3 translation_;
	glm::vec3 rotation_;
	glm::vec3 scale_;

	box_primitive();
	box_primitive(const box_primitive&);
};

BEGIN_DEFINE_CALLABLE(box_primitive, draw_primitive)
	DEFINE_FIELD(color, "[int,int,int,int]")
		return obj.color_.write();
	DEFINE_SET_FIELD_TYPE("[int,int,int,int]|string")
		obj.color_ = graphics::color(value);
	DEFINE_FIELD(points, "[[decimal,decimal,decimal],[decimal,decimal,decimal]]")
		std::vector<variant> v;
		v.push_back(vec3_to_variant(obj.b1_));
		v.push_back(vec3_to_variant(obj.b2_));
		return variant(&v);
	DEFINE_SET_FIELD
		ASSERT_LOG(value.is_list() && value.num_elements() == 2, "'points' must be a list of two elements.");
		obj.b1_ = variant_to_vec3(value[0]);
		obj.b2_ = variant_to_vec3(value[1]);
		obj.init();
	DEFINE_FIELD(point1, "[decimal,decimal,decimal]")
		return vec3_to_variant(obj.b1_);
	DEFINE_SET_FIELD
		obj.b1_ = variant_to_vec3(value);
		obj.init();
	DEFINE_FIELD(point1, "[decimal,decimal,decimal]")
		return vec3_to_variant(obj.b2_);
	DEFINE_SET_FIELD
		obj.b2_ = variant_to_vec3(value);
		obj.init();
	DEFINE_FIELD(translation, "[decimal,decimal,decimal]")
		return vec3_to_variant(obj.translation_);
	DEFINE_SET_FIELD
		obj.translation_ = variant_to_vec3(value);
	DEFINE_FIELD(scale, "[decimal,decimal,decimal]")
		return vec3_to_variant(obj.scale_);
	DEFINE_SET_FIELD
		obj.scale_ = variant_to_vec3(value);
END_DEFINE_CALLABLE(box_primitive)


class line_primitive : public draw_primitive
{
public:
    line_primitive(const variant& node);
    ~line_primitive() {}
void handle_draw() const;
#ifdef USE_ISOMAP
void handle_draw(const lighting_ptr& lighting, const camera_callable_ptr& camera) const;
#endif
private:
    DECLARE_CALLABLE(line_primitive);
    void init();
    shader_program_ptr shader_;
    int x1_;
    int y1_;
    int x2_;
    int y2_;
    float width_;
    graphics::color color1_;
    graphics::color color2_;
    graphics::color stroke_color_;
    bool has_stroke_;
    std::vector<glm::vec2> v1array_;
    std::vector<glm::vec2> v2array_;
    std::vector<glm::u8vec4> carray_;
    line_primitive();
    line_primitive(const line_primitive&);
    line_primitive& operator=(const line_primitive&);
};

line_primitive::line_primitive(const variant& node)
    : draw_primitive(node),
      color1_(node["color1"]),
      color2_(node["color2"]),
      width_(1.0f),
      has_stroke_(false)
{
    if(node.has_key("shader")) {
        shader_.reset(new shader_program(node["shader"].as_string()));
    } else {
        shader_ = gles2::get_simple_col_shader();
    }
    if(node.has_key("p1") && node.has_key("p2")) {
        point p1(node["p1"]);
        x1_ = p1.x;
        y1_ = p1.y;
        point p2(node["p2"]);
        x2_ = p2.x;
        y2_ = p2.y;
    } else if(node.has_key("area")) {
        rect r(node["area"]);
        x1_ = r.x();
        y1_ = r.y();
        x2_ = r.x2();
        y2_ = r.y2();
    } else if(node.has_key("x1") && node.has_key("y1") && node.has_key("x2") && node.has_key("y2")) {
        x1_ = node["x1"].as_int();
        y1_ = node["y1"].as_int();
        x2_ = node["x2"].as_int();
        y2_ = node["y2"].as_int();
    } else {
        ASSERT_LOG(false, "Nothing containing points was found, either p1/p2, area or x1/y1/x2/y2 are required.");
    }
    if(node.has_key("width")) {
        width_ = static_cast<float>(node["width"].as_decimal().as_float());
    }
    if(node.has_key("stroke_color")) {
        has_stroke_ = true;
        stroke_color_ = graphics::color(node["stroke_color"]);
    }
    init();
}

void line_primitive::init()
{
    double theta = std::atan2(static_cast<double>(y2_-y1_),static_cast<double>(x2_-x1_));
    double wx_half = width_/2.0 * std::sin(theta);
    double wy_half = width_/2.0 * std::cos(theta);

    v1array_.emplace_back(static_cast<float>(x1_ - wx_half), static_cast<float>(y1_ + wy_half));
    v1array_.emplace_back(static_cast<float>(x2_ - wx_half), static_cast<float>(y2_ + wy_half));
    v1array_.emplace_back(static_cast<float>(x1_), static_cast<float>(y1_));
    v1array_.emplace_back(static_cast<float>(x2_), static_cast<float>(y2_));
    v1array_.emplace_back(static_cast<float>(x1_ + wx_half), static_cast<float>(y1_ - wy_half));
    v1array_.emplace_back(static_cast<float>(x2_ + wx_half), static_cast<float>(y2_ - wy_half));
    carray_.emplace_back(color1_.r(), color1_.g(), color1_.b(), 0);
    carray_.emplace_back(color2_.r(), color2_.g(), color2_.b(), 0);
    carray_.emplace_back(color1_.r(), color1_.g(), color1_.b(), color1_.a());
    carray_.emplace_back(color2_.r(), color2_.g(), color2_.b(), color2_.a());
    carray_.emplace_back(color1_.r(), color1_.g(), color1_.b(), 0);
    carray_.emplace_back(color2_.r(), color2_.g(), color2_.b(), 0);
    v2array_.emplace_back(static_cast<float>(x1_ - wx_half), static_cast<float>(y1_ + wy_half));
    v2array_.emplace_back(static_cast<float>(x2_ - wx_half), static_cast<float>(y2_ + wy_half));
    v2array_.emplace_back(static_cast<float>(x2_ + wx_half), static_cast<float>(y2_ - wy_half));
    v2array_.emplace_back(static_cast<float>(x1_ + wx_half), static_cast<float>(y1_ - wy_half));
}

void line_primitive::handle_draw() const
{
    gles2::manager gles2_manager(shader_);
    gles2::active_shader()->prepare_draw();

    gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &v1array_.front());
    gles2::active_shader()->shader()->color_array(4, GL_UNSIGNED_BYTE, GL_TRUE, 0, &carray_.front());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, v1array_.size());
    if(has_stroke_) {
        stroke_color_.set_as_current_color();
        // hack
        gles2::active_shader()->shader()->disable_vertex_attrib(-1);
        gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &v2array_.front());
        glDrawArrays(GL_LINE_LOOP, 0, v2array_.size());
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

#ifdef USE_ISOMAP
void line_primitive::handle_draw(const lighting_ptr& lighting, const camera_callable_ptr& camera) const
{
}
#endif

BEGIN_DEFINE_CALLABLE(line_primitive, draw_primitive)
    DEFINE_FIELD(color1, "[int,int,int,int]")
        return obj.color1_.write();
    DEFINE_SET_FIELD_TYPE("[int,int,int,int]|string")
        obj.color1_ = graphics::color(value);
    DEFINE_FIELD(color2, "[int,int,int,int]")
        return obj.color2_.write();
    DEFINE_SET_FIELD_TYPE("[int,int,int,int]|string")
        obj.color2_ = graphics::color(value);
    DEFINE_FIELD(p1, "[int,int]")
        return point(obj.x1_, obj.y1_).write();
    DEFINE_SET_FIELD
        point p1(value);
        obj.x1_ = p1.x;
        obj.y1_ = p1.y;
    DEFINE_FIELD(p2, "[int,int]")
        return point(obj.x2_, obj.y2_).write();
    DEFINE_SET_FIELD
        point p2(value);
        obj.x2_ = p2.x;
        obj.y2_ = p2.y;
END_DEFINE_CALLABLE(line_primitive)



draw_primitive_ptr draw_primitive::create(const variant& v)
{
	if(v.is_callable()) {
		draw_primitive_ptr dp = v.try_convert<draw_primitive>();
		ASSERT_LOG(dp != NULL, "Couldn't convert callable type to draw_primitive");
		return dp;
	}
	const std::string type = v["type"].as_string();
	if(type == "arrow") {
		return new arrow_primitive(v);
	} else if(type == "circle") {
		return new circle_primitive(v);
	} else if(type == "rect") {
		return new rect_primitive(v);
    } else if(type == "line") {
        return new line_primitive(v);
	} else if(type == "box") {
		return new box_primitive(v);
	} else if(type == "box_wireframe") {
		return new wireframe_box_primitive(v);
	}

	ASSERT_LOG(false, "UNKNOWN DRAW PRIMITIVE TYPE: " << v["type"].as_string());
	return draw_primitive_ptr();
}

draw_primitive::draw_primitive(const variant& v)
  : src_factor_(GL_SRC_ALPHA), dst_factor_(GL_ONE_MINUS_SRC_ALPHA)
{
	if(v.has_key("blend")) {
		const std::string blend_mode = v["blend"].as_string();
		if(blend_mode == "overwrite") {
			src_factor_ = GL_ONE;
			dst_factor_ = GL_ZERO;
		} else {
			ASSERT_LOG(false, "Unrecognized blend mode: " << blend_mode);
		}
	}
}

void draw_primitive::draw() const
{
	if(src_factor_ != GL_SRC_ALPHA || dst_factor_ != GL_ONE_MINUS_SRC_ALPHA) {
		glBlendFunc(src_factor_, dst_factor_);
		handle_draw();
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		handle_draw();
	}
}

#ifdef USE_ISOMAP
void draw_primitive::draw(const lighting_ptr& lighting, const camera_callable_ptr& camera) const
{
	if(src_factor_ != GL_SRC_ALPHA || dst_factor_ != GL_ONE_MINUS_SRC_ALPHA) {
		glBlendFunc(src_factor_, dst_factor_);
		handle_draw(lighting, camera);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		handle_draw(lighting, camera);
	}
}
#endif

BEGIN_DEFINE_CALLABLE_NOBASE(draw_primitive)
	DEFINE_FIELD(blend, "string")
		if(obj.src_factor_ == GL_ONE && obj.dst_factor_ == GL_ZERO) {
			return variant("overwrite");
		}
		return variant("normal");
	DEFINE_SET_FIELD
		if(value.as_string() == "overwrite") {
			obj.src_factor_ = GL_ONE;
			obj.dst_factor_ = GL_ZERO;
		} else if(value.as_string() == "normal") {
			obj.src_factor_ = GL_SRC_ALPHA;
			obj.dst_factor_ = GL_ONE_MINUS_SRC_ALPHA;
		} else {
			ASSERT_LOG(false, "Unrecognized blend mode: " << value.as_string());
		}
END_DEFINE_CALLABLE(draw_primitive)

}
#endif
