#pragma once

#include <boost/intrusive_ptr.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "frustum.hpp"
#include "variant.hpp"

class camera_callable : public game_logic::formula_callable
{
public:
	enum CAMERA_TYPE { PERSPECTIVE_CAMERA, ORTHOGONAL_CAMERA };
	camera_callable();
	explicit camera_callable(CAMERA_TYPE type, int left, int right, int top, int bottom);
	explicit camera_callable(CAMERA_TYPE type, float fov, float aspect, float near_clip, float far_clip);
	explicit camera_callable(const variant& node);
	virtual ~camera_callable();
	//virtual variant get_value(const std::string&) const;
	//virtual void set_value(const std::string& key, const variant& value);

	void compute_view();
	void compute_projection();

	void set_mousespeed(float ms) { mouse_speed_ = ms; }
	void set_speed(float spd) { speed_ = spd; }
	void set_hangle(float ha) { horizontal_angle_ = ha; }
	void set_vangle(float va) { vertical_angle_ = va; }
	void set_fov(float fov);
	void set_aspect(float aspect);
	void set_clip_planes(float z_near, float z_far);
	void set_type(CAMERA_TYPE type);
	void set_ortho_window(int left, int right, int top, int bottom);
	float mousespeed() const { return mouse_speed_; }
	float speed() const { return speed_; }
	float hangle() const { return horizontal_angle_; }
	float vangle() const { return vertical_angle_; }
	float fov() const { return fov_; }
	float aspect() const { return aspect_; }
	float near_clip() const { return near_clip_; }
	float far_clip() const { return far_clip_; }
	CAMERA_TYPE type() const { return type_; }
	int ortho_left() const { return ortho_left_; }
	int ortho_right() const { return ortho_right_; }
	int ortho_top() const { return ortho_top_; }
	int ortho_bottom() const { return ortho_bottom_; }
	const glm::vec3& position() const { return position_; }
	const glm::vec3& right() const { return right_; }
	const glm::vec3& direction() const { return direction_; }
	const glm::vec3& target() const { return target_; }
	const glm::vec3& up() const { return up_; }
	void set_position(const glm::vec3& position) { position_ = position; }

	void look_at(glm::vec3 position, glm::vec3 target, glm::vec3 up);

	const float* projection() const { return glm::value_ptr(projection_); }
	const float* view() const { return glm::value_ptr(view_); }
	const glm::mat4& view_mat() const { return view_; }
	const glm::mat4& projection_mat() const { return projection_; }

	const graphics::frustum& frustum() { return frustum_; }

	glm::vec3 screen_to_world(int x, int y, int wx, int wy) const;
	glm::ivec3 get_facing(const glm::vec3& coords) const;

	variant write();
protected:
private:
	DECLARE_CALLABLE(camera_callable);

	enum {MODE_AUTO, MODE_MANUAL} mode_;

	float fov_;
	float horizontal_angle_;
	float vertical_angle_;
	glm::vec3 position_;
	glm::vec3 target_;
	glm::vec3 up_;
	glm::vec3 right_;
	glm::vec3 direction_;
	float speed_;
	float mouse_speed_;

	float near_clip_;
	float far_clip_;
	bool clip_planes_set_;

	float aspect_;

	graphics::frustum frustum_;

	CAMERA_TYPE type_;
	int ortho_left_;
	int ortho_right_;
	int ortho_top_;
	int ortho_bottom_;

	glm::mat4 projection_;
	glm::mat4 view_;

	camera_callable(const camera_callable&);
};

typedef boost::intrusive_ptr<camera_callable> camera_callable_ptr;
typedef boost::intrusive_ptr<const camera_callable> const_camera_callable_ptr;
