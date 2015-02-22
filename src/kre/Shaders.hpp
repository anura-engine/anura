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

#pragma once

#include <functional>
#include <exception>
#include <memory>
#include <string>

#include "DisplayDeviceFwd.hpp"
#include "Util.hpp"

#include "variant.hpp"

namespace KRE
{
	struct ShaderError : public std::runtime_error
	{
		ShaderError(const char* what) : std::runtime_error(what) {}
	};
	struct ShaderUniformError : public std::runtime_error
	{
		ShaderUniformError(const char* what) : std::runtime_error(what) {}
	};
	struct ShaderAttributeError : public std::runtime_error
	{
		ShaderAttributeError(const char* what) : std::runtime_error(what) {}
	};

	class ShaderProgram
	{
	public:
		ShaderProgram(const variant& node);
		virtual ~ShaderProgram();

		virtual void makeActive() = 0;
		virtual void applyAttribute(AttributeBasePtr attr) = 0;
		//virtual void applyUniformSet(UniformSetPtr uniforms) = 0;
		virtual void cleanUpAfterDraw() = 0;

		virtual void configureActives(AttributeSetPtr attrset) = 0;
		//virtual void configureUniforms(UniformSetPtr uniforms) = 0;

		static ShaderProgramPtr getSystemDefault();

		//! Look-up the given shader program name in the list and return it.
		static ShaderProgramPtr getProgram(const std::string& name);

		//! loads the internal store of shader programs from the given data.
		static void loadFromVariant(const variant& node);
	private:
		DISALLOW_COPY_ASSIGN_AND_DEFAULT(ShaderProgram);
	};
}
