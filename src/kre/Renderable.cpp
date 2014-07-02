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

#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "DisplayDevice.hpp"
#include "Renderable.hpp"

namespace KRE
{
	Renderable::Renderable()
		: order_(0),
		position_(0.0f),
		rotation_(1.0f, 0.0f, 0.0f, 0.0f),
		scale_(1.0f),
		color_set_(false)
	{
	}

	Renderable::Renderable(size_t order)
		: order_(order), 
		position_(0.0f),
		rotation_(1.0f, 0.0f, 0.0f, 0.0f),
		scale_(1.0f),
		color_set_(false)
	{
	}

	Renderable::~Renderable()
	{
	}

	void Renderable::SetPosition(const glm::vec3& position) 
	{
		position_ = position;
	}

	void Renderable::SetPosition(float x, float y, float z) 
	{
		position_ = glm::vec3(x, y, z);
	}

	void Renderable::SetPosition(int x, int y, int z) 
	{
		position_ = glm::vec3(float(x), float(y), float(z));
	}

	void Renderable::SetRotation(float angle, const glm::vec3& axis) 
	{
		rotation_ = glm::angleAxis(angle, axis);
	}

	void Renderable::SetRotation(const glm::quat& rot) 
	{
		rotation_ = rot;
	}

	void Renderable::SetScale(float xs, float ys, float zs) 
	{
		scale_ = glm::vec3(xs, ys, zs);
	}

	void Renderable::SetScale(const glm::vec3& scale) 
	{
		scale_ = scale;
	}

	glm::mat4 Renderable::ModelMatrix() const 
	{
		return glm::translate(glm::mat4(1.0f), position_) * glm::toMat4(rotation_) * glm::scale(glm::mat4(1.0f), scale_);
	}

	void Renderable::SetCamera(const CameraPtr& camera)
	{
		camera_ = camera;
	}

	void Renderable::SetLights(const LightPtrList& lights)
	{
		lights_ = lights;
	}

	void Renderable::SetMaterial(const MaterialPtr& material)
	{
		material_ = material;
	}

	void Renderable::SetRenderTarget(const RenderTargetPtr& rt)
	{
		render_target_ = rt;
	}
	
	void Renderable::SetColor(float r, float g, float b, float a)
	{
		color_set_ = true;
		color_ = Color(r, g, b, a);
	}

	void Renderable::SetColor(int r, int g, int b, int a)
	{
		color_set_ = true;
		color_ = Color(r, g, b, a);
	}

	void Renderable::SetColor(const Color& color)
	{
		color_set_ = true;
		color_ = color;
	}

	void Renderable::SetDisplayData(const DisplayDevicePtr& dd, const DisplayDeviceDef& def)
	{
		display_data_ = dd->CreateDisplayDeviceData(def);
	}

	void Renderable::AddAttributeSet(const AttributeSetPtr& attrset)
	{
		attributes_.emplace_back(attrset);
	}

	//void Renderable::AddUniformSet(const UniformSetPtr& uniset)
	//{
	//	uniforms_.emplace_back(uniset);
	//}
}
