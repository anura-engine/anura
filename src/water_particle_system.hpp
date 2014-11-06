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
#ifndef water_PARTICLE_SYSTEM_H
#define water_PARTICLE_SYSTEM_H

#include "graphics.hpp"

#include <deque>

#include "color_utils.hpp"
#include "foreach.hpp"
#include "geometry.hpp"
#include "entity.hpp"
#include "particle_system.hpp"
#include "variant.hpp"

struct water_particle_system_info {
	water_particle_system_info(variant node);

	int number_of_particles;
	int repeat_period;
	int velocity_x, velocity_y;
	int velocity_rand;
	int dot_size;
	
	union {
		uint8_t rgba[4];
		uint32_t irgba;
	};
};

class water_particle_system_factory : public particle_system_factory {
public:
	explicit water_particle_system_factory(variant node);
	~water_particle_system_factory() {}
	
	particle_system_ptr create(const entity& e) const;
	water_particle_system_info info;
};

class water_particle_system : public particle_system
{
public:
	water_particle_system(const entity& e, const water_particle_system_factory& factory);
	
	bool is_destroyed() const { return false; }
	void process(const entity& e);
	void draw(const rect& area, const entity& e) const;
	
private:
	variant get_value(const std::string& key) const { return variant(); }
	void set_value(const std::string& key, const variant& value);	
	
	const water_particle_system_factory& factory_;
	const water_particle_system_info& info_;
	
	int cycle_;
	
	rect area_;
	
	struct particle {
		GLfloat pos[2];
		GLfloat velocity;
	};
	
	GLfloat direction[2];
	GLfloat base_velocity;
	int velocity_x_, velocity_y_;
	
	
	std::vector<particle> particles_;
};

#endif
