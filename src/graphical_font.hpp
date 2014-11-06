/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#ifndef GRAPHICAL_FONT_HPP_INCLUDED
#define GRAPHICAL_FONT_HPP_INCLUDED

#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

#include <string>
#include <vector>

#include "geometry.hpp"
#include "texture.hpp"
#include "variant.hpp"

class graphical_font;
typedef boost::shared_ptr<graphical_font> graphical_font_ptr;
typedef boost::shared_ptr<const graphical_font> const_graphical_font_ptr;

class graphical_font
{
public:
	static void init(variant node);
	static void init_for_locale(const std::string& locale);
	static const_graphical_font_ptr get(const std::string& id);
	explicit graphical_font(variant node);
	const std::string& id() const { return id_; }
	rect draw(int x, int y, const std::string& text, int size=2) const;
	rect dimensions(const std::string& text, int size=2) const;

private:
	rect do_draw(int x, int y, const std::string& text, bool draw_text, int size) const;

	std::string id_;

	graphics::texture texture_;
	//hashmap to map characters to rectangles in the texture
	typedef boost::unordered_map<unsigned int, rect> char_rect_map;
	char_rect_map char_rect_map_;
	int kerning_;
};

#endif
