/*
	Copyright (C) 2003-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "Shaders.hpp"
#include "DisplayDevice.hpp"

namespace KRE
{
	ShaderProgram::ShaderProgram(const std::string& name, const variant& node)
		: name_(name),
		  node_(node)
	{
	}

	ShaderProgram::~ShaderProgram()
	{
	}

	ShaderProgramPtr ShaderProgram::getProgram(const std::string& name)
	{
		return DisplayDevice::getCurrent()->getShaderProgram(name);
	}

	void ShaderProgram::loadFromVariant(const variant& node)
	{
		DisplayDevice::getCurrent()->loadShadersFromVariant(node);
	}

	ShaderProgramPtr ShaderProgram::getSystemDefault()
	{
		return DisplayDevice::getCurrent()->getDefaultShader();
	}

	ShaderProgramPtr ShaderProgram::createShader(const std::string& name, 
		const std::vector<ShaderData>& shader_data, 
		const std::vector<ActiveMapping>& uniform_map,
		const std::vector<ActiveMapping>& attribute_map)
	{
		return DisplayDevice::getCurrent()->createShader(name, shader_data, uniform_map, attribute_map);
	}

	std::vector<float> generate_gaussian(float sigma, int radius)
	{
		std::vector<float> std_gaussian_weights;
		float weights_sum = 0;
		const float sigma_2_2 = 2.0f * sigma * sigma;
		const float term1 = (1.0f / std::sqrt(static_cast<float>(M_PI) * sigma_2_2));
		for(int n = 0; n < radius + 1; ++n) {
			std_gaussian_weights.emplace_back(term1 * std::exp(-static_cast<float>(n*n) / sigma_2_2));
			weights_sum += (n == 0 ? 1.0f : 2.0f) * std_gaussian_weights.back();
		}
		// normalise weights
		for(auto& weight : std_gaussian_weights) {
			weight /= weights_sum;
		}

		std::vector<float> res;
		for(auto it = std_gaussian_weights.crbegin(); it != std_gaussian_weights.crend(); ++it) {
			res.emplace_back(*it);
		}
		for(auto it = std_gaussian_weights.cbegin()+1; it != std_gaussian_weights.cend(); ++it) {
			res.emplace_back(*it);
		}

		/*std::stringstream ss;
		ss << "Gaussian(sigma=" << sigma << ", radius=" << radius << "):";
		for(auto it = std_gaussian_weights.crbegin(); it != std_gaussian_weights.crend(); ++it) {
			ss << " " << (*it);
		}
		for(auto it = std_gaussian_weights.cbegin()+1; it != std_gaussian_weights.cend(); ++it) {
			ss << " " << (*it);
		}
		LOG_DEBUG(ss.str());*/
		return res;
	}

	ShaderProgramPtr ShaderProgram::createGaussianShader(int radius)
	{
		return DisplayDevice::getCurrent()->createGaussianShader(radius);
	}
}
