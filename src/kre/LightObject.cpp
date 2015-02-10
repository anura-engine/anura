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

#include "DisplayDevice.hpp"
#include "LightObject.hpp"
#include "variant_utils.hpp"

namespace KRE
{
	namespace
	{
		const Color default_ambient_color = Color::colorBlack();
		const Color default_diffuse_color = Color::colorWhite();
		const Color default_specular_color = Color::colorWhite();
		const glm::vec3 default_spot_direction(0.0f,0.0f,-1.0f);
		const float default_spot_exponent = 0.0f;
		const float default_spot_cutoff = 180.0f;
		const float default_constant_attenuation = 1.0f;
		const float default_linear_attenuation = 0.0f;
		const float default_quadratic_attenuation = 0.0f;
	}

	Light::Light(const std::string& name, const glm::vec3& position)
		: SceneObject(name),
		type_(LightType::POINT),
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

	Light::Light(const variant& node)
		: SceneObject(node["name"].as_string_default("light")),
		type_(LightType::POINT),
		position_(),
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
		if(node.has_key("type")) {
			const std::string& type = node["type"].as_string();
			if(type == "point") {
				type_ = LightType::POINT;
			} else if(type == "directional") {
				type_ = LightType::DIRECTIONAL;
			} else if(type == "spot") {
				type_ = LightType::SPOT;
			}
		}
		if(node.has_key("position")) {
			position_ = variant_to_vec3(node["position"]);
		} else if(node.has_key("translation")) {
			position_ = variant_to_vec3(node["translation"]);
		}
		if(node.has_key("ambient_color")) {
			ambient_color_ = Color(node["ambient_color"]);
		}
		if(node.has_key("diffuse_color")) {
			diffuse_color_ = Color(node["diffuse_color"]);
		}
		if(node.has_key("specular_color")) {
			specular_color_ = Color(node["specular_color"]);
		}
		if(node.has_key("spot_direction")) {
			spot_direction_ = variant_to_vec3(node["spot_direction"]);
		}
		if(node.has_key("spot_exponent")) {
			spot_exponent_ = node["spot_exponent"].as_float();
		}
		if(node.has_key("spot_cutoff")) {
			spot_cutoff_ = node["spot_cutoff"].as_float();
		}
		if(node.has_key("constant_attenuation")) {
			constant_attenuation_ = node["constant_attenuation_"].as_float();
		}
		if(node.has_key("linear_attenuation")) {
			linear_attenuation_ = node["linear_attenuation"].as_float();
		}
		if(node.has_key("quadratic_attenuation")) {
			quadratic_attenuation_ = node["quadratic_attenuation"].as_float();
		}
	}

	Light::~Light()
	{
	}

	void Light::setType(LightType type)
	{
		type_ = type;
	}

	void Light::setPosition(const glm::vec3& position)
	{
		position_ = position;
	}

	void Light::setAmbientColor(const Color& color)
	{
		ambient_color_ = color;
	}

	void Light::setDiffuseColor(const Color& color)
	{
		diffuse_color_ = color;
	}

	void Light::setSpecularColor(const Color& color)
	{
		specular_color_ = color;
	}

	void Light::setSpotDirection(const glm::vec3& direction)
	{
		spot_direction_ = direction;
	}

	void Light::setSpotExponent(float sexp)
	{
		spot_exponent_ = sexp;
	}

	void Light::setSpotCutoff(float cutoff)
	{
		spot_cutoff_ = cutoff;
	}

	void Light::setAttenuation(float constant, float linear, float quadratic)
	{
		constant_attenuation_ = constant;
		linear_attenuation_ = linear;
		quadratic_attenuation_ = quadratic;
	}

	LightPtr Light::clone()
	{
		auto lp = std::make_shared<Light>(*this);
		return lp;
	}

	variant Light::write() const
	{
		variant_builder res;
		res.add("name", objectName());
		res.add("position", position_.x);
		res.add("position", position_.y);
		res.add("position", position_.z);
		switch(type_) {
			case LightType::POINT:			res.add("type", "point"); break;
			case LightType::DIRECTIONAL:	res.add("type", "directional"); break;
			case LightType::SPOT:			res.add("type", "spot"); break;
		}
		if(ambient_color_ != default_ambient_color) {
			res.add("ambient_color", ambient_color_.write());
		}
		if(diffuse_color_ != default_diffuse_color) {
			res.add("diffuse_color", diffuse_color_.write());
		}
		if(specular_color_ != default_specular_color) {
			res.add("specular_color", specular_color_.write());
		}
		if(spot_direction_ != default_spot_direction) {
			res.add("spot_direction", vec3_to_variant(spot_direction_));
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
		return res.build();
	}
}
