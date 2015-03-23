/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#include <math.h>

#include "DisplayDevice.hpp"
#include "Shaders.hpp"

#include "custom_object.hpp"
#include "formatter.hpp"
#include "light.hpp"
#include "variant_utils.hpp"

namespace 
{
	int fade_length = 64;
}

LightPtr Light::createLight(const CustomObject& obj, variant node)
{
	if(node["type"].as_string() == "circle") {
		return LightPtr(new CircleLight(obj, node));
	} else {
		return LightPtr();
	}
}

Light::Light(const CustomObject& obj, variant node) 
	: SceneObjectCallable(node), 
	obj_(obj)
{
}

Light::~Light() 
{
}

BEGIN_DEFINE_CALLABLE(Light, SceneObjectCallable)
	DEFINE_FIELD(dummy, "null")
		return variant();
END_DEFINE_CALLABLE(Light)

CircleLight::CircleLight(const CustomObject& obj, variant node)
  : Light(obj, node), 
  center_(obj.getMidpoint()), 
  radius_(node["radius"].as_int())
{
	init();
}

CircleLight::CircleLight(const CustomObject& obj, int radius)
  : Light(obj, variant()), 
  center_(obj.getMidpoint()), 
  radius_(radius)
{
	init();
}

void CircleLight::init()
{
	using namespace KRE;
	clearAttributeSets();

	setShader(KRE::ShaderProgram::getProgram("attr_color_shader"));

	auto as_fan = DisplayDevice::createAttributeSet(false, false, false);
	fan_.reset(new Attribute<glm::vec2>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
	fan_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false));
	as_fan->addAttribute(AttributeBasePtr(fan_));
	as_fan->setDrawMode(DrawMode::TRIANGLE_FAN);
	addAttributeSet(as_fan);

	auto as_sq = DisplayDevice::createAttributeSet(false, false, false);
	sq_.reset(new Attribute<vertex_color>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
	sq_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_color), offsetof(vertex_color,vertex)));
	sq_->addAttributeDesc(AttributeDesc(AttrType::COLOR, 4, AttrFormat::UNSIGNED_BYTE, true, sizeof(vertex_color), offsetof(vertex_color,color)));
	as_sq->addAttribute(AttributeBasePtr(sq_));
	as_sq->setDrawMode(DrawMode::TRIANGLE_STRIP);
	addAttributeSet(as_sq);

	updateVertices();
}

void CircleLight::preRender(const KRE::WindowPtr& wnd)
{
	if(getColor() != last_color_) {
		updateVertices();
	}
}

void CircleLight::updateVertices()
{
	std::vector<glm::vec2> varray;

	static std::vector<float> x_angles;
	static std::vector<float> y_angles;

	if(x_angles.empty()) {
		for(float angle = 0.0f; angle < 3.145926535f*2.0f; angle += 0.2f) {
			x_angles.push_back(cos(angle));
			y_angles.push_back(sin(angle));
		}
	}

	const float x = static_cast<float>(center_.x);
	const float y = static_cast<float>(center_.y);

	varray.emplace_back(x,y);
	for(unsigned n = 0; n != x_angles.size(); ++n) {
		const float xpos = x + radius_*x_angles[n];
		const float ypos = y + radius_*y_angles[n];
		varray.emplace_back(xpos,ypos);
	}
	varray.emplace_back(varray[1]);
	fan_->update(&varray);

	std::vector<KRE::vertex_color> vc_array;
	KRE::Color c1 = getColor(); c1.setAlpha(255);
	KRE::Color c2 = getColor(); c2.setAlpha(0);
	glm::u8vec4 col1 = c1.as_u8vec4();
	glm::u8vec4 col2 = c2.as_u8vec4();
	for(int n = 0; n != x_angles.size(); ++n) {
		const float xpos = x + radius_*x_angles[n];
		const float ypos = y + radius_*y_angles[n];
		const float xpos2 = x + (radius_+fade_length)*x_angles[n];
		const float ypos2 = y + (radius_+fade_length)*y_angles[n];
		vc_array.emplace_back(glm::vec2(xpos, ypos), col1);
		vc_array.emplace_back(glm::vec2(xpos2, ypos2), col2);
	}
	vc_array.emplace_back(vc_array[1]);
	vc_array.emplace_back(vc_array[2]);
	sq_->update(&vc_array);

	last_color_ = getColor();
}

variant CircleLight::write() const
{
	variant_builder res;
	res.add("type", "circle");
	res.add("radius", radius_);

	return res.build();
}

void CircleLight::process()
{
	center_ = object().getMidpoint();
}

bool CircleLight::onScreen(const rect& screen_area) const
{
	return true;
}

BEGIN_DEFINE_CALLABLE(CircleLight, Light)
	DEFINE_FIELD(center, "[int,int]")
		std::vector<variant> v;
		v.emplace_back(obj.center_.x);
		v.emplace_back(obj.center_.y);
		return variant(&v);
	DEFINE_SET_FIELD
		obj.center_.x = value[0].as_int();
		obj.center_.y = value[0].as_int();
		obj.updateVertices();

	DEFINE_FIELD(radius, "int")
		return variant(obj.radius_);
	DEFINE_SET_FIELD
		obj.radius_ = value.as_int();
		obj.updateVertices();
END_DEFINE_CALLABLE(CircleLight)


LightFadeLengthSetter::LightFadeLengthSetter(int value)
  : old_value_(fade_length)
{
	fade_length = value;
}

LightFadeLengthSetter::~LightFadeLengthSetter()
{
	fade_length = old_value_;
}
