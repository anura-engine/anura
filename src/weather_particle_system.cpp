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
#include <cstdio>
#include <math.h>
#include <vector>

#include "weather_particle_system.hpp"
#include "variant_utils.hpp"

weather_particle_system_factory::weather_particle_system_factory (variant node)
 : info(node)
{
	
}

particle_system_ptr weather_particle_system_factory::create(const entity& e) const
{
	return particle_system_ptr(new weather_particle_system(e, *this));
}


weather_particle_system::weather_particle_system(const entity& e, const weather_particle_system_factory& factory)
 : factory_(factory), info_(factory.info), cycle_(0)
{
	base_velocity = sqrtf(info_.velocity_x*info_.velocity_x + info_.velocity_y*info_.velocity_y);
	direction[0] = info_.velocity_x / base_velocity;
	direction[1] = info_.velocity_y / base_velocity;
	particles_.reserve(info_.number_of_particles);
	for (int i = 0; i < info_.number_of_particles; i++)
	{
		particle new_p;
		new_p.pos[0] = rand()%info_.repeat_period;
		new_p.pos[1] = rand()%info_.repeat_period;
		new_p.velocity = base_velocity + (info_.velocity_rand ? (rand() % info_.velocity_rand) : 0);
		particles_.push_back(new_p);
	}
}

void weather_particle_system::process(const entity& e)
{
	++cycle_;
	
	foreach(particle& p, particles_)
	{
		p.pos[0] = static_cast<int>(p.pos[0]+direction[0] * p.velocity) % info_.repeat_period;
		p.pos[1] = static_cast<int>(p.pos[1]+direction[1] * p.velocity) % info_.repeat_period;
	}
	
	//while (particles_.size() > 1500) particles_.pop_front();
}

void weather_particle_system::draw(const rect& area, const entity& e) const
{
#if !defined(USE_SHADERS)
	glDisable(GL_TEXTURE_2D);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
#endif
	glLineWidth(info_.line_width);
	glColor4f(info_.rgba[0]/255.0, info_.rgba[1]/255.0, info_.rgba[2]/255.0, info_.rgba[3]/255.0);
	int offset_x = area.x() - area.x()%info_.repeat_period;
	if (area.x() < 0) offset_x -= info_.repeat_period;
	int offset_y = area.y() - area.y()%info_.repeat_period;
	if (area.y() < 0) offset_y -= info_.repeat_period;
	static std::vector<GLfloat> vertices;
	vertices.clear();
	foreach(const particle& p, particles_)
	{
		float my_y = p.pos[1]+offset_y;
		do
		{
			float my_x = p.pos[0]+offset_x;
			do
			{
				vertices.push_back(my_x);
				vertices.push_back(my_y);
				vertices.push_back(my_x+direction[0]*info_.line_length);
				vertices.push_back(my_y+direction[1]*info_.line_length);
				my_x += info_.repeat_period;
				//printf("my_x: %f, area.x: %i, area.w: %i\n", my_x, area.x(), area.w());
			} while (my_x < area.x()+area.w());
			my_y += info_.repeat_period;
		} while (my_y < area.y()+area.h());
	}
#if defined(USE_SHADERS)
	gles2::manager gles2_manager(gles2::get_simple_shader());
	gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &vertices.front());
#else
	glVertexPointer(2, GL_FLOAT, 0, &vertices.front());
#endif
	glDrawArrays(GL_LINES, 0, vertices.size()/2);
#if !defined(USE_SHADERS)
	//glDisable(GL_SMOOTH);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnable(GL_TEXTURE_2D);
#endif
	glColor4f(1.0, 1.0, 1.0, 1.0);
}

variant weather_particle_system::get_value(const std::string& key) const
{
	if(key == "velocity_x") {
		return variant(decimal(direction[0]));
	} else if(key == "velocity_y") {
		return variant(decimal(direction[1]));
	} else {
		return variant();
	}
}

void weather_particle_system::set_value(const std::string& key, const variant& value)
{
	if(key == "velocity_x") {
		direction[0] = value.as_decimal().as_float();
	} else if(key == "velocity_y") {
		direction[1] = value.as_decimal().as_float();
	}
}

