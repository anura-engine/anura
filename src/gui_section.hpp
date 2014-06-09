/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef GUI_SECTION_HPP_INCLUDED
#define GUI_SECTION_HPP_INCLUDED

#include <string>

#include <boost/shared_ptr.hpp>

#include "kre/Geometry.hpp"
#include "kre/Texture.hpp"
#include "variant.hpp"

class GuiSection;
typedef boost::shared_ptr<const GuiSection> ConstGuiSectionPtr;

class GuiSection
{
public:
	static void init(variant node);
	static ConstGuiSectionPtr get(const std::string& key);
	static ConstGuiSectionPtr get(const variant& v);

	explicit GuiSection(variant node);

	void blit(int x, int y) const { blit(x, y, width(), height()); }
	void blit(int x, int y, int w, int h) const;
	int width() const { return area_.w()*2; }
	int height() const { return area_.h()*2; }

	static std::vector<std::string> getSections();
private:
	KRE::TexturePtr texture_;
	rect area_;
	rect draw_area_;

	int x_adjust_, y_adjust_, x2_adjust_, y2_adjust_;
};

#endif
