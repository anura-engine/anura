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

#include <stack>

#include "asserts.hpp"
#include "ModelMatrixScope.hpp"

namespace KRE
{
	namespace 
	{
		// These are purely for 2D
		std::stack<glm::vec2>& get_translation_stack()
		{
			static std::stack<glm::vec2> res;
			return res;
		}

		std::stack<float>& get_rotation_stack()
		{
			static std::stack<float> res;
			return res;
		}

		std::stack<glm::vec2>& get_scale_stack()
		{
			static std::stack<glm::vec2> res;
			return res;
		}

		glm::mat4& get_model_matrix() 
		{
			static glm::mat4 res(1.0f);
			return res;
		}

		static bool model_matrix_changed = true;
	}

	ModelManager2D::ModelManager2D()
	{
		if(get_translation_stack().empty()) {
			get_translation_stack().emplace(0.0f, 0.0f);
		} else {
			auto top = get_translation_stack().top();
			get_translation_stack().emplace(top.x, top.y);
		}

		if(get_rotation_stack().empty()) {
			get_rotation_stack().emplace(0.0f);
		} else {
			auto top = get_rotation_stack().top();
			get_rotation_stack().emplace(top);
		}

		if(get_scale_stack().empty()) {
			get_scale_stack().emplace(1.0f, 1.0f);
		} else {
			auto top = get_scale_stack().top();
			get_scale_stack().emplace(top.x, top.y);
		}
	}

	ModelManager2D::ModelManager2D(int tx, int ty, float angle, float scale)
	{
		if(get_translation_stack().empty()) {
			get_translation_stack().emplace(static_cast<float>(tx), static_cast<float>(ty));
		} else {
			auto top = get_translation_stack().top();
			get_translation_stack().emplace(static_cast<float>(tx) + top.x, static_cast<float>(ty) + top.y);
		}

		if(get_rotation_stack().empty()) {
			get_rotation_stack().emplace(glm::radians(angle));
		} else {
			auto top = get_rotation_stack().top();
			get_rotation_stack().emplace(glm::radians(angle) + top);
		}

		if(get_scale_stack().empty()) {
			get_scale_stack().emplace(scale, scale);
		} else {
			auto top = get_scale_stack().top();
			get_scale_stack().emplace(scale * top.x, scale * top.y);
		}
		model_matrix_changed = true;
	}

	ModelManager2D::ModelManager2D(int tx, int ty, float angle, const glm::vec2& scale)
	{
		if(get_translation_stack().empty()) {
			get_translation_stack().emplace(static_cast<float>(tx), static_cast<float>(ty));
		} else {
			auto top = get_translation_stack().top();
			get_translation_stack().emplace(static_cast<float>(tx) + top.x, static_cast<float>(ty) + top.y);
		}

		if(get_rotation_stack().empty()) {
			get_rotation_stack().emplace(glm::radians(angle));
		} else {
			auto top = get_rotation_stack().top();
			get_rotation_stack().emplace(glm::radians(angle) + top);
		}

		if(get_scale_stack().empty()) {
			get_scale_stack().emplace(scale.x, scale.y);
		} else {
			auto top = get_scale_stack().top();
			get_scale_stack().emplace(scale.x * top.x, scale.y * top.y);
		}
		model_matrix_changed = true;
	}

	ModelManager2D::~ModelManager2D() 
	{
		ASSERT_LOG(get_translation_stack().empty() == false, "Unbalanced translation stack.");
		ASSERT_LOG(get_rotation_stack().empty() == false, "Unbalanced rotation stack.");
		ASSERT_LOG(get_scale_stack().empty() == false, "Unbalanced scale stack.");

		get_translation_stack().pop();
		get_rotation_stack().pop();
		get_scale_stack().pop();
		model_matrix_changed = true;
	}

	void ModelManager2D::setIdentity()
	{
		auto& top = get_translation_stack().top();
		top.x = top.y = 0.0f;

		get_rotation_stack().top() = 0.0f;

		auto& scale = get_scale_stack().top();
		scale.x = scale.y = 1.0f;

		model_matrix_changed = true;
	}

	void ModelManager2D::translate(int tx, int ty)
	{
		if(get_translation_stack().empty()) {
			get_translation_stack().emplace(static_cast<float>(tx), static_cast<float>(ty));
		} else {
			auto& top = get_translation_stack().top();
			top.x += static_cast<float>(tx);
			top.y += static_cast<float>(ty);
		}
		model_matrix_changed = true;
	}

	void ModelManager2D::rotate(float angle)
	{
		if(get_rotation_stack().empty()) {
			get_rotation_stack().emplace(glm::radians(angle));
		} else {
			get_rotation_stack().top() += glm::radians(angle);
		}
		model_matrix_changed = true;
	}

	void ModelManager2D::scale(float sx, float sy)
	{
		if(get_scale_stack().empty()) {
			get_scale_stack().emplace(sx, sy);
		} else {
			auto& top = get_scale_stack().top();
			top.x *= sx;
			top.y *= sy;
		}
		model_matrix_changed = true;
	}

	void ModelManager2D::scale(float s)
	{
		if(get_scale_stack().empty()) {
			get_scale_stack().emplace(s, s);
		} else {
			auto& top = get_scale_stack().top();
			top.x *= s;
			top.y *= s;
		}
		model_matrix_changed = true;
	}

	bool is_global_model_matrix_valid()
	{
		//return !get_translation_stack().empty() || !get_rotation_stack().empty() || !get_scale_stack().empty();
		return true;
	}

	const glm::mat4& get_global_model_matrix()
	{
		if(model_matrix_changed) {
			model_matrix_changed = false;
			get_model_matrix() = glm::mat4(1.0f);

			if(!get_scale_stack().empty()) {
				auto& top = get_scale_stack().top();
				get_model_matrix() = glm::scale(get_model_matrix(), glm::vec3(top.x, top.y, 1.0f));
			}

			if(!get_rotation_stack().empty()) {
				get_model_matrix() = glm::rotate(get_model_matrix(), get_rotation_stack().top(), glm::vec3(0.0f, 0.0f, 1.0f));
			}

			if(!get_translation_stack().empty()) {
				auto& top = get_translation_stack().top();
				get_model_matrix() = glm::translate(get_model_matrix(), glm::vec3(top, 0.0f));
			}
		}
		return get_model_matrix();
	}

	glm::mat4 set_global_model_matrix(const glm::mat4& m)
	{
		glm::mat4 current_matrix = get_model_matrix();
		get_model_matrix() = m;
		return current_matrix;
	}
}
