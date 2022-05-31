//-----------------------------------------------------------------------------
// COMPILE-TIME OPTIONS FOR DEAR IMGUI
// See https://github.com/ocornut/imgui/blob/master/imconfig.h for supported options.
//-----------------------------------------------------------------------------

#pragma once

#include "glm/glm.hpp"

#define IM_VEC2_CLASS_EXTRA		\
	ImVec2(const glm::vec2& f) { x = f.x; y = f.y; } \
	operator glm::vec2() const { return glm::vec2(x, y); }

#define IM_VEC3_CLASS_EXTRA		\
	ImVec3(const glm::vec3& f) { x = f.x; y = f.y; z = f.z;}	\
	operator glm::vec3() const { return glm::vec3(x,y,z); }

#define IM_VEC4_CLASS_EXTRA		\
	ImVec4(const glm::vec4& f) { x = f.x; y = f.y; z = f.z; w = f.w; } \
	operator glm::vec4() const { return glm::vec4(x,y,z,w); }

#define IM_QUAT_CLASS_EXTRA		\
	ImQuat(const glm::quat& f) { x = f.x; y = f.y; z = f.z; w = f.w; }	\
	operator glm::quat() const { return glm::quat(w,x,y,z); }

#define IMGUI_DEFINE_MATH_OPERATORS     1
