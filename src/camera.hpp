#pragma once

#include <boost/intrusive_ptr.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "variant.hpp"

#if defined(USE_ISOMAP)

class camera_callable : public game_logic::formula_callable
{
public:
	camera_callable();
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
	void set_clip_planes(float z_near, float z_far);
	float mousespeed() const { return mouse_speed_; }
	float speed() const { return speed_; }
	float hangle() const { return horizontal_angle_; }
	float vangle() const { return vertical_angle_; }
	float fov() const { return fov_; }
	float near_clip() const { return near_clip_; }
	float far_clip() const { return far_clip_; }
	const glm::vec3& position() const { return position_; }
	const glm::vec3& right() const { return right_; }
	const glm::vec3& direction() const { return direction_; }
	void set_position(const glm::vec3& position) { position_ = position; }

	void look_at(glm::vec3 position, glm::vec3 target, glm::vec3 up);

	const float* projection() const { return glm::value_ptr(projection_); }
	const float* view() const { return glm::value_ptr(view_); }
	const glm::mat4& view_mat() const { return view_; }
	const glm::mat4& projection_mat() const { return projection_; }

	variant write();
protected:
private:
	DECLARE_CALLABLE(camera_callable);

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

	glm::mat4 projection_;
	glm::mat4 view_;
};

typedef boost::intrusive_ptr<camera_callable> camera_callable_ptr;
typedef boost::intrusive_ptr<const camera_callable> const_camera_callable_ptr;

#endif
