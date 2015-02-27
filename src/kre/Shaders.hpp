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
		enum {
			INALID_UNIFORM		= -1,
			INALID_ATTRIBUTE	= -1,
		};
		ShaderProgram(const variant& node);
		virtual ~ShaderProgram();

		virtual void makeActive() = 0;
		virtual void applyAttribute(AttributeBasePtr attr) = 0;
		//virtual void applyUniformSet(UniformSetPtr uniforms) = 0;
		virtual void cleanUpAfterDraw() = 0;

		virtual int getAttributeOrDie(const std::string& attr) const = 0;
		virtual int getUniformOrDie(const std::string& attr) const = 0;

		virtual int getAttribute(const std::string& attr) const = 0;
		virtual int getUniform(const std::string& attr) const = 0;

		virtual void setUniformValue(int uid, const int) const = 0;
		virtual void setUniformValue(int uid, const float) const = 0;
		virtual void setUniformValue(int uid, const float*) const = 0;
		virtual void setUniformValue(int uid, const int*) const = 0;
		virtual void setUniformValue(int uid, const void*) const = 0;

		virtual void configureActives(AttributeSetPtr attrset) = 0;
		virtual void configureUniforms(UniformBufferBase& uniforms) = 0;

		virtual int getColorUniform() const = 0;
		virtual int getLineWidthUniform() const = 0;
		virtual int getMvUniform() const = 0;
		virtual int getPUniform() const = 0;
		virtual int getMvpUniform() const = 0;
		virtual int getTexMapUniform() const = 0;

		virtual int getColorAttribute() const = 0;
		virtual int getVertexAttribute() const = 0;
		virtual int getTexcoordAttribute() const = 0;
		virtual int getNormalAttribute() const = 0;

		virtual void setUniformsForTexture(const TexturePtr& tex) const = 0;

		static ShaderProgramPtr getSystemDefault();

		//! Look-up the given shader program name in the list and return it.
		static ShaderProgramPtr getProgram(const std::string& name);

		//! loads the internal store of shader programs from the given data.
		static void loadFromVariant(const variant& node);
	private:
		DISALLOW_COPY_ASSIGN_AND_DEFAULT(ShaderProgram);
	};
}
