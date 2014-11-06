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
#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace graphics
{
	class frustum
	{
	public:
		frustum();
		virtual ~frustum();
		explicit frustum(const glm::mat4& perspective, const glm::mat4& view);
		
		void update_matrices(const glm::mat4& perspective, const glm::mat4& view);

		bool point_inside(const glm::vec3& pt) const;
		bool circle_inside(const glm::vec3& pt, float radius) const;
		bool cube_inside(const glm::vec3& pt, float xlen, float ylen, float zlen) const;
		
		int circle_intersects(const glm::vec3& pt, float radius) const;
		int cube_intersects(const glm::vec3& pt, float xlen, float ylen, float zlen) const;

		void draw() const;
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
}
