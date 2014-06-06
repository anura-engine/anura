/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "Frustum.hpp"
#include "SceneObject.hpp"
#include "WindowManagerFwd.hpp"
#include <glm/gtc/type_ptr.hpp>

namespace KRE
{
	class Camera : public SceneObject
	{
	public:
		enum CameraType { CAMERA_PERSPECTIVE, CAMERA_ORTHOGONAL };
		Camera(const std::string& name, const WindowManagerPtr& wnd);
		explicit Camera(const std::string& name, int left, int right, int top, int bottom);
		explicit Camera(const std::string& name, const WindowManagerPtr& wnd, float fov, float aspect, float near_clip, float far_clip);
		virtual ~Camera();

		void SetMouseSpeed(float ms) { mouse_speed_ = ms; }
		void SetSpeed(float spd) { speed_ = spd; }
		void SetHangle(float ha) { horizontal_angle_ = ha; }
		void SetVangle(float va) { vertical_angle_ = va; }
		void SetFov(float fov);
		void SetAspect(float aspect);
		void SetClipPlanes(float z_near, float z_far);
		void SetType(CameraType type);
		void SetOrthoWindow(int left, int right, int top, int bottom);
		float Mousespeed() const { return mouse_speed_; }
		float Speed() const { return speed_; }
		float Hangle() const { return horizontal_angle_; }
		float Vangle() const { return vertical_angle_; }
		float Fov() const { return fov_; }
		float Aspect() const { return aspect_; }
		float NearClip() const { return near_clip_; }
		float FarClip() const { return far_clip_; }
		CameraType Type() const { return type_; }
		int OrthoLeft() const { return ortho_left_; }
		int OrthoRight() const { return ortho_right_; }
		int OrthoTop() const { return ortho_top_; }
		int OrthoBottom() const { return ortho_bottom_; }
		const glm::vec3& Position() const { return position_; }
		const glm::vec3& Right() const { return right_; }
		const glm::vec3& Direction() const { return direction_; }
		const glm::vec3& Target() const { return target_; }
		const glm::vec3& Up() const { return up_; }
		void SetPosition(const glm::vec3& position) { position_ = position; }

		void LookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up);

		const float* Projection() const { return glm::value_ptr(projection_); }
		const float* View() const { return glm::value_ptr(view_); }
		const glm::mat4& ViewMat() const { return view_; }
		const glm::mat4& ProjectionMat() const { return projection_; }

		const FrustumPtr& GetFrustum() const { return frustum_; }
		void AttachFrustum(const FrustumPtr& frustum);

		glm::vec3 ScreenToWorld(int x, int y, int wx, int wy) const;
		glm::ivec3 GetFacing(const glm::vec3& coords) const;

		//variant write();

		void ComputeView();
		void ComputeProjection();

		virtual DisplayDeviceDef Attach(const DisplayDevicePtr& dd) override;
	private:
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

		Camera(const Camera&);
		Camera& operator=(const Camera&);
	};
}