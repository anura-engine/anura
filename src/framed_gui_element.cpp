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

#include <iostream>

#include "Canvas.hpp"
#include "DisplayDevice.hpp"

#include "asserts.hpp"
#include "framed_gui_element.hpp"
#include "variant_utils.hpp"

namespace 
{
	typedef std::map<std::string, ConstFramedGuiElementPtr> cache_map;
	cache_map cache;
}

using namespace KRE;

FramedGuiElement::FramedGuiElement(variant node)
	: area_(node["rect"]),
	cornerHeight_(node["corner_height"].as_int()),
	texture_(Texture::createTexture(node["image"]))
{
	top_left_corner_ = rect(area_.x(),area_.y(),cornerHeight_,cornerHeight_);
	top_right_corner_ = rect(area_.x2() - cornerHeight_,area_.y(),cornerHeight_,cornerHeight_);
	bottom_left_corner_ = rect(area_.x(),area_.y2() - cornerHeight_,cornerHeight_,cornerHeight_);
	bottom_right_corner_ = rect(area_.x2() - cornerHeight_,area_.y2() - cornerHeight_,cornerHeight_,cornerHeight_);
	
	top_border_ = rect(area_.x() + cornerHeight_, area_.y(),area_.w() - cornerHeight_ * 2, cornerHeight_);
	bottom_border_ = rect(area_.x() + cornerHeight_, area_.y2() - cornerHeight_,area_.w() - cornerHeight_ * 2, cornerHeight_);
	left_border_ = rect(area_.x(), area_.y() + cornerHeight_,cornerHeight_, area_.h() - cornerHeight_ * 2);
	right_border_ = rect(area_.x2() - cornerHeight_, area_.y() + cornerHeight_,cornerHeight_,area_.h() - cornerHeight_ * 2);
	
	interior_fill_ = rect(area_.x() + cornerHeight_, area_.y() + cornerHeight_,area_.w() - cornerHeight_ * 2,area_.h() - cornerHeight_ * 2);
}

FramedGuiElement::~FramedGuiElement()
{
}

std::vector<std::string> FramedGuiElement::getElements()
{
	std::vector<std::string> v;
	for(auto it : cache) {
		v.push_back(it.first);
	}
	return v;
}


void FramedGuiElement::init(variant node)
{
	for(auto obj : node["framed_gui_element"].as_list()) {
		const std::string& id = obj["id"].as_string();
		cache[id].reset(new FramedGuiElement(obj));
	}
}

ConstFramedGuiElementPtr FramedGuiElement::get(const std::string& key)
{
	cache_map::const_iterator itor = cache.find(key);
	ASSERT_LOG(itor != cache.end(), "Couldn't find gui_element named '" << key << "' in list");
	return itor->second;
}

void FramedGuiElement::blit(int x, int y, int w, int h, bool upscaled, const KRE::Color& color) const
{
	/*blit_subsection(interior_fill_,x,y,w/2,h/2,scale);
	
	blit_subsection(top_border_,x,y,w/2,top_border_.h(),scale);
	blit_subsection(bottom_border_,x,y + h - bottom_border_.h(),w/2,bottom_border_.h(),scale);
	blit_subsection(left_border_,x,y,left_border_.w(),h/2,scale);
	blit_subsection(right_border_,x + w - right_border_.w(), y,right_border_.w(),h/2,scale);
	
	blit_subsection(top_left_corner_,x,y,top_left_corner_.w(),top_left_corner_.h(),scale);
	blit_subsection(top_right_corner_,x + w - top_right_corner_.w(),y, top_right_corner_.w(), top_right_corner_.h(),scale);
	blit_subsection(bottom_left_corner_,x,y + h - bottom_left_corner_.h(),bottom_left_corner_.w(), bottom_left_corner_.h(),scale);
	blit_subsection(bottom_right_corner_,x + w - bottom_right_corner_.w(),y + h - bottom_right_corner_.h(),bottom_right_corner_.w(), bottom_right_corner_.h(),scale);*/
	
  //old code based on the assumption that scale=1 meant drawing to an 400x300 screen
	
	int scale = upscaled ? 2 : 1;
	
	blitSubsection(interior_fill_,x+ cornerHeight_,y+ cornerHeight_,w-2*cornerHeight_,h-2*cornerHeight_, color);
	
	blitSubsection(top_border_,x + cornerHeight_,y,w - cornerHeight_*2,top_border_.h()*scale, color);
	blitSubsection(bottom_border_,x + cornerHeight_,y + h - bottom_border_.h()*scale,w-cornerHeight_*2,bottom_border_.h()*scale, color);
	blitSubsection(left_border_,x,y+cornerHeight_,left_border_.w()*scale,h-2*cornerHeight_, color);
	blitSubsection(right_border_,x + w - right_border_.w()*scale, y+cornerHeight_,right_border_.w()*scale,h-2*cornerHeight_, color);
	
	blitSubsection(top_left_corner_,x,y,top_left_corner_.w()*scale,top_left_corner_.h()*scale, color);
	blitSubsection(top_right_corner_,x + w - top_right_corner_.w()*scale,y, top_right_corner_.w()*scale, top_right_corner_.h()*scale, color);
	blitSubsection(bottom_left_corner_,x,y + h - bottom_left_corner_.h()*scale,bottom_left_corner_.w()*scale, bottom_left_corner_.h()*scale, color);
	blitSubsection(bottom_right_corner_,x + w - bottom_right_corner_.w()*scale,y + h - bottom_right_corner_.h()*scale,bottom_right_corner_.w()*scale, bottom_right_corner_.h()*scale, color); 
}

void FramedGuiElement::blitSubsection(rect subsection, int x, int y, int w, int h, const KRE::Color& color) const
{
	Canvas::getInstance()->blitTexture(texture_, subsection, 0, rect(x,y,w,h), color);
}

