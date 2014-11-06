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
#include "asserts.hpp"
#include "collision_utils.hpp"
#include "foreach.hpp"
#include "geometry.hpp"
#include "level.hpp"
#include "object_events.hpp"

namespace {
std::map<std::string, int> solid_dimensions;
std::vector<std::string> solid_dimension_ids;
}

void collision_info::read_surf_info()
{
	if(surf_info) {
		friction = surf_info->friction;
		traction = surf_info->traction;
		damage = surf_info->damage;
	}
}

int get_num_solid_dimensions()
{
	return solid_dimensions.size();
}

const std::string& get_solid_dimension_key(int id)
{
	ASSERT_INDEX_INTO_VECTOR(id, solid_dimension_ids);
	return solid_dimension_ids[id];
}

int get_solid_dimension_id(const std::string& key)
{
	std::map<std::string, int>::const_iterator itor = solid_dimensions.find(key);
	if(itor != solid_dimensions.end()) {
		return itor->second;
	}

	solid_dimensions[key] = solid_dimension_ids.size();
	solid_dimension_ids.push_back(key);
	return solid_dimensions.size()-1;
}

bool point_standable(const level& lvl, const entity& e, int x, int y, collision_info* info, ALLOW_PLATFORM allow_platform)
{
	if(allow_platform == SOLID_AND_PLATFORMS  && lvl.standable(x, y, info ? &info->surf_info : NULL) ||
	   allow_platform != SOLID_AND_PLATFORMS  && lvl.solid(x, y, info ? &info->surf_info : NULL)) {
		if(info) {
			info->read_surf_info();
		}

		if(info && !lvl.solid(x, y)) {
			info->platform = true;
		}
		return true;
	}

	const point pt(x, y);

	const std::vector<entity_ptr>& chars = lvl.get_solid_chars();

	for(std::vector<entity_ptr>::const_iterator i = chars.begin();
	    i != chars.end(); ++i) {
		const entity_ptr& obj = *i;
		if(&e == obj.get()) {
			continue;
		}

		if(allow_platform == SOLID_AND_PLATFORMS || obj->solid_platform()) {
			const rect& platform_rect = obj->platform_rect_at(pt.x);
			if(point_in_rect(pt, platform_rect) && obj->platform()) {
				if(info) {
					info->collide_with = obj;
					info->friction = obj->surface_friction();
					info->traction = obj->surface_traction();
					info->adjust_y = y - platform_rect.y();
					info->platform = !obj->solid_platform();
				}

				return true;
			}
		}

		if((e.weak_solid_dimensions()&obj->solid_dimensions()) == 0 &&
		   (e.solid_dimensions()&obj->weak_solid_dimensions()) == 0) {
			continue;
		}

		if(!point_in_rect(pt, obj->solid_rect())) {
			continue;
		}

		const frame& f = obj->current_frame();
		const int xpos = obj->face_right() ? x - obj->x() : obj->x() + f.width() - x - 1;

		const solid_info* solid = obj->solid();

		if(solid && solid->solid_at(x - obj->x(), y - obj->y(), info ? &info->collide_with_area_id : NULL)) {
			if(info) {
				info->collide_with = obj;
				info->friction = obj->surface_friction();
				info->traction = obj->surface_traction();
			}

			return true;
		}
	}

	return false;
}

bool entity_collides(level& lvl, const entity& e, MOVE_DIRECTION dir, collision_info* info)
{
	if(!e.solid()) {
		return false;
	}

	if(!e.allow_level_collisions() && entity_collides_with_level(lvl, e, dir, info)) {
		return true;
	}

	const std::vector<entity_ptr>& solid_chars = lvl.get_solid_chars();
	for(std::vector<entity_ptr>::const_iterator obj = solid_chars.begin(); obj != solid_chars.end(); ++obj) {
		if(obj->get() != &e && entity_collides_with_entity(e, **obj, info)) {
			if(info) {
				info->collide_with = *obj;
			}
			return true;
		}
	}

	return false;
}

void debug_check_entity_solidity(const level& lvl, const entity& e)
{
	if(!e.allow_level_collisions() && entity_collides_with_level(lvl, e, MOVE_NONE, NULL)) {
		const solid_info* s = e.solid();
		ASSERT_LOG(s, "ENTITY COLLIDES BUT DOES NOT HAVE SOLID");
		const frame& f = e.current_frame();
		const rect& area = s->area();

		int min_x = INT_MIN, max_x = INT_MIN, min_y = INT_MIN, max_y = INT_MIN;
		std::set<point> solid_points;

		foreach(const const_solid_map_ptr& m, s->solid()) {
			const std::vector<point>& points = m->dir(MOVE_NONE);
			foreach(const point& p, points) {
				const int x = e.x() + (e.face_right() ? p.x : (f.width() - 1 - p.x));
				const int y = e.y() + p.y;

				if(min_x == INT_MIN || x < min_x) {
					min_x = x;
				}

				if(max_x == INT_MIN || x > max_x) {
					max_x = x;
				}

				if(min_y == INT_MIN || y < min_y) {
					min_y = y;
				}

				if(max_y == INT_MIN || y > max_y) {
					max_y = y;
				}

				solid_points.insert(point(x, y));
			}
		}

		std::cerr << "COLLIDING OBJECT MAP:\n";
		for(int y = min_y - 5; y < max_y + 5; ++y) {
			for(int x = min_x - 5; x < max_x + 5; ++x) {
				const bool lvl_solid = lvl.solid(x, y, NULL);
				const bool char_solid = solid_points.count(point(x, y));
				if(lvl_solid && char_solid) {
					std::cerr << "X";
				} else if(lvl_solid) {
					std::cerr << "L";
				} else if(char_solid) {
					std::cerr << "C";
				} else {
					std::cerr << "-";
				}
			}

			std::cerr << "\n";
		}
		std::cerr << "\n";

		ASSERT_LOG(false, "ENTITY " << e.debug_description() << " COLLIDES WITH LEVEL");
	}
}

bool entity_collides_with_entity(const entity& e, const entity& other, collision_info* info)
{
	if((e.solid_dimensions()&other.weak_solid_dimensions()) == 0 &&
	   (e.weak_solid_dimensions()&other.solid_dimensions()) == 0) {
		return false;
	}

	const rect& our_rect = e.solid_rect();
	const rect& other_rect = other.solid_rect();

	if(!rects_intersect(our_rect, other_rect)) {
		return false;
	}

	if(other.destroyed()) {
		return false;
	}

	const rect area = intersection_rect(our_rect, other_rect);

	const solid_info* our_solid = e.solid();
	const solid_info* other_solid = other.solid();
	assert(our_solid && other_solid);

	const frame& our_frame = e.current_frame();
	const frame& other_frame = other.current_frame();

	for(int y = area.y(); y <= area.y2(); ++y) {
		for(int x = area.x(); x < area.x2(); ++x) {
			const int our_x = e.face_right() ? x - e.x() : (e.x() + our_frame.width()-1) - x;
			const int our_y = y - e.y();
			if(our_solid->solid_at(our_x, our_y, info ? &info->area_id : NULL)) {
				const int other_x = other.face_right() ? x - other.x() : (other.x() + other_frame.width()-1) - x;
				const int other_y = y - other.y();
				if(other_solid->solid_at(other_x, other_y, info ? &info->collide_with_area_id : NULL)) {
					return true;
				}
			}
		}
	}

	return false;
}

bool entity_collides_with_level(const level& lvl, const entity& e, MOVE_DIRECTION dir, collision_info* info)
{
	const solid_info* s = e.solid();
	if(!s) {
		return false;
	}

	if(e.face_right() == false) {
		if(dir == MOVE_RIGHT) {
			dir = MOVE_LEFT;
		} else if(dir == MOVE_LEFT) {
			dir = MOVE_RIGHT;
		}
	}

	const frame& f = e.current_frame();

	const rect& area = s->area();
	if(e.face_right()) {
		rect solid_area(e.x() + area.x(), e.y() + area.y(), area.w(), area.h());
		if(!lvl.may_be_solid_in_rect(solid_area)) {
			return false;
		}
	} else {
		rect solid_area(e.x() + f.width() - area.x() - area.w(), e.y() + area.y(), area.w(), area.h());
		if(!lvl.may_be_solid_in_rect(solid_area)) {
			return false;
		}
	}

	foreach(const const_solid_map_ptr& m, s->solid()) {
		if(lvl.solid(e, m->dir(dir), info ? &info->surf_info : NULL)) {
			if(info) {
				info->read_surf_info();
			}

			return true;
		}
	}

	return false;
}

int entity_collides_with_level_count(const level& lvl, const entity& e, MOVE_DIRECTION dir)
{
	const solid_info* s = e.solid();
	if(!s) {
		return 0;
	}

	const frame& f = e.current_frame();
	int count = 0;
	foreach(const const_solid_map_ptr& m, s->solid()) {
		const std::vector<point>& points = m->dir(dir);
		foreach(const point& p, points) {
			const int xpos = e.face_right() ? e.x() + p.x : e.x() + f.width() - 1 - p.x;
			if(lvl.solid(xpos, e.y() + p.y)) {
				++count;
			}
		}
	}

	return count;
}

bool non_solid_entity_collides_with_level(const level& lvl, const entity& e)
{
	const frame& f = e.current_frame();
	if(!lvl.may_be_solid_in_rect(rect(e.x(), e.y(), f.width(), f.height()))) {
		return false;
	}

	const int increment = e.face_right() ? 2 : -2;
	for(int y = 0; y < f.height(); y += 2) {
		std::vector<bool>::const_iterator i = f.get_alpha_itor(0, y, e.time_in_frame(), e.face_right());
		for(int x = 0; x < f.width(); x += 2) {
			if(i == f.get_alpha_buf().end() || i == f.get_alpha_buf().begin()) {
				continue;
			}
			if(!*i && lvl.solid(e.x() + x, e.y() + y)) {
				return true;
			}

			i += increment;
		}
	}

	return false;
}

bool place_entity_in_level(level& lvl, entity& e)
{
	if(e.editor_force_standing()) {
		if(!e.move_to_standing(lvl, 128)) {
			return false;
		}
	}

	if(!entity_collides(lvl, e, MOVE_NONE)) {
		return true;
	}

	if(!entity_collides(lvl, e, MOVE_UP)) {
		while(entity_collides(lvl, e, MOVE_NONE)) {
			e.set_pos(e.x(), e.y()-1);
			if(entity_collides(lvl, e, MOVE_UP)) {
				return false;
			}
		}

		return true;
	}

	if(!entity_collides(lvl, e, MOVE_DOWN)) {
		while(entity_collides(lvl, e, MOVE_NONE)) {
			e.set_pos(e.x(), e.y()+1);
			if(entity_collides(lvl, e, MOVE_DOWN)) {
				return false;
			}
		}

		return true;
	}

	if(!entity_collides(lvl, e, MOVE_LEFT)) {
		while(entity_collides(lvl, e, MOVE_NONE)) {
			e.set_pos(e.x()-1, e.y());
			if(entity_collides(lvl, e, MOVE_LEFT)) {
				return false;
			}
		}

		return true;
	}

	if(!entity_collides(lvl, e, MOVE_RIGHT)) {
		while(entity_collides(lvl, e, MOVE_NONE)) {
			e.set_pos(e.x()+1, e.y());
			if(entity_collides(lvl, e, MOVE_RIGHT)) {
				return false;
			}
		}

		return true;
	}

	return false;
}

bool place_entity_in_level_with_large_displacement(level& lvl, entity& e)
{
	if(!place_entity_in_level(lvl, e)) {
		//the object can't immediately/easily be placed in the level
		//due to a solid collision. Try to incrementally push it in
		//different directions and try to place it until we find
		//a direction that works.
		const int xpos = e.x();
		const int ypos = e.y();

		bool found = false;
		for(int distance = 4; distance < 256 && !found; distance *= 2) {
			const point points[] = { point(xpos-distance, ypos),
			                         point(xpos+distance, ypos),
			                         point(xpos, ypos-distance),
			                         point(xpos, ypos+distance), };
			foreach(const point& p, points) {
				e.set_pos(p);
				if(place_entity_in_level(lvl, e)) {
					found = true;
					break;
				}
			}
		}

		if(!found) {
			return false;
		}
	}

	return true;
}

int entity_user_collision(const entity& a, const entity& b, collision_pair* areas_colliding, int buf_size)
{
	const frame& fa = a.current_frame();
	const frame& fb = b.current_frame();

	if(fa.collision_areas().empty() || fb.collision_areas().empty() ||
	   fa.collision_areas_inside_frame() && fb.collision_areas_inside_frame() &&
	   !rects_intersect(a.frame_rect(), b.frame_rect())) {
		return 0;
	}

	int result = 0;

	foreach(const frame::collision_area& area_a, fa.collision_areas()) {
		rect rect_a = a.calculate_collision_rect(fa, area_a);
		foreach(const frame::collision_area& area_b, fb.collision_areas()) {
			rect rect_b = b.calculate_collision_rect(fb, area_b);
			if(rects_intersect(rect_a, rect_b)) {
				const int time_a = a.time_in_frame();
				const int time_b = b.time_in_frame();

				//we only check every other pixel, since this gives us
				//enough accuracy and is 4x faster.
				const int Stride = 2;
				bool found = false;
				const rect intersection = intersection_rect(rect_a, rect_b);
				for(int y = intersection.y(); y <= intersection.y2() && !found; y += Stride) {
					for(int x = intersection.x(); x <= intersection.x2(); x += Stride) {
						if((area_a.no_alpha_check || !fa.is_alpha(x - a.x(), y - a.y(), time_a, a.face_right())) &&
						   (area_b.no_alpha_check || !fb.is_alpha(x - b.x(), y - b.y(), time_b, b.face_right()))) {
							found = true;
							break;
						}
					}
				}

				if(found) {
					++result;
					if(buf_size > 0) {
						areas_colliding->first = &area_a.name;
						areas_colliding->second = &area_b.name;
						++areas_colliding;
						--buf_size;
					}
				}
			}
		}
	}

	return result;
}

bool entity_user_collision_specific_areas(const entity& a, const std::string& area_a_id, const entity& b, const std::string& area_b_id)
{
	if(&a == &b) {
		return false;
	}

	const frame& fa = a.current_frame();
	const frame& fb = b.current_frame();

	if(fa.collision_areas().empty() || fb.collision_areas().empty()) {
		return false;
	}

	if(!rects_intersect(rect(a.x(), a.y(), fa.width(), fa.height()),
	                    rect(b.x(), b.y(), fb.width(), fb.height()))) {
		return false;
	}

	const frame::collision_area* area_a = NULL;
	foreach(const frame::collision_area& area, fa.collision_areas()) {
		if(area.name == area_a_id) {
			area_a = &area;
			break;
		}
	}

	if(!area_a) {
		return false;
	}

	const frame::collision_area* area_b = NULL;
	foreach(const frame::collision_area& area, fb.collision_areas()) {
		if(area.name == area_b_id) {
			area_b = &area;
			break;
		}
	}

	if(!area_b) {
		return false;
	}

	rect rect_a(a.face_right() ? a.x() + area_a->area.x() : a.x() + fa.width() - area_a->area.x() - area_a->area.w(),
	            a.y() + area_a->area.y(),
				area_a->area.w(), area_a->area.h());
	rect rect_b(b.face_right() ? b.x() + area_b->area.x() : b.x() + fb.width() - area_b->area.x() - area_b->area.w(),
	            b.y() + area_b->area.y(),
				area_b->area.w(), area_b->area.h());
	if(!rects_intersect(rect_a, rect_b)) {
		return false;
	}

	const int time_a = a.time_in_frame();
	const int time_b = b.time_in_frame();

	const rect intersection = intersection_rect(rect_a, rect_b);
	for(int y = intersection.y(); y <= intersection.y2(); ++y) {
		for(int x = intersection.x(); x <= intersection.x2(); ++x) {
			if(!fa.is_alpha(x - a.x(), y - a.y(), time_a, a.face_right()) &&
			   !fb.is_alpha(x - b.x(), y - b.y(), time_b, b.face_right())) {
				return true;
			}
		}
	}

	return false;
}

namespace {
class user_collision_callable : public game_logic::formula_callable {
	entity_ptr a_, b_;
	const std::string* area_a_;
	const std::string* area_b_;
	int index_;
	variant all_collisions_;

	DECLARE_CALLABLE(user_collision_callable);
public:
	user_collision_callable(entity_ptr a, entity_ptr b, const std::string& area_a, const std::string& area_b, int index) : a_(a), b_(b), area_a_(&area_a), area_b_(&area_b), index_(index) {
	}

	void set_all_collisions(variant v) {
		all_collisions_ = v;
	}
};

BEGIN_DEFINE_CALLABLE_NOBASE(user_collision_callable)
DEFINE_FIELD(collide_with, "custom_obj")
	return variant(obj.b_.get());
DEFINE_FIELD(area, "string")
	return variant(*obj.area_a_);
DEFINE_FIELD(collide_with_area, "string")
	return variant(*obj.area_b_);
DEFINE_FIELD(collision_index, "int")
	return variant(obj.index_);
DEFINE_FIELD(all_collisions, "[builtin user_collision_callable]")
	return obj.all_collisions_;
END_DEFINE_CALLABLE(user_collision_callable)

int get_collision_event_id(const std::string& area)
{
	static std::map<std::string, int> cache;
	std::map<std::string, int>::const_iterator itor = cache.find(area);
	if(itor != cache.end()) {
		return itor->second;
	}

	cache[area] = get_object_event_id("collide_object_" + area);
	return cache[area];
}

}

void detect_user_collisions(level& lvl)
{
	std::vector<entity_ptr> chars;
	chars.reserve(lvl.get_active_chars().size());
	foreach(const entity_ptr& a, lvl.get_active_chars()) {
		if(a->weak_collide_dimensions() != 0 && a->current_frame().collision_areas().empty() == false) {
			chars.push_back(a);
		}
	}

	typedef std::pair<entity_ptr, const std::string*> collision_key;
	std::map<collision_key, std::vector<collision_key> > collision_info;

	static const int CollideObjectID = get_object_event_id("collide_object");

	const int MaxCollisions = 16;
	collision_pair collision_buf[MaxCollisions];
	for(std::vector<entity_ptr>::const_iterator i = chars.begin(); i != chars.end(); ++i) {
		for(std::vector<entity_ptr>::const_iterator j = i + 1; j != chars.end(); ++j) {
			const entity_ptr& a = *i;
			const entity_ptr& b = *j;
			if(a == b ||
			   (a->weak_collide_dimensions()&b->collide_dimensions()) == 0 &&
			   (a->collide_dimensions()&b->weak_collide_dimensions()) == 0) {
				//the objects do not share a dimension, and so can't collide.
				continue;
			}

			int ncollisions = entity_user_collision(*a, *b, collision_buf, MaxCollisions);
			if(ncollisions > MaxCollisions) {
				ncollisions = MaxCollisions;
			}

			for(int n = 0; n != ncollisions; ++n) {
				{
					collision_info[collision_key(a, collision_buf[n].first)].push_back(collision_key(b, collision_buf[n].second));
				}

				{
					collision_info[collision_key(b, collision_buf[n].second)].push_back(collision_key(a, collision_buf[n].first));
				}
			}
		}
	}

	for(std::map<collision_key, std::vector<collision_key> >::iterator i = collision_info.begin(); i != collision_info.end(); ++i) {
		std::vector<boost::intrusive_ptr<user_collision_callable> > v;
		std::vector<variant> all_callables;
		v.reserve(i->second.size());
		int index = 0;
		foreach(const collision_key& k, i->second) {
			v.push_back(boost::intrusive_ptr<user_collision_callable>(new user_collision_callable(i->first.first, k.first, *i->first.second, *k.second, index)));
			all_callables.push_back(variant(v.back().get()));
			++index;
		}

		variant all_callables_variant(&all_callables);

		collision_key key = i->first;

		foreach(const boost::intrusive_ptr<user_collision_callable>& p, v) {
			p->set_all_collisions(all_callables_variant);
			key.first->handle_event_delay(CollideObjectID, p.get());
			key.first->handle_event_delay(get_collision_event_id(*i->first.second), p.get());
		}

		foreach(const boost::intrusive_ptr<user_collision_callable>& p, v) {
			//make sure we don't retain circular references.
			p->set_all_collisions(variant());
		}
	}

	for(std::vector<entity_ptr>::const_iterator i = chars.begin(); i != chars.end(); ++i) {
		const entity_ptr& a = *i;
		a->resolve_delayed_events();
	}
}

bool is_flightpath_clear(const level& lvl, const entity& e, const rect& area)
{
	if(lvl.may_be_solid_in_rect(area)) {
		return false;
	}

	const std::vector<entity_ptr>& v = lvl.get_solid_chars();
	for(std::vector<entity_ptr>::const_iterator obj = v.begin();
	    obj != v.end(); ++obj) {
		if(obj->get() == &e) {
			continue;
		}

		if(rects_intersect(area, (*obj)->solid_rect())) {
			return false;
		}
	}

	return true;
}
