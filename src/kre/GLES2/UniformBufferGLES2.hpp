/*
	Copyright (C) 2013-2016 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "SDL_opengles2.h"

#include "Shaders.hpp"
#include "UniformBuffer.hpp"

namespace KRE
{
	// Implementation when we have hardware uniform buffer objects
	class UniformHardwareGLESv2 : public UniformHardwareInterface
	{
	public:
		explicit UniformHardwareGLESv2(const std::string& name);
		~UniformHardwareGLESv2();
		void update(void* buffer, int size) override;
		void mapToShader(ShaderProgramPtr shader);
	private:
		UniformHardwareGLESv2();
		GLuint ubo_;
	};
}
