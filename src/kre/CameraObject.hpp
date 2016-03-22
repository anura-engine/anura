/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "geometry.hpp"
#include "Frustum.hpp"
#include "SceneObject.hpp"

namespace KRE
{
	class Camera : public SceneObject
	{
	public:
		enum CameraType { CAMERA_PERSPECTIVE, CAMERA_ORTHOGONAL };
		explicit Camera(const std::string& name);
		explicit Camera(const std::string& name, int left, int right, int top, int bottom);
		explicit Camera(const std::string& name, const rect& r);
		explicit Camera(const std::string& name, float fov, float aspect, float near_clip, float far_clip);
		explicit Camera(const variant& node);

		void setMouseSpeed(float ms) { mouse_speed_ = ms; }
		void setSpeed(float spd) { speed_ = spd; }
		void setHangle(float ha) { horizontal_angle_ = ha; }
		void setVangle(float va) { vertical_angle_ = va; }
		void setFov(float fov);
		void setAspect(float aspect);
		void setClipPlanes(float z_near, float z_far);
		void setType(CameraType type);
		void setOrthoWindow(int left, int right, int top, int bottom);
		float getMousespeed() const { return mouse_speed_; }
		float getSpeed() const { return speed_; }
		float getHangle() const { return horizontal_angle_; }
		float getVangle() const { return vertical_angle_; }
		float getFov() const { return fov_; }
		float getAspect() const { return aspect_; }
		float getNearClip() const { return near_clip_; }
		float getFarClip() const { return far_clip_; }
		CameraType getType() const { return type_; }
		int getOrthoLeft() const { return ortho_left_; }
		int getOrthoRight() const { return ortho_right_; }
		int getOrthoTop() const { return ortho_top_; }
		int getOrthoBottom() const { return ortho_bottom_; }
		const glm::vec3& getPosition() const { return position_; }
		const glm::vec3& getRight() const { return right_; }
		const glm::vec3& getDirection() const { return direction_; }
		const glm::vec3& getTarget() const { return target_; }
		const glm::vec3& getUp() const { return up_; }
		void setPosition(const glm::vec3& position) { position_ = position; }

		void lookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up);

		const float* getProjection() const { return glm::value_ptr(projection_); }
		const float* getView() const { return glm::value_ptr(view_); }
		const glm::mat4& getViewMat() const { return view_; }
		const glm::mat4& getProjectionMat() const { return projection_; }

		const FrustumPtr& getFrustum() const { return frustum_; }
		void attachFrustum(const FrustumPtr& frustum);

		glm::vec3 screenToWorld(int x, int y, int wx, int wy) const;
		glm::ivec3 getFacing(const glm::vec3& coords) const;

		CameraPtr clone();

		variant write();

#if _MSC_VER < 1800
		// Handling for MSVC versions that don't support variadic templates.
		static CameraPtr createInstance(const std::string& name) {
			return std::make_shared<Camera>(name);
		}
		static CameraPtr createInstance(const std::string& name, int left, int right, int top, int bottom) {
			return std::make_shared<Camera>(name, left, top, right, bottom);
		}
		static CameraPtr createInstance(const std::string& name, float fov, float aspect, float near_clip, float far_clip) {
			return std::make_shared<Camera>(name, fov, aspect, near_clip, far_clip);
		}
		static CameraPtr createInstance(const variant& node) {
			return std::make_shared<Camera>(node);
		}
#else
		template<typename... T>
		static CameraPtr createInstance(T&& ... args) {
			return CameraPtr(new Camera(std::forward<T>(args)...));
		}
#endif

		void createFrustum();

	private:
		void computeView();
		void computeProjection();
		// special case handling if LookAt function is called.
		// Since we then are specifying the position/target/up
		// vectors directly rather than being calculated.
		enum {
			VIEW_MODE_MANUAL,
			VIEW_MODE_AUTO,
		} view_mode_;

		CameraType type_;

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

		FrustumPtr frustum_;

		int ortho_left_;
		int ortho_right_;
		int ortho_top_;
		int ortho_bottom_;

		glm::mat4 projection_;
		glm::mat4 view_;

		Camera& operator=(const Camera&);
	};
}