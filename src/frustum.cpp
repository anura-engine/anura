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
#include "asserts.hpp"
#include "frustum.hpp"
#include "graphics.hpp"
//#include "row_echelon.hpp"
#include "unit_test.hpp"

namespace graphics
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
	};

	frustum::frustum()
	{
		planes_.resize(MAX_PLANES);
	}

	frustum::~frustum()
	{
	}

	frustum::frustum(const glm::mat4& perspective, const glm::mat4& view)
	{
		planes_.resize(MAX_PLANES);
		update_matrices(perspective, view);
	}

	namespace
	{
		glm::vec4 normalize(const glm::vec4& v) 
		{
			return v / glm::length(glm::vec3(v.x,v.y,v.z));
		}
	}

	void frustum::update_matrices(const glm::mat4& perspective, const glm::mat4& view)
	{
		vp_ = glm::transpose(perspective * view);

		// planes are converted to hessian normal form.
		//planes_[NEAR_PLANE] = normalize(vp_[3] + vp_[2]);
		//planes_[FAR_PLANE] = normalize(-vp_[3] - vp_[2]);
		//planes_[LEFT_PLANE] = normalize(vp_[3] + vp_[0]);
		//planes_[RIGHT_PLANE] = normalize(-vp_[3] - vp_[0]);
		//planes_[BOTTOM_PLANE] = normalize(vp_[3] + vp_[1]);
		//planes_[TOP_PLANE] = normalize(-vp_[3] - vp_[1]);
		planes_[NEAR_PLANE] = normalize(vp_ * glm::vec4(0,0,-1,1));
		planes_[FAR_PLANE] = normalize(vp_ * glm::vec4(0,0,1,1));
		planes_[LEFT_PLANE] = normalize(vp_ * glm::vec4(1,0,0,1));
		planes_[RIGHT_PLANE] = normalize(vp_ * glm::vec4(-1,0,0,1));
		planes_[BOTTOM_PLANE] = normalize(vp_ * glm::vec4(0,1,0,1));
		planes_[TOP_PLANE] = normalize(vp_ * glm::vec4(0,-1,0,1));
	}

	bool frustum::point_inside(const glm::vec3& pt) const
	{
		for(int n = NEAR_PLANE; n < MAX_PLANES; ++n) {
			if(glm::dot(planes_[n], glm::vec4(pt, 1.0f)) < 0.0f) {
				return false;
			}
		}
		return true;
	}

	bool frustum::circle_inside(const glm::vec3& pt, float radius) const
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
	int frustum::circle_intersects(const glm::vec3& pt, float radius) const
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
	bool frustum::cube_inside(const glm::vec3& pt, float xlen, float ylen, float zlen) const
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
	int frustum::cube_intersects(const glm::vec3& pt, float xlen, float ylen, float zlen) const
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

	void frustum::draw() const
	{
		/*
		// XXX
		glm::mat3x4 p[8];
		p[0] = glm::mat3x4(planes_[0], planes_[1], planes_[2]);
		p[1] = glm::mat3x4(planes_[0], planes_[1], planes_[2]);
		p[2] = glm::mat3x4(planes_[0], planes_[1], planes_[2]);
		p[3] = glm::mat3x4(planes_[0], planes_[1], planes_[2]);
		p[4] = glm::mat3x4(planes_[0], planes_[1], planes_[2]);
		p[5] = glm::mat3x4(planes_[0], planes_[1], planes_[2]);
		p[6] = glm::mat3x4(planes_[0], planes_[1], planes_[2]);
		p[7] = glm::mat3x4(planes_[0], planes_[1], planes_[2]);
		for(int n = 0; n != 8; ++n) {
			to_reduced_row_echelon_form(p[n]);
		}
		gles2::manager manager(gles2::shader_program::get_global("line_3d"));
		static GLuint col = -1;
		if(col == -1) {
			col = gles2::active_shader()->shader()->get_fixed_uniform("color");
		}
		glUniform4f(col, 255.0f, 0.0f, 0.0f, 255.0f);
		glUniform4f(colgles2::active_shader()->shader()->get_mvp_uniform(), );
		std::vector<GLfloat> varray;
		varray.push_back(p[0][3][0]); varray.push_back(p[0][3][1]); varray.push_back(p[0][3][2]);
		varray.push_back(p[1][3][0]); varray.push_back(p[1][3][1]); varray.push_back(p[1][3][2]);
		varray.push_back(p[2][3][0]); varray.push_back(p[2][3][1]); varray.push_back(p[2][3][2]);
		varray.push_back(p[3][3][0]); varray.push_back(p[3][3][1]); varray.push_back(p[3][3][2]);
		gles2::active_shader()->shader()->vertex_array(3, GL_FLOAT, GL_FALSE, 0, &varray[0]);
		glDrawArrays(GL_LINE_STRIP, 0, varray.size()/3);
		varray.clear();
		varray.push_back(p[4][3][0]); varray.push_back(p[4][3][1]); varray.push_back(p[4][3][2]);
		varray.push_back(p[5][3][0]); varray.push_back(p[5][3][1]); varray.push_back(p[5][3][2]);
		varray.push_back(p[6][3][0]); varray.push_back(p[6][3][1]); varray.push_back(p[6][3][2]);
		varray.push_back(p[7][3][0]); varray.push_back(p[7][3][1]); varray.push_back(p[7][3][2]);
		gles2::active_shader()->shader()->vertex_array(3, GL_FLOAT, GL_FALSE, 0, &varray[0]);
		glDrawArrays(GL_LINE_STRIP, 0, varray.size()/3);*/
	}
}

UNIT_TEST(frustum)
{
	graphics::frustum f(glm::perspective(45.0f, 1.0f, 1.0f, 10.0f), glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
	//CHECK_EQ(f.point_inside(glm::vec3(0.0f, 0.0f, 1.5f)), false);
	//CHECK_EQ(f.point_inside(glm::vec3(0.0f, 0.0f, 0.5f)), true);
	//CHECK_EQ(f.cube_inside(glm::vec3(0.0f, 0.0f, -3.125f), 1.0f, 1.0f, 1.0f), true);
}
