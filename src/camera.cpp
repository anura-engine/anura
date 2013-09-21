#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#if defined(_MSC_VER)
#include <boost/math/special_functions/round.hpp>
#define bmround	boost::math::round
#else
#define bmround	round
#endif

#include "camera.hpp"
#include "preferences.hpp"
#include "profile_timer.hpp"
#include "variant_utils.hpp"

#ifdef USE_ISOMAP

camera_callable::camera_callable()
	: fov_(45.0f), horizontal_angle_(float(M_PI)), vertical_angle_(0.0f),
	speed_(0.1f), mouse_speed_(0.005f), near_clip_(0.1f), far_clip_(300.0f),
	mode_(MODE_AUTO), type_(PERSPECTIVE_CAMERA), ortho_left_(0), ortho_bottom_(0),
	ortho_top_(preferences::actual_screen_height()), ortho_right_(preferences::actual_screen_width())
{
	up_ = glm::vec3(0.0f, 1.0f, 0.0f);
	position_ = glm::vec3(0.0f, 0.0f, 10.0f); 
	aspect_ = float(preferences::actual_screen_width())/float(preferences::actual_screen_height());
	
	compute_view();
	compute_projection();
}

camera_callable::camera_callable(const variant& node)
	: fov_(45.0f), horizontal_angle_(float(M_PI)), vertical_angle_(0.0f),
	speed_(0.1f), mouse_speed_(0.005f), near_clip_(0.1f), far_clip_(300.0f),
	mode_(MODE_AUTO), type_(PERSPECTIVE_CAMERA), ortho_left_(0), ortho_bottom_(0),
	ortho_top_(preferences::actual_screen_height()), ortho_right_(preferences::actual_screen_width())
{
	position_ = glm::vec3(0.0f, 0.0f, 10.0f); 
	if(node.has_key("fov")) {
		fov_ = std::min(90.0f, std::max(15.0f, float(node["fov"].as_decimal().as_float())));
	}
	if(node.has_key("horizontal_angle")) {
		horizontal_angle_ = float(node["horizontal_angle"].as_decimal().as_float());
	}
	if(node.has_key("vertical_angle")) {
		vertical_angle_ = float(node["vertical_angle"].as_decimal().as_float());
	}
	if(node.has_key("speed")) {
		speed_ = float(node["speed"].as_decimal().as_float());
	}
	if(node.has_key("mouse_speed")) {
		mouse_speed_ = float(node["mouse_speed"].as_decimal().as_float());
	}
	if(node.has_key("aspect")) {
		aspect_ = float(node["aspect"].as_decimal().as_float());
	} else {
		aspect_ = float(preferences::actual_screen_width())/float(preferences::actual_screen_height());
	}

	if(node.has_key("position")) {
		ASSERT_LOG(node["position"].is_list() && node["position"].num_elements() == 3, 
			"position must be a list of 3 decimals.");
		position_ = glm::vec3(float(node["position"][0].as_decimal().as_float()),
			float(node["position"][1].as_decimal().as_float()),
			float(node["position"][2].as_decimal().as_float()));
	}

	if(node.has_key("type")) {
		if(node["type"].as_string() == "orthogonal") {
			type_ = ORTHOGONAL_CAMERA;
		}
	}
	if(node.has_key("ortho_window")) {
		ASSERT_LOG(node["ortho_window"].is_list() && node["ortho_window"].num_elements() == 4, "Attribute 'ortho_window' must be a 4 element list. left,right,top,bottom");
		ortho_left_ = node["ortho_window"][0].as_int();
		ortho_right_ = node["ortho_window"][1].as_int();
		ortho_top_ = node["ortho_window"][2].as_int();
		ortho_bottom_ = node["ortho_window"][3].as_int();
	}

	// If lookat key is specified it overrides the normal compute.
	if(node.has_key("lookat")) {
		const variant& la = node["lookat"];
		ASSERT_LOG(la.has_key("position") && la.has_key("target") && la.has_key("up"),
			"lookat must be a map having 'position', 'target' and 'up' as tuples");
		glm::vec3 position(la["position"][0].as_decimal().as_float(), 
			la["position"][1].as_decimal().as_float(), 
			la["position"][2].as_decimal().as_float());
		glm::vec3 target(la["target"][0].as_decimal().as_float(), 
			la["target"][1].as_decimal().as_float(), 
			la["target"][2].as_decimal().as_float());
		glm::vec3 up(la["up"][0].as_decimal().as_float(), 
			la["up"][1].as_decimal().as_float(), 
			la["up"][2].as_decimal().as_float());
		look_at(position, target, up);
		mode_ = MODE_MANUAL;
	} else {
		compute_view();
	}
	compute_projection();
}

camera_callable::~camera_callable()
{
}

variant camera_callable::write()
{
	variant_builder res;
	if(fov_ != 45.0) {
		res.add("fov", fov_);
	}
	if(horizontal_angle_ != float(M_PI)) {
		res.add("horizontal_angle", horizontal_angle_);
	}
	if(vertical_angle_ != 0.0f) {
		res.add("vertical_angle", vertical_angle_);
	}
	if(speed_ != 0.1f) {
		res.add("speed", speed_);
	}
	if(mouse_speed_ != 0.005f) {
		res.add("mouse_speed", mouse_speed_);
	}
	res.add("position", position_.x);
	res.add("position", position_.y);
	res.add("position", position_.z);
	return res.build();
}

void camera_callable::compute_view()
{
	mode_ = MODE_AUTO;
	direction_ = glm::vec3(
		cos(vertical_angle_) * sin(horizontal_angle_), 
		sin(vertical_angle_),
		cos(vertical_angle_) * cos(horizontal_angle_)
	);
	right_ = glm::vec3(
		sin(horizontal_angle_ - float(M_PI)/2.0f), 
		0,
		cos(horizontal_angle_ - float(M_PI)/2.0f)
	);
	
	// Up vector
	up_ = glm::cross(right_, direction_);
	target_ = position_ + direction_;

	view_ = glm::lookAt(position_, target_, up_);
	frustum_.update_matrices(projection_, view_);
}

void camera_callable::set_type(CAMERA_TYPE type)
{
	type_ = type;
	compute_projection();
}

void camera_callable::set_ortho_window(int left, int right, int top, int bottom)
{
	ortho_left_ = left;
	ortho_right_ = right;
	ortho_top_ = top;
	ortho_bottom_ = bottom;
	
	if(type_ == ORTHOGONAL_CAMERA) {
		compute_projection();
	}
}

BEGIN_DEFINE_CALLABLE_NOBASE(camera_callable)
BEGIN_DEFINE_FN(screen_to_world, "(int,int,int=0,int=0) -> [decimal,decimal,decimal]")
	int wx = preferences::actual_screen_width();
	int wy = preferences::actual_screen_height();
	if(NUM_FN_ARGS > 2) {
		wx = FN_ARG(2).as_int();
		if(NUM_FN_ARGS > 3) {
			wy = FN_ARG(3).as_int();
		}
	}
	return vec3_to_variant(obj.screen_to_world(FN_ARG(0).as_int(), FN_ARG(1).as_int(),wx,wy));
END_DEFINE_FN

DEFINE_FIELD(position, "[decimal,decimal,decimal]")
	std::vector<variant> v;
	v.push_back(variant(obj.position().x));
	v.push_back(variant(obj.position().y));
	v.push_back(variant(obj.position().z));
	return variant(&v);
DEFINE_SET_FIELD
	ASSERT_LOG(value.is_list() && value.num_elements() == 3, "position must be a list of 3 elements");
	obj.set_position(glm::vec3(float(value[0].as_decimal().as_float()),
		float(value[1].as_decimal().as_float()),
		float(value[2].as_decimal().as_float())));
	if(obj.mode_ == MODE_MANUAL) {
		obj.view_ = glm::lookAt(obj.position_, obj.target_, obj.up_);
		obj.frustum_.update_matrices(obj.projection_, obj.view_);
	} else {
		obj.compute_view();
	}

DEFINE_FIELD(speed, "decimal")
	return variant(obj.speed());
DEFINE_SET_FIELD
	obj.set_speed(value.as_decimal().as_float());

DEFINE_FIELD(right, "[decimal,decimal,decimal]")
	std::vector<variant> v;
	v.push_back(variant(obj.right().x));
	v.push_back(variant(obj.right().y));
	v.push_back(variant(obj.right().z));
	return variant(&v);

DEFINE_FIELD(direction, "[decimal,decimal,decimal]")
	std::vector<variant> v;
	v.push_back(variant(obj.direction().x));
	v.push_back(variant(obj.direction().y));
	v.push_back(variant(obj.direction().z));
	return variant(&v);

DEFINE_FIELD(horizontal_angle, "decimal")
	return variant(obj.hangle());
DEFINE_SET_FIELD
	obj.set_hangle(value.as_decimal().as_float());
	obj.compute_view();

DEFINE_FIELD(hangle, "decimal")
	return variant(obj.hangle());
DEFINE_SET_FIELD
	obj.set_hangle(value.as_decimal().as_float());
	obj.compute_view();

DEFINE_FIELD(vertical_angle, "decimal")
	return variant(obj.vangle());
DEFINE_SET_FIELD
	obj.set_vangle(value.as_decimal().as_float());
	obj.compute_view();

DEFINE_FIELD(vangle, "decimal")
	return variant(obj.vangle());
DEFINE_SET_FIELD
	obj.set_vangle(value.as_decimal().as_float());
	obj.compute_view();

DEFINE_FIELD(mouse_speed, "decimal")
	return variant(obj.mousespeed());
DEFINE_SET_FIELD
	obj.set_mousespeed(value.as_decimal().as_float());

DEFINE_FIELD(target, "[decimal,decimal,decimal]")
	std::vector<variant> v;
	v.push_back(variant(obj.target_.x));
	v.push_back(variant(obj.target_.y));
	v.push_back(variant(obj.target_.z));
	return variant(&v);

DEFINE_FIELD(up, "[decimal,decimal,decimal]")
	std::vector<variant> v;
	v.push_back(variant(obj.up_.x));
	v.push_back(variant(obj.up_.y));
	v.push_back(variant(obj.up_.z));
	return variant(&v);

DEFINE_FIELD(fov, "decimal")
	return variant(obj.fov());
DEFINE_SET_FIELD
	obj.set_fov(value.as_decimal().as_float());

DEFINE_FIELD(aspect, "decimal")
	return variant(obj.aspect());
DEFINE_SET_FIELD
	obj.set_aspect(value.as_decimal().as_float());

DEFINE_FIELD(clip_planes, "[decimal,decimal]")
	std::vector<variant> v;
	v.push_back(variant(obj.near_clip_));
	v.push_back(variant(obj.far_clip_));
	return variant(&v);
DEFINE_SET_FIELD
	ASSERT_LOG(value.is_list() && value.num_elements() == 2, "clip_planes takes a tuple of two decimals");
	obj.set_clip_planes(value[0].as_decimal().as_float(), value[1].as_decimal().as_float());

DEFINE_FIELD(type, "string")
	if(obj.type() == ORTHOGONAL_CAMERA) {
		return variant("orthogonal");
	} 
	return variant("perspective");
DEFINE_SET_FIELD
	if(value.as_string() == "orthogonal") {
		obj.set_type(ORTHOGONAL_CAMERA);
	} else {
		obj.set_type(PERSPECTIVE_CAMERA);
	}
	
DEFINE_FIELD(ortho_window, "[int,int,int,int]")
	std::vector<variant> v;
	v.push_back(variant(obj.ortho_left()));
	v.push_back(variant(obj.ortho_right()));
	v.push_back(variant(obj.ortho_top()));
	v.push_back(variant(obj.ortho_bottom()));
	return variant(&v);
DEFINE_SET_FIELD
	ASSERT_LOG(value.is_list() && value.num_elements() == 4, "Attribute 'ortho_window' must be a 4 element list. left,right,top,bottom");
	obj.set_ortho_window(value[0].as_int(), value[1].as_int(), value[2].as_int(), obj.ortho_bottom_ = value[3].as_int());

END_DEFINE_CALLABLE(camera_callable)


void camera_callable::look_at(glm::vec3 position, glm::vec3 target, glm::vec3 up)
{
	mode_ = MODE_MANUAL;
	position_ = position;
	target_ = target;
	up_ = up;
	direction_ = target_ - position_;
	view_ = glm::lookAt(position_, target_, up_);
	frustum_.update_matrices(projection_, view_);
}


void camera_callable::set_fov(float fov)
{
	fov_ = fov;
	compute_projection();
}

void camera_callable::set_clip_planes(float z_near, float z_far)
{
	near_clip_ = z_near;
	far_clip_ = z_far;
	compute_projection();
}

void camera_callable::set_aspect(float aspect)
{
	aspect_ = aspect;
	compute_projection();
}

void camera_callable::compute_projection()
{
	if(type_ == ORTHOGONAL_CAMERA) {
		projection_ = glm::frustum(float(ortho_left_), float(ortho_right_), float(ortho_bottom_), float(ortho_top_), near_clip(), far_clip());
	} else {
		projection_ = glm::perspective(fov(), aspect_, near_clip(), far_clip());
	}
	frustum_.update_matrices(projection_, view_);
}

// Convert from a screen position (assume +ve x to right, +ve y down) to world space.
// Assumes the depth buffer was enabled.
glm::vec3 camera_callable::screen_to_world(int x, int y, int wx, int wy) const
{
	glm::vec4 view_port(0, 0, wx, wy);

	GLfloat depth;
	glReadPixels(x, wy - y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
	glm::vec3 screen(x, wy - y, depth);

	return glm::unProject(screen, view_, projection_, view_port);
}


namespace
{
	float dti(float val) 
	{
		return abs(val - bmround(val));
	}
}

glm::ivec3 camera_callable::get_facing(const glm::vec3& coords) const
{
	if(dti(coords.x) < dti(coords.y)) {
		if(dti(coords.x) < dti(coords.z)) {
			if(direction_.x > 0) {
				return glm::ivec3(-1,0,0);
			} else {
				return glm::ivec3(1,0,0);
			}
		} else {
			if(direction_.z > 0) {
				return glm::ivec3(0,0,-1);
			} else {
				return glm::ivec3(0,0,1);
			}
		}
	} else {
		if(dti(coords.y) < dti(coords.z)) {
			if(direction_.y > 0) {
				return glm::ivec3(0,-1,0);
			} else {
				return glm::ivec3(0,1,0);
			}
		} else {
			if(direction_.z > 0) {
				return glm::ivec3(0,0,-1);
			} else {
				return glm::ivec3(0,0,1);
			}
		}
	}

}

#endif //USE_ISOMAP
