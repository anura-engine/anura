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

#include "entity_fwd.hpp"
#include "level_solid_map.hpp"
#include "solid_map.hpp"

class Level;

int get_num_solid_dimensions();
const std::string& get_solid_dimension_key(int id);
int get_solid_dimension_id(const std::string& key);

//struct which provides information about a surface we collide with.
struct CollisionInfo {
	CollisionInfo() 
		: surf_info(0), friction(0), traction(0), damage(0), 
		adjust_y(0), platform(false), area_id(0), 
		collide_with_area_id(0)
	{}

	void readSurfInfo();

	const SurfaceInfo* surf_info;
	int friction;
	int traction;
	int damage;

	//adjustment that should take place of the colliding object's position.
	//the reason for this is if the object is moving downwards, and at the
	//same time a platform is moving upwards. The platform will NOT check
	//for the downwards-moving object standing on it during its cycle, so
	//on the downwards-moving object's cycle it may already be below where
	//the platform is. This adjusts it so it is on top of the platform again.
	int adjust_y;

	//true iff the collided with area is a platform, rather than solid.
	bool platform;

	//the ID of the area of our body which collided.
	const std::string* area_id;

	//the object, if any, that we collided with. nullptr if we collided with
	//a tile in the level.
	EntityPtr collide_with;

	//if collide_with is non-null this will contain the ID of the area
	//that we collided with.
	const std::string* collide_with_area_id;
};

//value of what kind of collision we are looking for. If we only want to
//know if there is a collision with solid space, or with platforms as well.
enum ALLOW_PLATFORM { SOLID_ONLY, SOLID_AND_PLATFORMS };

//function which finds it a given point can be stood on.
bool point_standable(const Level& lvl, const Entity& e, int x, int y, CollisionInfo* info=nullptr, ALLOW_PLATFORM allow_platform=SOLID_AND_PLATFORMS);

bool point_standable(const Level& lvl, const Entity& e, const std::vector<EntityPtr>& chars, int x, int y, CollisionInfo* info=nullptr, ALLOW_PLATFORM allow_platform=SOLID_AND_PLATFORMS);

//Get a vector of objects that might be standable within a given area. This can be used to give to
//subsequent calls to point_standable()
std::vector<EntityPtr> get_potentially_standable_objects_in_area(const Level& lvl, const Entity& e, const rect& area, ALLOW_PLATFORM allow_platform=SOLID_AND_PLATFORMS);

//function which finds if an Entity's solid area collides with anything, when
//the object has just moved one pixel in the direction given by 'dir'. If
//'dir' is MOVE_DIRECTION::NONE, then all pixels will be checked.
bool entity_collides(Level& lvl, const Entity& e, MOVE_DIRECTION dir, CollisionInfo* info=nullptr);

void debug_check_entity_solidity(const Level& lvl, const Entity& e);

//function which finds if one Entity collides with another given Entity.
bool entity_collides_with_entity(const Entity& e, const Entity& other, CollisionInfo* info=nullptr);


//function which finds if an Entity collides with a level tile.
bool entity_collides_with_level(const Level& lvl, const Entity& e, MOVE_DIRECTION dir, CollisionInfo* info=nullptr);

//function which finds how many pixels in an Entity collide with the level.
//this is generally used for debug purposes.
int entity_collides_with_level_count(const Level& lvl, const Entity& e, MOVE_DIRECTION dir);

//function to try placing an Entity in a level, without it colliding. The Entity
//may be moved according to some heuristics to place it sensibly -- the object's
//location will be modified. Will return true iff it succeeds in placing it.
bool place_entity_in_level(Level& lvl, Entity& e);

//function to try to place an Entity in a level, prioritizing finding a place
//to put it over keeping it near its starting point.
bool place_entity_in_level_with_large_displacement(Level& lvl, Entity& e);

//function which returns true iff an Entity collides with the level in
//'non-solid' space. That is, if any of the Entity's pixels collide with
//level solid space.
bool non_solid_entity_collides_with_level(const Level& lvl, const Entity& e);

//function which detects user collisions between two entities. All
//collision areas on the objects will be checked, and the results stored
//in areas_colliding. The function will return the number of collision
//combinations that were found.
typedef std::pair<const std::string*, const std::string*> CollisionPair;
int entity_user_collision(const Entity& a, const Entity& b, CollisionPair* areas_colliding, int buf_size);

//function which returns true iff area_a of 'a' collides with area_b of 'b'
bool entity_user_collision_specific_areas(const Entity& a, const std::string& area_a, const Entity& b, const std::string& area_b);

//function to detect all user collisions and fire appropriate events to
//the colliding objects.
void detect_user_collisions(Level& lvl);

bool is_flightpath_clear(const Level& lvl, const Entity& e, const rect& area);
