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
#include <glm/gtc/matrix_transform.hpp>

namespace KRE
{
	class ModelManager2D
	{
	public:
		ModelManager2D();
		explicit ModelManager2D(int tx, int ty, float angle=0.0f, float scale=1.0f);
		explicit ModelManager2D(int tx, int ty, float angle, const glm::vec2& scale);
		~ModelManager2D();
		void setIdentity();
		void translate(int tx, int ty);
		void rotate(float angle);
		void scale(float sx, float sy);
		void scale(float s);
	};

	bool is_global_model_matrix_valid();
	const glm::mat4& get_global_model_matrix();
	glm::mat4 set_global_model_matrix(const glm::mat4& m);
}
