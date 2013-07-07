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