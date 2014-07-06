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

#include <vector>

#include "kre/Geometry.hpp"
#include "kre/Texture.hpp"

#include "solid_map_fwd.hpp"
#include "variant.hpp"

enum MOVE_DIRECTION { MOVE_LEFT, MOVE_RIGHT, MOVE_UP, MOVE_DOWN, MOVE_NONE };

class SolidMap
{
public:
	static void create_object_solid_maps(variant node, std::vector<ConstSolidMapPtr>& v);
	static void create_object_platform_maps(const rect& area, std::vector<ConstSolidMapPtr>& v);
	static SolidMapPtr create_from_texture(const KRE::TexturePtr& t, const rect& area);

	const std::string& id() const { return id_; }
	const rect& area() const { return area_; }

	bool solid_at(int x, int y) const;

	const std::vector<point>& dir(MOVE_DIRECTION d) const;
	const std::vector<point>& left() const { return left_; }
	const std::vector<point>& right() const { return right_; }
	const std::vector<point>& top() const { return top_; }
	const std::vector<point>& bottom() const { return bottom_; }
	const std::vector<point>& all() const { return all_; }
private:
	static ConstSolidMapPtr create_object_solid_map_from_solid_node(variant node);

	SolidMap() {}

	void set_solid(int x, int y, bool value=true);

	void calculate_side(int xdir, int ydir, std::vector<point>& points) const;

	void apply_offsets(const std::vector<int>& offsets);

	std::string id_;
	rect area_;

	std::vector<bool> solid_;

	//all the solid points that are on the different sides of the solid area.
	std::vector<point> left_, right_, top_, bottom_, all_;
};

class SolidInfo
{
public:
	static ConstSolidInfoPtr create(variant node);
	static ConstSolidInfoPtr create_platform(variant node);
	static ConstSolidInfoPtr create_platform(const rect& area);
	static ConstSolidInfoPtr create_from_texture(const KRE::TexturePtr& t, const rect& area);
	const std::vector<ConstSolidMapPtr>& solid() const { return solid_; }
	const rect& area() const { return area_; }
	bool solid_at(int x, int y, const std::string** area_id=NULL) const;
private:
	static ConstSolidInfoPtr create_from_solid_maps(const std::vector<ConstSolidMapPtr>& v);

	std::vector<ConstSolidMapPtr> solid_;
	rect area_;
};
