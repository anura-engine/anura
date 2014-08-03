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

#include "kre/Geometry.hpp"
#include "kre/Texture.hpp"
#include "variant.hpp"

class GuiSection;
typedef std::shared_ptr<const GuiSection> ConstGuiSectionPtr;

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
