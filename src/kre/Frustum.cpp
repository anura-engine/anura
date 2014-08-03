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

#include <glm/glm.hpp>

#include "Frustum.hpp"

namespace KRE
{
	namespace
	{
		const char* const face_names[] =
		{
			"near",
			"right",
			"top",
			"far",
			"left",
			"bottom",
		};
	}

	Frustum::Frustum()
	{
		planes_.resize(MAX_PLANES);
	}

	Frustum::~Frustum()
	{
	}

	Frustum::Frustum(const glm::mat4& perspective, const glm::mat4& view)
	{
		planes_.resize(MAX_PLANES);
		updateMatrices(perspective, view);
	}

	namespace
	{
		glm::vec4 normalize(const glm::vec4& v) 
		{
			return v / glm::length(glm::vec3(v.x,v.y,v.z));
		}
	}

	void Frustum::updateMatrices(const glm::mat4& perspective, const glm::mat4& view)
	{
		vp_ = glm::transpose(perspective * view);

		planes_[NEAR_PLANE] = normalize(vp_ * glm::vec4(0,0,-1,1));
		planes_[FAR_PLANE] = normalize(vp_ * glm::vec4(0,0,1,1));
		planes_[LEFT_PLANE] = normalize(vp_ * glm::vec4(1,0,0,1));
		planes_[RIGHT_PLANE] = normalize(vp_ * glm::vec4(-1,0,0,1));
		planes_[BOTTOM_PLANE] = normalize(vp_ * glm::vec4(0,1,0,1));
		planes_[TOP_PLANE] = normalize(vp_ * glm::vec4(0,-1,0,1));
	}

	bool Frustum::isPointInside(const glm::vec3& pt) const
	{
		for(int n = NEAR_PLANE; n < MAX_PLANES; ++n) {
			if(glm::dot(planes_[n], glm::vec4(pt, 1.0f)) < 0.0f) {
				return false;
			}
		}
		return true;
	}

	bool Frustum::isCircleInside(const glm::vec3& pt, float radius) const
	{
		for(int n = NEAR_PLANE; n < MAX_PLANES; ++n) {
			if(glm::dot(planes_[n], glm::vec4(pt, 1.0f)) < -radius) {
				return false;
			}
		}
		return true;
	}

	// Returns >0 if circle is inside the frustum
	// Returns <0 if circle is outside frustum
	// Returns 0 if circle intersects.
	int Frustum::doesCircleIntersect(const glm::vec3& pt, float radius) const
	{
		int out = 0;
		int in = 0;
		for(int n = NEAR_PLANE; n < MAX_PLANES; ++n) {
			if(glm::dot(planes_[n], glm::vec4(pt, 1.0f)) < -radius) {
				++out;
			} else {
				++in;
			}
		}
		return in == MAX_PLANES ? 1 : out == MAX_PLANES ? -1 : 0;
	}

	// Cube specified by one corner and the three side lenghts
	// returns true if is inside else false
	bool Frustum::isCubeInside(const glm::vec3& pt, float xlen, float ylen, float zlen) const
	{
		for(int n = NEAR_PLANE; n < MAX_PLANES; ++n) {
			if(glm::dot(planes_[n], glm::vec4(pt.x, pt.y, pt.z, 1.0)) >= 0.0f) {
				continue;
			}
			if(glm::dot(planes_[n], glm::vec4(pt.x + xlen, pt.y, pt.z, 1.0)) >= 0.0f) {
				continue;
			}
			if(glm::dot(planes_[n], glm::vec4(pt.x, pt.y + ylen, pt.z, 1.0)) >= 0.0f) {
				continue;
			}
			if(glm::dot(planes_[n], glm::vec4(pt.x, pt.y, pt.z + zlen, 1.0)) >= 0.0f) {
				continue;
			}
			if(glm::dot(planes_[n], glm::vec4(pt.x + xlen, pt.y + ylen, pt.z, 1.0)) >= 0.0f) {
				continue;
			}
			if(glm::dot(planes_[n], glm::vec4(pt.x + xlen, pt.y, pt.z + zlen, 1.0)) >= 0.0f) {
				continue;
			}
			if(glm::dot(planes_[n], glm::vec4(pt.x, pt.y + ylen, pt.z + zlen, 1.0)) >= 0.0f) {
				continue;
			}
			if(glm::dot(planes_[n], glm::vec4(pt.x + xlen, pt.y + ylen, pt.z + zlen, 1.0)) >= 0.0f) {
				continue;
			}
			//std::cerr << "Failed: " << pt.x << "," << pt.y << "," << pt.z << " - " << face_names[n]  << " " << planes_[n].x << "," << planes_[n].y << "," << planes_[n].z << "," << planes_[n].w << std::endl;
			return false;
		}
		return true;
	}

	// Returns >0 if cube is inside the frustum
	// Returns <0 if cube is outside frustum
	// Returns 0 if cube intersects.
	int Frustum::doesCubeIntersect(const glm::vec3& pt, float xlen, float ylen, float zlen) const
	{
		int in = 0;
		int out = 0;
		for(int n = NEAR_PLANE; n < MAX_PLANES; ++n) {
			if(glm::dot(planes_[n], glm::vec4(pt.x, pt.y, pt.z, 1.0)) >= 0.0f) {
				++in;
			} else {
				++out;
			}
			if(glm::dot(planes_[n], glm::vec4(pt.x + xlen, pt.y, pt.z, 1.0)) >= 0.0f) {
				++in;
			} else {
				++out;
			}
			if(glm::dot(planes_[n], glm::vec4(pt.x, pt.y + ylen, pt.z, 1.0)) >= 0.0f) {
				++in;
			} else {
				++out;
			}
			if(glm::dot(planes_[n], glm::vec4(pt.x, pt.y, pt.z + zlen, 1.0)) >= 0.0f) {
				++in;
			} else {
				++out;
			}
			if(glm::dot(planes_[n], glm::vec4(pt.x + xlen, pt.y + ylen, pt.z, 1.0)) >= 0.0f) {
				++in;
			} else {
				++out;
			}
			if(glm::dot(planes_[n], glm::vec4(pt.x + xlen, pt.y, pt.z + zlen, 1.0)) >= 0.0f) {
				++in;
			} else {
				++out;
			}
			if(glm::dot(planes_[n], glm::vec4(pt.x, pt.y + ylen, pt.z + zlen, 1.0)) >= 0.0f) {
				++in;
			} else {
				++out;
			}
			if(glm::dot(planes_[n], glm::vec4(pt.x + xlen, pt.y + ylen, pt.z + zlen, 1.0)) >= 0.0f) {
				++in;
			} else {
				++out;
			}
		}
		//std::cerr << "pt: " << pt.x << "," << pt.y << "," << pt.z << " : in: " << in << ", out: " << out << std::endl;
		return in == 0 ? -1 : out != 0 ? 0 : 1;
	}
}
