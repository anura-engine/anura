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

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace KRE
{
	class Frustum
	{
	public:
		Frustum();
		virtual ~Frustum();
		explicit Frustum(const glm::mat4& perspective, const glm::mat4& view);
		
		void updateMatrices(const glm::mat4& perspective, const glm::mat4& view);

		bool isPointInside(const glm::vec3& pt) const;
		bool isCircleInside(const glm::vec3& pt, float radius) const;
		bool isCubeInside(const glm::vec3& pt, float xlen, float ylen, float zlen) const;
		
		int doesCircleIntersect(const glm::vec3& pt, float radius) const;
		int doesCubeIntersect(const glm::vec3& pt, float xlen, float ylen, float zlen) const;
	private:
		enum 
		{
			NEAR_PLANE,
			RIGHT_PLANE,
			TOP_PLANE,
			FAR_PLANE,
			LEFT_PLANE,
			BOTTOM_PLANE,
			MAX_PLANES,
		};
		std::vector<glm::vec4> planes_;
		glm::mat4 vp_;
	};

	typedef std::shared_ptr<Frustum> FrustumPtr;
}
