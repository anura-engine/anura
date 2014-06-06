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

#include "DisplayDevice.hpp"
#include "LightObject.hpp"

namespace KRE
{
	namespace
	{
		const glm::vec4 default_ambient_color(0.0f);
		const glm::vec4 default_diffuse_color(1.0f);
		const glm::vec4 default_specular_color(1.0f);
		const glm::vec3 default_spot_direction(0.0f,0.0f,-1.0f);
		const float default_spot_exponent = 0.0f;
		const float default_spot_cutoff = 180.0f;
		const float default_constant_attenuation = 1.0f;
		const float default_linear_attenuation = 0.0f;
		const float default_quadratic_attenuation = 0.0f;
	}

	Light::Light(const std::string& name, const glm::vec3& position)
		: SceneObject(name),
		type_(LT_POINT),
		position_(position),
		ambient_color_(default_ambient_color),
		diffuse_color_(default_diffuse_color),
		specular_color_(default_specular_color),
		spot_direction_(default_spot_direction),
		spot_exponent_(default_spot_exponent),
		spot_cutoff_(default_spot_cutoff),
		constant_attenuation_(default_constant_attenuation),
		linear_attenuation_(default_linear_attenuation),
		quadratic_attenuation_(default_quadratic_attenuation)
	{
	}

	Light::~Light()
	{
	}

	DisplayDeviceDef Light::Attach(const DisplayDevicePtr& dd)
	{
		DisplayDeviceDef def(GetAttributeSet());
		// XXX
		return def;
	}

	void Light::SetType(LightType type)
	{
		type_ = type;
	}

	void Light::SetPosition(const glm::vec3& position)
	{
		position_ = position;
	}

	void Light::SetAmbientColor(const glm::vec4& color)
	{
		ambient_color_ = color;
	}

	void Light::SetDiffuseColor(const glm::vec4& color)
	{
		diffuse_color_ = color;
	}

	void Light::SetSpecularColor(const glm::vec4& color)
	{
		specular_color_ = color;
	}

	void Light::SetSpotDirection(const glm::vec3& direction)
	{
		spot_direction_ = direction;
	}

	void Light::SetSpotExponent(float sexp)
	{
		spot_exponent_ = sexp;
	}

	void Light::SetSpotCutoff(float cutoff)
	{
		spot_cutoff_ = cutoff;
	}

	void Light::SetAttenuation(float constant, float linear, float quadratic)
	{
		constant_attenuation_ = constant;
		linear_attenuation_ = linear;
		quadratic_attenuation_ = quadratic;
	}

	/*variant Light::write() const
	{
		variant_builder res;
		res.add("name", name());
		res.add("position", position_.x);
		res.add("position", position_.y);
		res.add("position", position_.z);
		switch(type_) {
			case LT_POINT:			res.add("type", "point"); break;
			case LT_DIRECTIONAL:	res.add("type", "directional"); break;
			case LT_SPOT:			res.add("type", "spot"); break;
		}
		if(ambient_color_ != default_ambient_color) {
			res.add("ambient_color", ambient_color_.r);
			res.add("ambient_color", ambient_color_.g);
			res.add("ambient_color", ambient_color_.b);
			res.add("ambient_color", ambient_color_.a);
		}
		if(diffuse_color_ != default_diffuse_color) {
			res.add("diffuse_color", diffuse_color_.r);
			res.add("diffuse_color", diffuse_color_.g);
			res.add("diffuse_color", diffuse_color_.b);
			res.add("diffuse_color", diffuse_color_.a);
		}
		if(specular_color_ != default_specular_color) {
			res.add("specular_color", specular_color_.r);
			res.add("specular_color", specular_color_.g);
			res.add("specular_color", specular_color_.b);
			res.add("specular_color", specular_color_.a);
		}
		if(spot_direction_ != default_spot_direction) {
			res.add("spot_direction", spot_direction_.x);
			res.add("spot_direction", spot_direction_.y);
			res.add("spot_direction", spot_direction_.z);
		}
		if(spot_exponent_ != default_spot_exponent) {
			res.add("spot_exponent", spot_exponent_);
		}
		if(spot_cutoff_ != default_spot_cutoff) {
			res.add("spot_cutoff", spot_cutoff_);
		}
		if(constant_attenuation_ != default_constant_attenuation) {
			res.add("constant_attenuation", constant_attenuation_);
		}
		if(linear_attenuation_ != default_linear_attenuation) {
			res.add("linear_attenuation", linear_attenuation_);
		}
		if(quadratic_attenuation_ != default_quadratic_attenuation) {
			res.add("quadratic_attenuation", quadratic_attenuation_);
		}
	}*/
}
