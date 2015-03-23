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

#include "SceneObject.hpp"
#include "Util.hpp"

namespace KRE
{
	enum class LightType {
		POINT,
		DIRECTIONAL,
		SPOT,
	};

	class Light : public SceneObject
	{
	public:
		explicit Light(const std::string& name, const glm::vec3& position);
		explicit Light(const variant& node);
		virtual ~Light();
		void setType(LightType type);
		void setPosition(const glm::vec3& position);
		void setAmbientColor(const Color& color);
		void setDiffuseColor(const Color& color);
		void setSpecularColor(const Color& color);
		void setSpotDirection(const glm::vec3& direction);
		void setSpotExponent(float sexp);
		void setSpotCutoff(float cutoff);
		void setAttenuation(float constant, float linear, float quadratic);
		LightType getType() const { return type_; }
		const glm::vec3 getPosition() const { return position_; }
		const Color getAmbientColor() const { return ambient_color_; }
		const Color getDiffuseColor() const { return diffuse_color_; }
		const Color getSpecularColor() const { return specular_color_; }
		const glm::vec3 getSpotDirection() const { return spot_direction_; }
		float getSpotExponent() const { return spot_exponent_; }
		float getSpotCutoff() const { return spot_cutoff_; }
		float getConstantAttenuation() const { return constant_attenuation_; }
		float getLinearAttenuation() const { return linear_attenuation_; }
		float getQuadraticAttenuation() const { return quadratic_attenuation_; }
		variant write() const;
		LightPtr clone();
	private:
		LightType type_;
		glm::vec3 position_;
		Color ambient_color_;
		Color diffuse_color_;
		Color specular_color_;
		glm::vec3 spot_direction_;
		float spot_exponent_;
		float spot_cutoff_;
		float constant_attenuation_;
		float linear_attenuation_;
		float quadratic_attenuation_;

		Light();
		void operator=(const Light&);
	};
}
