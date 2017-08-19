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

#include "asserts.hpp"
#include "collision_utils.hpp"
#include "frame.hpp"
#include "geometry.hpp"
#include "level.hpp"
#include "object_events.hpp"
#include "rectangle_rotator.hpp"
#include "solid_map.hpp"

namespace 
{
	std::map<std::string, int> solid_dimensions;
	std::vector<std::string> solid_dimension_ids;

	//Translate the y position when the object is inverted. To do this we invert the solid position
	int translate_y_for_inverted_solid(int ypos, const rect& frame_rect, const rect& solid_rect) {
		const int dist_from_bottom = (frame_rect.h()-1) - solid_rect.y2();

		const int delta_y = solid_rect.y() - dist_from_bottom;

		return ypos + delta_y;
	}
}

void CollisionInfo::readSurfInfo()
{
	if(surf_info) {
		friction = surf_info->friction;
		traction = surf_info->traction;
		damage = surf_info->damage;
	}
}

int get_num_solid_dimensions()
{
	return static_cast<int>(solid_dimensions.size());
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

	solid_dimensions[key] = static_cast<int>(solid_dimension_ids.size());
	solid_dimension_ids.push_back(key);
	return static_cast<int>(solid_dimensions.size())-1;
}

std::vector<EntityPtr> get_potentially_standable_objects_in_area(const Level& lvl, const Entity& e, const rect& area, ALLOW_PLATFORM allow_platform)
{
	std::vector<EntityPtr> result;

	const std::vector<EntityPtr>& chars = lvl.get_solid_chars();

	for(const EntityPtr& obj : chars) {
		if(obj.get() == &e) {
			continue;
		}

		if((allow_platform == SOLID_AND_PLATFORMS || obj->isSolidPlatform()) && geometry::rects_intersect(obj->platformRect(), area)) {
			result.push_back(obj);
			continue;
		}

		if((e.getWeakSolidDimensions()&obj->getSolidDimensions()) == 0 &&
		   (e.getSolidDimensions()&obj->getWeakSolidDimensions()) == 0) {
			continue;
		}

		if(!geometry::rects_intersect(area, obj->solidRect())) {
			continue;
		}

		result.push_back(obj);
	}

	return result;
}

bool point_standable(const Level& lvl, const Entity& e, int x, int y, CollisionInfo* info, ALLOW_PLATFORM allow_platform)
{
	return point_standable(lvl, e, lvl.get_solid_chars(), x, y, info, allow_platform);
}

bool point_standable(const Level& lvl, const Entity& e, const std::vector<EntityPtr>& chars, int x, int y, CollisionInfo* info, ALLOW_PLATFORM allow_platform)
{
	if((allow_platform == SOLID_AND_PLATFORMS  && lvl.standable(x, y, info ? &info->surf_info : nullptr)) ||
	   (allow_platform != SOLID_AND_PLATFORMS  && lvl.solid(x, y, info ? &info->surf_info : nullptr))) {
		if(info) {
			info->readSurfInfo();
		}

		if(info && !lvl.solid(x, y)) {
			info->platform = true;
		}
		return true;
	}

	const point pt(x, y);

	for(std::vector<EntityPtr>::const_iterator i = chars.begin();
	    i != chars.end(); ++i) {
		const EntityPtr& obj = *i;
		if(&e == obj.get()) {
			continue;
		}

		if(allow_platform == SOLID_AND_PLATFORMS || obj->isSolidPlatform()) {
			const rect& platform_rect = obj->platformRectAt(pt.x);
			if(pointInRect(pt, platform_rect) && obj->platform()) {
				if(info) {
					info->collide_with = obj;
					info->friction = obj->getSurfaceFriction();
					info->traction = obj->getSurfaceTraction();
					info->adjust_y = y - platform_rect.y();
					info->platform = !obj->isSolidPlatform();
				}

				return true;
			}
		}

		if((e.getWeakSolidDimensions()&obj->getSolidDimensions()) == 0 &&
		   (e.getSolidDimensions()&obj->getWeakSolidDimensions()) == 0) {
			continue;
		}

		if(!pointInRect(pt, obj->solidRect())) {
			continue;
		}

		const Frame& f = obj->getCurrentFrame();

		const SolidInfo* solid = obj->solid();

		int ypos = y - obj->y();
		if(obj->isUpsideDown()) {
			ypos = translate_y_for_inverted_solid(ypos, obj->frameRect(), solid->area());
		}

		if(solid && solid->isSolidAt(x - obj->x(), ypos, info ? &info->collide_with_area_id : nullptr)) {
			if(info) {
				info->collide_with = obj;
				info->friction = obj->getSurfaceFriction();
				info->traction = obj->getSurfaceTraction();
			}

			return true;
		}
	}

	return false;
}

bool entity_collides(Level& lvl, const Entity& e, MOVE_DIRECTION dir, CollisionInfo* info)
{
	if(!e.solid()) {
		return false;
	}

	if(!e.allowLevelCollisions() && entity_collides_with_level(lvl, e, dir, info)) {
		return true;
	}

	const std::vector<EntityPtr>& solid_chars = lvl.get_solid_chars();
	for(std::vector<EntityPtr>::const_iterator obj = solid_chars.begin(); obj != solid_chars.end(); ++obj) {
		if(obj->get() != &e && entity_collides_with_entity(e, **obj, info)) {
			if(info) {
				info->collide_with = *obj;
			}
			return true;
		}
	}

	return false;
}

void debug_check_entity_solidity(const Level& lvl, const Entity& e)
{
	if(!e.allowLevelCollisions() && entity_collides_with_level(lvl, e, MOVE_DIRECTION::NONE, nullptr)) {
		const SolidInfo* s = e.solid();
		ASSERT_LOG(s, "ENTITY COLLIDES BUT DOES NOT HAVE SOLID");
		const Frame& f = e.getCurrentFrame();
		const rect& area = s->area();

		int min_x = std::numeric_limits<int>::min(), max_x = std::numeric_limits<int>::min(), min_y = std::numeric_limits<int>::min(), max_y = std::numeric_limits<int>::min();
		std::set<point> solid_points;

		for(const ConstSolidMapPtr& m : s->solid()) {
			const std::vector<point>& points = m->dir(MOVE_DIRECTION::NONE);
			for(const point& p : points) {
				const int x = e.x() + (e.isFacingRight() ? p.x : (f.width() - 1 - p.x));
				const int y = e.y() + p.y;

				if(min_x == std::numeric_limits<int>::min() || x < min_x) {
					min_x = x;
				}

				if(max_x == std::numeric_limits<int>::min() || x > max_x) {
					max_x = x;
				}

				if(min_y == std::numeric_limits<int>::min() || y < min_y) {
					min_y = y;
				}

				if(max_y == std::numeric_limits<int>::min() || y > max_y) {
					max_y = y;
				}

				solid_points.insert(point(x, y));
			}
		}

		LOG_DEBUG("COLLIDING OBJECT MAP:");
		std::ostringstream ss;
		for(int y = min_y - 5; y < max_y + 5; ++y) {
			for(int x = min_x - 5; x < max_x + 5; ++x) {
				const bool lvl_solid = lvl.solid(x, y, nullptr);
				const bool char_solid = solid_points.count(point(x, y)) != 0;
				if(lvl_solid && char_solid) {
					ss << "X";
				} else if(lvl_solid) {
					ss << "L";
				} else if(char_solid) {
					ss << "C";
				} else {
					ss << "-";
				}
			}
			ss << "\n";
		}
		ss << "\n";
		LOG_DEBUG(ss.str());

		ASSERT_LOG(false, "ENTITY " << e.getDebugDescription() << " COLLIDES WITH LEVEL");
	}
}

bool entity_collides_with_entity(const Entity& e, const Entity& other, CollisionInfo* info)
{
	if((e.getSolidDimensions()&other.getWeakSolidDimensions()) == 0 &&
	   (e.getWeakSolidDimensions()&other.getSolidDimensions()) == 0) {
		return false;
	}

	const rect& our_rect = e.solidRect();
	const rect& other_rect = other.solidRect();

	if(!rects_intersect(our_rect, other_rect)) {
		return false;
	}

	if(other.destroyed()) {
		return false;
	}

	const rect area = intersection_rect(our_rect, other_rect);

	const SolidInfo* our_solid = e.solid();
	const SolidInfo* other_solid = other.solid();
	assert(our_solid && other_solid);

	const Frame& our_frame = e.getCurrentFrame();
	const Frame& other_frame = other.getCurrentFrame();

	for(int y = area.y(); y <= area.y2(); ++y) {
		for(int x = area.x(); x < area.x2(); ++x) {
			const int our_x = e.isFacingRight() ? x - e.x() : (e.x() + our_frame.width()-1) - x;
			int our_y = y - e.y();
			if(e.isUpsideDown()) {
				our_y = translate_y_for_inverted_solid(our_y, e.frameRect(), our_solid->area());
			}

			if(our_solid->isSolidAt(our_x, our_y, info ? &info->area_id : nullptr)) {
				const int other_x = other.isFacingRight() ? x - other.x() : (other.x() + other_frame.width()-1) - x;
				int other_y = y - other.y();
				if(other.isUpsideDown()) {
					other_y = translate_y_for_inverted_solid(other_y, other.frameRect(), other_solid->area());
				}

				if(other_solid->isSolidAt(other_x, other_y, info ? &info->collide_with_area_id : nullptr)) {
					return true;
				}
			}
		}
	}

	return false;
}

bool entity_collides_with_level(const Level& lvl, const Entity& e, MOVE_DIRECTION dir, CollisionInfo* info)
{
	const SolidInfo* s = e.solid();
	if(!s) {
		return false;
	}

	if(e.isFacingRight() == false) {
		if(dir == MOVE_DIRECTION::RIGHT) {
			dir = MOVE_DIRECTION::LEFT;
		} else if(dir == MOVE_DIRECTION::LEFT) {
			dir = MOVE_DIRECTION::RIGHT;
		}
	}

	const Frame& f = e.getCurrentFrame();

	const rect& area = s->area();
	if(e.isFacingRight()) {
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

	for(const ConstSolidMapPtr& m : s->solid()) {
		if(lvl.solid(e, m->dir(dir), info ? &info->surf_info : nullptr)) {
			if(info) {
				info->readSurfInfo();
			}

			return true;
		}
	}

	return false;
}

int entity_collides_with_level_count(const Level& lvl, const Entity& e, MOVE_DIRECTION dir)
{
	const SolidInfo* s = e.solid();
	if(!s) {
		return 0;
	}

	const Frame& f = e.getCurrentFrame();
	int count = 0;
	for(const ConstSolidMapPtr& m : s->solid()) {
		const std::vector<point>& points = m->dir(dir);
		for(const point& p : points) {
			const int xpos = e.isFacingRight() ? e.x() + p.x : e.x() + f.width() - 1 - p.x;
			if(lvl.solid(xpos, e.y() + p.y)) {
				++count;
			}
		}
	}

	return count;
}

bool non_solid_entity_collides_with_level(const Level& lvl, const Entity& e)
{
	const Frame& f = e.getCurrentFrame();
	if(!lvl.may_be_solid_in_rect(rect(e.x(), e.y(), f.width(), f.height()))) {
		return false;
	}

	const int increment = e.isFacingRight() ? 2 : -2;
	for(int y = 0; y < f.height(); y += 2) {
		std::vector<bool>::const_iterator i = f.getAlphaItor(0, y, e.getTimeInFrame(), e.isFacingRight());
		for(int x = 0; x < f.width(); x += 2) {
			if(i == f.getAlphaBuf().end() || i == f.getAlphaBuf().begin()) {
				continue;
			}
			if(*i && lvl.solid(e.x() + x, e.y() + y)) {
				return true;
			}

			i += increment;
		}
	}

	return false;
}

bool place_entity_in_level(Level& lvl, Entity& e)
{
	if(e.editorForceStanding()) {
		if(!e.moveToStanding(lvl, 128)) {
			return false;
		}
	}

	if(!entity_collides(lvl, e, MOVE_DIRECTION::NONE)) {
		return true;
	}

	if(!entity_collides(lvl, e, MOVE_DIRECTION::UP)) {
		while(entity_collides(lvl, e, MOVE_DIRECTION::NONE)) {
			e.setPos(e.x(), e.y()-1);
			if(entity_collides(lvl, e, MOVE_DIRECTION::UP)) {
				return false;
			}
		}

		return true;
	}

	if(!entity_collides(lvl, e, MOVE_DIRECTION::DOWN)) {
		while(entity_collides(lvl, e, MOVE_DIRECTION::NONE)) {
			e.setPos(e.x(), e.y()+1);
			if(entity_collides(lvl, e, MOVE_DIRECTION::DOWN)) {
				return false;
			}
		}

		return true;
	}

	if(!entity_collides(lvl, e, MOVE_DIRECTION::LEFT)) {
		while(entity_collides(lvl, e, MOVE_DIRECTION::NONE)) {
			e.setPos(e.x()-1, e.y());
			if(entity_collides(lvl, e, MOVE_DIRECTION::LEFT)) {
				return false;
			}
		}

		return true;
	}

	if(!entity_collides(lvl, e, MOVE_DIRECTION::RIGHT)) {
		while(entity_collides(lvl, e, MOVE_DIRECTION::NONE)) {
			e.setPos(e.x()+1, e.y());
			if(entity_collides(lvl, e, MOVE_DIRECTION::RIGHT)) {
				return false;
			}
		}

		return true;
	}

	return false;
}

bool place_entity_in_level_with_large_displacement(Level& lvl, Entity& e)
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
			for(const point& p : points) {
				e.setPos(p);
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

int entity_user_collision(const Entity& a, const Entity& b, CollisionPair* areas_colliding, int buf_size)
{
	const Frame& fa = a.getCurrentFrame();
	const Frame& fb = b.getCurrentFrame();

	const int rotate_a = a.currentRotation();
	const int rotate_b = b.currentRotation();

	if(fa.getCollisionAreas().empty() || fb.getCollisionAreas().empty() ||
	   (rotate_a == 0 && rotate_b == 0 &&
	    fa.hasCollisionAreasInsideFrame() && fb.hasCollisionAreasInsideFrame() &&
	   !rects_intersect(a.frameRect(), b.frameRect()))) {
		return 0;
	}

	const int time_a = a.getTimeInFrame();
	const int time_b = b.getTimeInFrame();

	int result = 0;

	for(const auto& area_a : fa.getCollisionAreas()) {
		rect rect_a = a.calculateCollisionRect(fa, area_a);
		for(const auto& area_b : fb.getCollisionAreas()) {
			rect rect_b = b.calculateCollisionRect(fb, area_b);

			bool found = false;

			if(rotate_a != 0 || rotate_b != 0) {
				//calculate axis-aligned bounding rects to
				//try to exclude any possible collision quickly.
				rect bounding_a, bounding_b;

				if(rotate_a == 0) {
					bounding_a = rect_a;
				} else {
					const int center_x = rect_a.x() + rect_a.w()/2;
					const int center_y = rect_a.y() + rect_a.h()/2;
					const int dim = std::max(rect_a.w(), rect_a.h());

					bounding_a = rect(center_x - dim/2 - 1, center_y - dim/2 - 1, dim+2, dim+2);
				}

				if(rotate_b == 0) {
					bounding_b = rect_b;
				} else {
					const int center_x = rect_b.x() + rect_b.w()/2;
					const int center_y = rect_b.y() + rect_b.h()/2;
					const int dim = std::max(rect_b.w(), rect_b.h());

					bounding_b = rect(center_x - dim/2 - 1, center_y - dim/2 - 1, dim+2, dim+2);
				}

				if(rects_intersect(bounding_a, bounding_b)) {
					const int Stride = 2;


					const float radians_to_degrees = 57.29577951308232087f;

					const float rot_a = rotate_a/radians_to_degrees;
					const float rot_b = rotate_b/radians_to_degrees;
					const float cos_a = cos(rot_a);
					const float sin_a = sin(rot_a);

					const float a_center_x = rect_a.x() + float(rect_a.w())*0.5f;
					const float a_center_y = rect_a.y() + float(rect_a.h())*0.5f;

					const float b_center_x = rect_b.x() + float(rect_b.w())*0.5f;
					const float b_center_y = rect_b.y() + float(rect_b.h())*0.5f;

					//there might be a collision. Do a rigorous check.
					for(int xpos = 1; xpos < rect_a.w() && !found; xpos += Stride) {
						for(int ypos = 1; ypos < rect_a.h(); ypos += Stride) {
							if(area_a.no_alpha_check == false && fa.isAlpha(xpos, ypos, time_a, a.isFacingRight())) {
								continue;
							}

							float a_x = static_cast<float>(rect_a.x() + xpos);
							float a_y = static_cast<float>(rect_a.y() + ypos);

							geometry::Point<float> p = rotate_point_around_origin_with_offset(a_x, a_y, rot_a, a_center_x, a_center_y, false);

							p = rotate_point_around_origin_with_offset(p.x, p.y, -rot_b, b_center_x, b_center_y, false);

							int b_x = static_cast<int>(p.x) - rect_b.x();
							int b_y = static_cast<int>(p.y) - rect_b.y();

							if(b_x < 0 || b_y < 0 || b_x >= rect_b.w() || b_y >= rect_b.h()) {
								continue;
							}

							if(area_b.no_alpha_check == false && fb.isAlpha(b_x, b_y, time_b, b.isFacingRight())) {
								continue;
							}

							fprintf(stderr, "COLLIDE: %d, %d / %d, %d\n", b_x, b_y, rect_b.w(), rect_b.h());

							found = true;
							break;
						}
					}
				}
			}

			//simple case of axis-aligned rectangles
			else if(rects_intersect(rect_a, rect_b)) {

				//we only check every other pixel, since this gives us
				//enough accuracy and is 4x faster.
				const int Stride = 2;
				const rect intersection = intersection_rect(rect_a, rect_b);
				for(int y = intersection.y(); y <= intersection.y2() && !found; y += Stride) {
					for(int x = intersection.x(); x <= intersection.x2(); x += Stride) {
						if((area_a.no_alpha_check || !fa.isAlpha(x - a.x(), y - a.y(), time_a, a.isFacingRight())) &&
						   (area_b.no_alpha_check || !fb.isAlpha(x - b.x(), y - b.y(), time_b, b.isFacingRight()))) {
							found = true;
							break;
						}
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

	return result;
}

bool entity_user_collision_specific_areas(const Entity& a, const std::string& area_a_id, const Entity& b, const std::string& area_b_id)
{
	if(&a == &b) {
		return false;
	}

	const Frame& fa = a.getCurrentFrame();
	const Frame& fb = b.getCurrentFrame();

	if(fa.getCollisionAreas().empty() || fb.getCollisionAreas().empty()) {
		return false;
	}

	if(!rects_intersect(rect(a.x(), a.y(), fa.width(), fa.height()),
	                    rect(b.x(), b.y(), fb.width(), fb.height()))) {
		return false;
	}

	const Frame::CollisionArea* area_a = nullptr;
	for(const auto& area : fa.getCollisionAreas()) {
		if(area.name == area_a_id) {
			area_a = &area;
			break;
		}
	}

	if(!area_a) {
		return false;
	}

	const Frame::CollisionArea* area_b = nullptr;
	for(const Frame::CollisionArea& area : fb.getCollisionAreas()) {
		if(area.name == area_b_id) {
			area_b = &area;
			break;
		}
	}

	if(!area_b) {
		return false;
	}

	rect rect_a(a.isFacingRight() ? a.x() + area_a->area.x() : a.x() + fa.width() - area_a->area.x() - area_a->area.w(),
	            a.y() + area_a->area.y(),
				area_a->area.w(), area_a->area.h());
	rect rect_b(b.isFacingRight() ? b.x() + area_b->area.x() : b.x() + fb.width() - area_b->area.x() - area_b->area.w(),
	            b.y() + area_b->area.y(),
				area_b->area.w(), area_b->area.h());
	if(!rects_intersect(rect_a, rect_b)) {
		return false;
	}

	const int time_a = a.getTimeInFrame();
	const int time_b = b.getTimeInFrame();

	const rect intersection = intersection_rect(rect_a, rect_b);
	for(int y = intersection.y(); y <= intersection.y2(); ++y) {
		for(int x = intersection.x(); x <= intersection.x2(); ++x) {
			if(!fa.isAlpha(x - a.x(), y - a.y(), time_a, a.isFacingRight()) &&
			   !fb.isAlpha(x - b.x(), y - b.y(), time_b, b.isFacingRight())) {
				return true;
			}
		}
	}

	return false;
}

namespace {
class UserCollisionCallable : public game_logic::FormulaCallable {
	EntityPtr a_, b_;
	const std::string* area_a_;
	const std::string* area_b_;
	int index_;
	variant all_collisions_;

	DECLARE_CALLABLE(UserCollisionCallable);
public:
	UserCollisionCallable(EntityPtr a, EntityPtr b, const std::string& area_a, const std::string& area_b, int index) 
		: a_(a), b_(b), area_a_(&area_a), area_b_(&area_b), index_(index) {
	}

	void setAllCollisions(variant v) {
		all_collisions_ = v;
	}
};

BEGIN_DEFINE_CALLABLE_NOBASE(UserCollisionCallable)
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
END_DEFINE_CALLABLE(UserCollisionCallable)

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

void detect_user_collisions(Level& lvl)
{
	std::vector<EntityPtr> chars;
	chars.reserve(lvl.get_active_chars().size());
	for(const EntityPtr& a : lvl.get_active_chars()) {
		if(a->getWeakCollideDimensions() != 0 && a->getCurrentFrame().getCollisionAreas().empty() == false) {
			chars.push_back(a);
		}
	}

	typedef std::pair<EntityPtr, const std::string*> collision_key;
	std::map<collision_key, std::vector<collision_key> > collision_info;

	static const int CollideObjectID = get_object_event_id("collide_object");

	const int MaxCollisions = 16;
	CollisionPair collision_buf[MaxCollisions];
	for(std::vector<EntityPtr>::const_iterator i = chars.begin(); i != chars.end(); ++i) {
		for(std::vector<EntityPtr>::const_iterator j = i + 1; j != chars.end(); ++j) {
			const EntityPtr& a = *i;
			const EntityPtr& b = *j;
			if(a == b ||
			   ((a->getWeakCollideDimensions()&b->getCollideDimensions()) == 0 &&
			   (a->getCollideDimensions()&b->getWeakCollideDimensions()) == 0)) {
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
		std::vector<ffl::IntrusivePtr<UserCollisionCallable> > v;
		std::vector<variant> all_callables;
		v.reserve(i->second.size());
		int index = 0;
		for(const collision_key& k : i->second) {
			v.push_back(ffl::IntrusivePtr<UserCollisionCallable>(new UserCollisionCallable(i->first.first, k.first, *i->first.second, *k.second, index)));
			all_callables.push_back(variant(v.back().get()));
			++index;
		}

		variant all_callables_variant(&all_callables);

		collision_key key = i->first;

		for(const ffl::IntrusivePtr<UserCollisionCallable>& p : v) {
			p->setAllCollisions(all_callables_variant);
			key.first->handleEventDelay(CollideObjectID, p.get());
			key.first->handleEventDelay(get_collision_event_id(*i->first.second), p.get());
		}

		for(const ffl::IntrusivePtr<UserCollisionCallable>& p : v) {
			//make sure we don't retain circular references.
			p->setAllCollisions(variant());
		}
	}

	for(std::vector<EntityPtr>::const_iterator i = chars.begin(); i != chars.end(); ++i) {
		const EntityPtr& a = *i;
		a->resolveDelayedEvents();
	}
}

bool is_flightpath_clear(const Level& lvl, const Entity& e, const rect& area)
{
	if(lvl.may_be_solid_in_rect(area)) {
		return false;
	}

	const std::vector<EntityPtr>& v = lvl.get_solid_chars();
	for(std::vector<EntityPtr>::const_iterator obj = v.begin();
	    obj != v.end(); ++obj) {
		if(obj->get() == &e) {
			continue;
		}

		if(rects_intersect(area, (*obj)->solidRect())) {
			return false;
		}
	}

	return true;
}
