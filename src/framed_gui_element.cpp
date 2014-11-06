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
#include <iostream>

#include "asserts.hpp"
#include "foreach.hpp"
#include "framed_gui_element.hpp"
#include "geometry.hpp"
#include "raster.hpp"
#include "variant_utils.hpp"

namespace {
	typedef std::map<std::string, const_framed_gui_element_ptr> cache_map;
	cache_map cache;
}

std::vector<std::string> framed_gui_element::get_elements()
{
	std::vector<std::string> v;
	for(auto it : cache) {
		v.push_back(it.first);
	}
	return v;
}


void framed_gui_element::init(variant node)
{
	foreach(variant obj, node["framed_gui_element"].as_list()) {
		const std::string& id = obj["id"].as_string();
		cache[id].reset(new framed_gui_element(obj));
	}
}

const_framed_gui_element_ptr framed_gui_element::get(const std::string& key)
{
	cache_map::const_iterator itor = cache.find(key);
	ASSERT_LOG(itor != cache.end(), "Couldn't find gui_element named '" << key << "' in list");
	return itor->second;
}


framed_gui_element::framed_gui_element(variant node)
: area_(node["rect"]),
corner_height_(node["corner_height"].as_int()),
texture_(graphics::texture::get(node["image"].as_string()))
{
	top_left_corner_ = rect(area_.x(),area_.y(),corner_height_,corner_height_);
	top_right_corner_ = rect(area_.x2() - corner_height_,area_.y(),corner_height_,corner_height_);
	bottom_left_corner_ = rect(area_.x(),area_.y2() - corner_height_,corner_height_,corner_height_);
	bottom_right_corner_ = rect(area_.x2() - corner_height_,area_.y2() - corner_height_,corner_height_,corner_height_);
	
	top_border_ = rect(area_.x() + corner_height_, area_.y(),area_.w() - corner_height_ * 2, corner_height_);
	bottom_border_ = rect(area_.x() + corner_height_, area_.y2() - corner_height_,area_.w() - corner_height_ * 2, corner_height_);
	left_border_ = rect(area_.x(), area_.y() + corner_height_,corner_height_, area_.h() - corner_height_ * 2);
	right_border_ = rect(area_.x2() - corner_height_, area_.y() + corner_height_,corner_height_,area_.h() - corner_height_ * 2);
	
	interior_fill_ = rect(area_.x() + corner_height_, area_.y() + corner_height_,area_.w() - corner_height_ * 2,area_.h() - corner_height_ * 2);
}

void framed_gui_element::blit(int x, int y, int w, int h, bool upscaled) const
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
	
	int scale = upscaled? 2:1;
	
	blit_subsection(interior_fill_,x+ corner_height_,y+ corner_height_,w-2*corner_height_,h-2*corner_height_);
	
	blit_subsection(top_border_,x + corner_height_,y,w - corner_height_*2,top_border_.h()*scale);
	blit_subsection(bottom_border_,x + corner_height_,y + h - bottom_border_.h()*scale,w-corner_height_*2,bottom_border_.h()*scale);
	blit_subsection(left_border_,x,y+corner_height_,left_border_.w()*scale,h-2*corner_height_);
	blit_subsection(right_border_,x + w - right_border_.w()*scale, y+corner_height_,right_border_.w()*scale,h-2*corner_height_);
	
	blit_subsection(top_left_corner_,x,y,top_left_corner_.w()*scale,top_left_corner_.h()*scale);
	blit_subsection(top_right_corner_,x + w - top_right_corner_.w()*scale,y, top_right_corner_.w()*scale, top_right_corner_.h()*scale);
	blit_subsection(bottom_left_corner_,x,y + h - bottom_left_corner_.h()*scale,bottom_left_corner_.w()*scale, bottom_left_corner_.h()*scale);
	blit_subsection(bottom_right_corner_,x + w - bottom_right_corner_.w()*scale,y + h - bottom_right_corner_.h()*scale,bottom_right_corner_.w()*scale, bottom_right_corner_.h()*scale); 
}

void framed_gui_element::blit_subsection(rect subsection, int x, int y, int w, int h) const
{
	graphics::blit_texture(texture_, x, y, w, h, 0.0,
	                       GLfloat(subsection.x())/GLfloat(texture_.width()),
	                       GLfloat(subsection.y())/GLfloat(texture_.height()),
	                       GLfloat(subsection.x2())/GLfloat(texture_.width()),
	                       GLfloat(subsection.y2())/GLfloat(texture_.height()));

}
