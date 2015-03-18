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

#include <glm/gtc/type_precision.hpp>

namespace KRE
{
	struct vertex_color
	{
		vertex_color(const glm::vec2& v, const glm::u8vec4& c)
			: vertex(v), color(c) {}
		glm::vec2 vertex;
		glm::u8vec4 color;
	};

	struct vertex_texture_color
	{
		vertex_texture_color(const glm::vec2& v, const glm::vec2& t, const glm::u8vec4& c)
			: vertex(v), texcoord(t), color(c) {}
		glm::vec2 vertex;
		glm::vec2 texcoord;
		glm::u8vec4 color;
	};

	struct vertex_texcoord
	{
		vertex_texcoord() : vtx(0.0f), tc(0.0f) {}
		vertex_texcoord(const glm::vec2& v, const glm::vec2& c) : vtx(v), tc(c) {}
		glm::vec2 vtx;
		glm::vec2 tc;
	};

	struct short_vertex_color
	{
		short_vertex_color(const glm::u16vec2& v, const glm::u8vec4& c) : vertex(v), color(c) {}
		glm::u16vec2 vertex;
		glm::u8vec4 color;
	};

	struct short_vertex_texcoord
	{
		short_vertex_texcoord(const glm::i16vec2& v, const glm::vec2& t) : vertex(v), tc(t) {}
		glm::i16vec2 vertex;
		glm::vec2 tc;
	};
}
