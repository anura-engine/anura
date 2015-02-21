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

#include "DisplayDevice.hpp"

#include "solid_map.hpp"
#include "string_utils.hpp"

ConstSolidMapPtr SolidMap::createObjectSolidMapFromSolidNode(variant node)
{
	SolidMapPtr result(createFromTexture(KRE::DisplayDevice::createTexture(node["image"]), rect(node["area"])));
	result->id_ = node["id"].as_string();
	return result;

}

void SolidMap::createObjectSolidMaps(variant node, std::vector<ConstSolidMapPtr>& v)
{
	for(variant solid_node : node["solid"].as_list()) {
		v.push_back(createObjectSolidMapFromSolidNode(solid_node));
	}

	if(!node.has_key("solid_area")) {
		return;
	}

	rect area(node["solid_area"]);
	area = rect(area.x()*2, area.y()*2, area.w()*2, area.h()*2);

	const int feet_width = node["feet_width"].as_int(0);

	int legs_height = area.w()/2 + 1 - feet_width;
	if(node["has_feet"].as_bool(true) == false || node.has_key("solid_offsets") || node["solid_shape"].as_string_default() == "rect" || legs_height < 0) {
		legs_height = 0;
	}


	//flat is a special case which says the solid area is to be
	//precisely one pixel high.
	if(node["solid_shape"].as_string_default() == "flat") {
		legs_height = 0;
		area = rect(area.x(), area.y()+area.h()-1, area.w(), 1);
	}

	if(legs_height < area.h()) {
		rect body(area.x(), area.y(), area.w(), area.h() - legs_height);
		SolidMapPtr body_map(new SolidMap());
		body_map->id_ = "body";
		body_map->area_ = body;
		body_map->solid_.resize(body.w()*body.h(), true);
		if(node.has_key("solid_offsets")) {
			body_map->applyOffsets(node["solid_offsets"].as_list_int());
		}

		body_map->calculateSide(0, -1, body_map->top_);
		body_map->calculateSide(-1, 0, body_map->left_);
		body_map->calculateSide(1, 0, body_map->right_);
		body_map->calculateSide(-100000, 0, body_map->all_);

		if(legs_height == 0) {
			body_map->calculateSide(0, 1, body_map->bottom_);
		}
		v.push_back(body_map);
	} else {
		legs_height = area.h();
	}

	if(legs_height) {
		rect legs(area.x(), area.y2() - legs_height, area.w(), legs_height);
		SolidMapPtr legs_map(new SolidMap());
		legs_map->id_ = "legs";
		legs_map->area_ = legs;
		legs_map->solid_.resize(legs.w()*legs.h(), false);
		for(int y = 0; y < legs.h()-1; ++y) {
			for(int x = y; x < legs.w() - y; ++x) {
				legs_map->setSolid(x, y);
			}
		}

		if(area.h() <= legs_height) {
			legs_map->calculateSide(0, -1, legs_map->top_);
		}

		legs_map->calculateSide(0, 1, legs_map->bottom_);
		legs_map->calculateSide(-1, 0, legs_map->left_);
		legs_map->calculateSide(1, 0, legs_map->right_);
		legs_map->calculateSide(-10000, 0, legs_map->all_);
		v.push_back(legs_map);
	}
}

void SolidMap::createObjectPlatformMaps(const rect& area_ref, std::vector<ConstSolidMapPtr>& v)
{

	//intentionally do NOT double the height of the area.
	rect area(area_ref.x()*2, area_ref.y()*2, area_ref.w()*2, 1);

	ASSERT_EQ(area.h(), 1);

	SolidMapPtr platform(new SolidMap());
	platform->id_ = "platform";
	platform->area_ = area;
	platform->solid_.resize(area.w()*area.h(), true);
	platform->calculateSide(0, -1, platform->top_);
	platform->calculateSide(0, 1, platform->bottom_);
	platform->calculateSide(-1, 0, platform->left_);
	platform->calculateSide(1, 0, platform->right_);
	platform->calculateSide(-100000, 0, platform->all_);
	v.push_back(platform);
}
SolidMapPtr SolidMap::createFromTexture(const KRE::TexturePtr& t, const rect& area_rect)
{
	rect area = area_rect;

	bool found_solid = false;
	while(!found_solid && area.h() > 0) {
		for(int x = 0; x < area.w(); ++x) {
			if(!t->getFrontSurface()->isAlpha(area.x() + x, area.y() + area.h() - 1)) {
				found_solid = true;
				break;
			}
		}

		if(!found_solid) {
			area = rect(area.x(), area.y(), area.w(), area.h()-1);
		}
	}

	found_solid = false;
	while(!found_solid && area.h() > 0) {
		for(int x = 0; x < area.w(); ++x) {
			if(!t->getFrontSurface()->isAlpha(area.x() + x, area.y())) {
				found_solid = true;
				break;
			}
		}

		if(!found_solid) {
			area = rect(area.x(), area.y()+1, area.w(), area.h()-1);
		}
	}

	found_solid = false;
	while(!found_solid && area.w() > 0) {
		for(int y = 0; y < area.h(); ++y) {
			if(!t->getFrontSurface()->isAlpha(area.x(), area.y() + y)) {
				found_solid = true;
				break;
			}
		}

		if(!found_solid) {
			area = rect(area.x()+1, area.y(), area.w()-1, area.h());
		}
	}

	found_solid = false;
	while(!found_solid && area.w() > 0) {
		for(int y = 0; y < area.h(); ++y) {
			if(!t->getFrontSurface()->isAlpha(area.x() + area.w() - 1, area.y() + y)) {
				found_solid = true;
				break;
			}
		}

		if(!found_solid) {
			area = rect(area.x(), area.y(), area.w()-1, area.h());
		}
	}

	SolidMapPtr solid(new SolidMap());
	solid->area_ = rect(area.x()*2, area.y()*2, area.w()*2, area.h()*2);
	solid->solid_.resize(solid->area_.w()*solid->area_.h(), false);
	for(int y = 0; y < solid->area_.h(); ++y) {
		for(int x = 0; x < solid->area_.w(); ++x) {
			bool is_solid = !t->getFrontSurface()->isAlpha(area.x() + x/2, area.y() + y/2);
			if(!is_solid && (y&1) && y < solid->area_.h() - 1 && !t->getFrontSurface()->isAlpha(area.x() + x/2, area.y() + y/2 + 1)) {
				//we are scaling things up by double, so we want to smooth
				//things out. In the bottom half of an empty source pixel, we
				//will set it to solid if the pixel below is solid, and the
				//adjacent horizontal pixel is solid
				if((x&1) && x < solid->area_.w() - 1 && !t->getFrontSurface()->isAlpha(area.x() + x/2 + 1, area.y() + y/2)) {
					is_solid = true;
				} else if(!(x&1) && x > 0 && !t->getFrontSurface()->isAlpha(area.x() + x/2 - 1, area.y() + y/2)) {
					is_solid = true;
				}
			}

			if(is_solid) {
				solid->setSolid(x, y);
			}
		}
	}
	return solid;
}

bool SolidMap::isSolidAt(int x, int y) const
{
	if(x < 0 || y < 0 || x >= area_.w() || y >= area_.h()) {
		return false;
	}

	return solid_[y*area_.w() + x];
}

const std::vector<point>& SolidMap::dir(MOVE_DIRECTION d) const
{
	switch(d) {
		case MOVE_DIRECTION::LEFT: return left();
		case MOVE_DIRECTION::RIGHT: return right();
		case MOVE_DIRECTION::UP: return top();
		case MOVE_DIRECTION::DOWN: return bottom();
		case MOVE_DIRECTION::NONE: return all();
		default:
			assert(false);
			return all();
	}
}

void SolidMap::setSolid(int x, int y, bool value)
{
	ASSERT_EQ(solid_.size(), area_.w()*area_.h());
	if(x < 0 || y < 0 || x >= area_.w() || y >= area_.h()) {
		return;
	}

	solid_[y*area_.w() + x] = value;
}

void SolidMap::applyOffsets(const std::vector<int>& offsets)
{
	if(offsets.size() <= 1) {
		return;
	}

	const int seg_width = (area_.w()*1024)/(offsets.size()-1);
	for(int x = 0; x != area_.w(); ++x) {
		const int pos = x*1024;
		const int segment = pos/seg_width;
		ASSERT_GE(segment, 0);
		ASSERT_LT(static_cast<unsigned>(segment), offsets.size()-1);

		const int partial = pos%seg_width;
		const int offset = (partial*offsets[segment+1]*2 + (seg_width-partial)*offsets[segment]*2)/seg_width;

		for(int y = 0; y < offset; ++y) {
			setSolid(x, y, false);
		}
	}
}

void SolidMap::calculateSide(int xdir, int ydir, std::vector<point>& points) const
{
	int index = 0;
	const int height = area_.h();
	const int width = area_.w();
	for(int y = 0; y < height; ++y) {
		for(int x = 0; x < width; ++x) {
			//for performance reasons, check our current position directly
			//rather than calling isSolidAt() so we don't do bounds checking.
			if(solid_[index] && !isSolidAt(x + xdir, y + ydir)) {
				points.push_back(point(area_.x() + x, area_.y() + y));
			}

			++index;
		}
	}
}

ConstSolidInfoPtr SolidInfo::createFromSolidMaps(const std::vector<ConstSolidMapPtr>& solid)
{
	if(solid.empty()) {
		return ConstSolidInfoPtr();
	} else {
		SolidInfo* result = new SolidInfo();
		int x1 = solid.front()->area().x();
		int y1 = solid.front()->area().y();
		int x2 = solid.front()->area().x2();
		int y2 = solid.front()->area().y2();
		for(ConstSolidMapPtr s : solid) {
			if(s->area().x() < x1) {
				x1 = s->area().x();
			}
			if(s->area().y() < y1) {
				y1 = s->area().y();
			}
			if(s->area().x2() > x2) {
				x2 = s->area().x2();
			}
			if(s->area().y2() > y2) {
				y2 = s->area().y2();
			}
		}

		result->area_ = rect::from_coordinates(x1, y1, x2-1, y2-1);
		result->solid_= solid;
		return ConstSolidInfoPtr(result);
	}
}

ConstSolidInfoPtr SolidInfo::create(variant node)
{
	std::vector<ConstSolidMapPtr> solid;
	SolidMap::createObjectSolidMaps(node, solid);
	return createFromSolidMaps(solid);
}

ConstSolidInfoPtr SolidInfo::createPlatform(variant node)
{
	std::vector<ConstSolidMapPtr> platform;

	if(!node.has_key("platform_area")) {
		return ConstSolidInfoPtr();
	}

	SolidMap::createObjectPlatformMaps(rect(node["platform_area"]), platform);

	return createFromSolidMaps(platform);
}

ConstSolidInfoPtr SolidInfo::createPlatform(const rect& area)
{
	std::vector<ConstSolidMapPtr> platform;
	SolidMap::createObjectPlatformMaps(area, platform);
	return createFromSolidMaps(platform);
}

ConstSolidInfoPtr SolidInfo::createFromTexture(const KRE::TexturePtr& t, const rect& area)
{
	std::vector<ConstSolidMapPtr> solid;
	solid.push_back(SolidMap::createFromTexture(t, area));
	return createFromSolidMaps(solid);
}

bool SolidInfo::isSolidAt(int x, int y, const std::string** area_id) const
{
	for(const ConstSolidMapPtr& s : solid_) {
		if(s->isSolidAt(x - s->area().x(), y - s->area().y())) {
			if(area_id) {
				*area_id = &s->id();
			}
			return true;
		}
	}

	return false;
}
