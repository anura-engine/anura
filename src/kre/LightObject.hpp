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

#pragma once

#include "SceneObject.hpp"

namespace KRE
{
	class Light : public SceneObject
	{
	public:
		enum LightType {
			LT_POINT,
			LT_DIRECTIONAL,
			LT_SPOT,
		};
		explicit Light(const std::string& name, const glm::vec3& position);
		//explicit Light(const variant& node);
		virtual ~Light();
		DisplayDeviceDef Attach(const DisplayDevicePtr& dd) override;
		void SetType(LightType type);
		void SetPosition(const glm::vec3& position);
		void SetAmbientColor(const glm::vec4& color);
		void SetDiffuseColor(const glm::vec4& color);
		void SetSpecularColor(const glm::vec4& color);
		void SetSpotDirection(const glm::vec3& direction);
		void SetSpotExponent(float sexp);
		void SetSpotCutoff(float cutoff);
		void SetAttenuation(float constant, float linear, float quadratic);
		LightType Type() const { return type_; }
		const glm::vec3 Position() const { return position_; }
		const glm::vec4 AmbientColor() const { return ambient_color_; }
		const glm::vec4 DiffuseColor() const { return diffuse_color_; }
		const glm::vec4 SpecularColor() const { return specular_color_; }
		const glm::vec3 SpotDirection() const { return spot_direction_; }
		float SpotExponent() const { return spot_exponent_; }
		float SpotCutoff() const { return spot_cutoff_; }
		float ConstantAttenuation() const { return constant_attenuation_; }
		float LinearAttenuation() const { return linear_attenuation_; }
		float QuadraticAttenuation() const { return quadratic_attenuation_; }
		//variant write() const;
	private:
		LightType type_;
		glm::vec3 position_;
		glm::vec4 ambient_color_;
		glm::vec4 diffuse_color_;
		glm::vec4 specular_color_;
		glm::vec3 spot_direction_;
		float spot_exponent_;
		float spot_cutoff_;
		float constant_attenuation_;
		float linear_attenuation_;
		float quadratic_attenuation_;
		Light();
		Light(const Light&);
		Light& operator=(const Light&);
	};
}
