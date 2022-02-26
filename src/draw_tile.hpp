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

#pragma once

#include <glm/glm.hpp>

#include "geometry.hpp"
#include "Texture.hpp"

#include "level_object_fwd.hpp"

struct tile_corner
{
	tile_corner(const glm::u16vec2& v, const glm::vec2& st) : vertex(v), uv(st) {}
	glm::u16vec2 vertex;
	glm::vec2 uv;
};

int get_tile_corners(std::vector<tile_corner>* result, const KRE::TexturePtr& t, const rect& area, int tile_num, int x, int y, bool reverse);

bool is_tile_using_alpha_channel(const KRE::TexturePtr& t, int tile_num);
bool is_tile_opaque(const KRE::TexturePtr& t, int tile_num);
bool is_tile_solid_color(const KRE::TexturePtr& t, int tile_num, KRE::Color& col);
rect get_tile_non_alpha_area(const KRE::TexturePtr& t, int tile_num);
