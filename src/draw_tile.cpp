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

#include <algorithm>

#include "asserts.hpp"
#include "draw_tile.hpp"
#include "level_object.hpp"

extern int g_tile_scale;
extern int g_tile_size;
#define BaseTileSize g_tile_size

int get_tile_corners(std::vector<tile_corner>* result, const KRE::TexturePtr& t, const rect& area, int tile_num, int x, int y, bool reverse)
{
	if(tile_num < 0 || area.w() <= 0 || area.h() <= 0 || area.x() < 0 || area.y() < 0) {
		return 0;
	}

	const int width = std::max<int>(t->width(), t->height());
	if (width == 0) return 0;
	
	const int xpos = BaseTileSize*(tile_num%(width/BaseTileSize)) + area.x();
	const int ypos = BaseTileSize*(tile_num/(width/BaseTileSize)) + area.y();

	rectf coords = rectf::from_coordinates(t->getTextureCoordW(0, xpos + 0.0001f),
		t->getTextureCoordH(0, ypos + 0.0001f),
		t->getTextureCoordW(0, xpos + area.w()),
		t->getTextureCoordH(0, ypos + area.h()));

	int area_x = area.x()*g_tile_scale;
	if(reverse) {
		coords = rectf::from_coordinates(coords.x2(), coords.y(), coords.x(), coords.y2());
		area_x = (BaseTileSize * g_tile_scale) - (area.x() - area.w()) * g_tile_scale;
	}

	const unsigned short x2 = x + area_x + area.w() * g_tile_scale;
	const unsigned short y2 = y + (area.y() + area.h()) * g_tile_scale;

	result->emplace_back(glm::u16vec2(x,y), glm::vec2(coords.x(), coords.y()));
	result->emplace_back(glm::u16vec2(x,y2), glm::vec2(coords.x(), coords.y2()));
	result->emplace_back(glm::u16vec2(x2,y), glm::vec2(coords.x2(), coords.y()));

	result->emplace_back(glm::u16vec2(x,y2), glm::vec2(coords.x(), coords.y2()));
	result->emplace_back(glm::u16vec2(x2,y), glm::vec2(coords.x2(), coords.y()));
	result->emplace_back(glm::u16vec2(x2,y2), glm::vec2(coords.x2(), coords.y2()));

	return 6;
}

bool is_tile_opaque(const KRE::TexturePtr& t, int tile_num)
{
	const int width = std::max<int>(t->width(), t->height());
	const int xpos = BaseTileSize*(tile_num%(width/BaseTileSize));
	const int ypos = BaseTileSize*(tile_num/(width/BaseTileSize));
	for(int y = 0; y != BaseTileSize; ++y) {
		const int v = ypos + y;
		for(int x = 0; x != BaseTileSize; ++x) {
			const int u = xpos + x;
			if(t->getFrontSurface()->isAlpha(u, v)) {
				return false;
			}
		}
	}
	
	return true;
}

bool is_tile_using_alpha_channel(const KRE::TexturePtr& t, int tile_num)
{
	const int width = std::max<int>(t->width(), t->height());
	const int xpos = BaseTileSize*(tile_num%(width/BaseTileSize));
	const int ypos = BaseTileSize*(tile_num/(width/BaseTileSize));
	for(int y = 0; y != BaseTileSize; ++y) {
		const int v = ypos + y;
		for(int x = 0; x != BaseTileSize; ++x) {
			const int u = xpos + x;
			const unsigned char* color = t->colorAt(u, v);
			ASSERT_LOG(color != nullptr, "COULD NOT FIND COLOR IN TEXTURE");
			KRE::Color new_color(color[0], color[1], color[2], color[3]);
			if(new_color.a() != 0 && new_color.a() != 0xFF) {
				return true;
			}
		}
	}

	return false;
}

bool is_tile_solid_color(const KRE::TexturePtr& t, int tile_num, KRE::Color& col)
{
	bool first = true;
	const int width = std::max<int>(t->width(), t->height());
	const int xpos = BaseTileSize*(tile_num%(width/BaseTileSize));
	const int ypos = BaseTileSize*(tile_num/(width/BaseTileSize));
	for(int y = 0; y != BaseTileSize; ++y) {
		const int v = ypos + y;
		for(int x = 0; x != BaseTileSize; ++x) {
			const int u = xpos + x;
			const unsigned char* color = t->colorAt(u, v);
			ASSERT_LOG(color != nullptr, "COULD NOT FIND COLOR IN TEXTURE");
			KRE::Color new_color(color[0], color[1], color[2], color[3]);
			if(new_color.a() != 0xFF) {
				return false;
			}

			if(first || col == new_color) {
				col = new_color;
				first = false;
			} else {
				return false;
			}
		}
	}
	
	return true;
}

rect get_tile_non_alpha_area(const KRE::TexturePtr& t, int tile_num)
{
	const int width = std::max<int>(t->width(), t->height());
	const int xpos = BaseTileSize*(tile_num%(width/BaseTileSize));
	const int ypos = BaseTileSize*(tile_num/(width/BaseTileSize));
	int top = -1, bottom = -1, left = -1, right = -1;

	for(int y = 0; y != BaseTileSize && top == -1; ++y) {
		const int v = ypos + y;
		for(int x = 0; x != BaseTileSize; ++x) {
			const int u = xpos + x;
			if(!t->getFrontSurface()->isAlpha(u, v)) {
				top = y;
				break;
			}
		}
	}

	for(int y = BaseTileSize-1; y != -1 && bottom == -1; --y) {
		const int v = ypos + y;
		for(int x = 0; x != BaseTileSize; ++x) {
			const int u = xpos + x;
			if(!t->getFrontSurface()->isAlpha(u, v)) {
				bottom = y + 1;
				break;
			}
		}
	}
	
	for(int x = 0; x != BaseTileSize && left == -1; ++x) {
		const int u = xpos + x;
		for(int y = 0; y != BaseTileSize; ++y) {
			const int v = ypos + y;
			if(!t->getFrontSurface()->isAlpha(u, v)) {
				left = x;
				break;
			}
		}
	}

	for(int x = BaseTileSize-1; x != -1 && right == -1; --x) {
		const int u = xpos + x;
		for(int y = 0; y != BaseTileSize; ++y) {
			const int v = ypos + y;
			if(!t->getFrontSurface()->isAlpha(u, v)) {
				right = x + 1;
				break;
			}
		}
	}

	if(right <= left || bottom <= top) {
		return rect();
	}

	return rect(left, top, right - left, bottom - top);
}
