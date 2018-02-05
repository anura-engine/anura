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

#include <string>
#include <vector>
#include <unordered_map>

#include "geometry.hpp"
#include "Texture.hpp"
#include "variant.hpp"

class GraphicalFont;
typedef std::shared_ptr<GraphicalFont> GraphicalFontPtr;
typedef std::shared_ptr<const GraphicalFont> ConstGraphicalFontPtr;

class GraphicalFont
{
public:
	static void init(variant node);
	static void initForLocale(const std::string& locale);
	static ConstGraphicalFontPtr get(const std::string& id);
	explicit GraphicalFont(variant node);
	const std::string& id() const { return id_; }
	const std::string& texture_file() const { return texture_file_; }
	rect draw(int x, int y, const std::string& text, int size=2, const KRE::Color& color=KRE::Color::colorWhite()) const;
	rect dimensions(const std::string& text, int size=2) const;

	const rect& get_codepoint_area(unsigned int codepoint) const;

private:
	rect doDraw(int x, int y, const std::string& text, bool draw_text, int size, const KRE::Color& color) const;

	std::string id_;

	std::string texture_file_;

	KRE::TexturePtr texture_;
	//hashmap to map characters to rectangles in the texture
	typedef std::unordered_map<unsigned int, rect> char_rect_map;
	char_rect_map char_rect_map_;
	int kerning_;
};
