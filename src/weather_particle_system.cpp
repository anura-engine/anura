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
#include "Shaders.hpp"
#include "WindowManager.hpp"

#include <cstdio>
#include <cmath>
#include <vector>

#include "weather_particle_system.hpp"
#include "variant_utils.hpp"

WeatherParticleSystemFactory::WeatherParticleSystemFactory (variant node)
 : info(node)
{
	
}

ParticleSystemPtr WeatherParticleSystemFactory::create(const Entity& e) const
{
	return ParticleSystemPtr(new WeatherParticleSystem(e, *this));
}


WeatherParticleSystem::WeatherParticleSystem(const Entity& e, const WeatherParticleSystemFactory& factory)
 : factory_(factory), info_(factory.info), cycle_(0)
{
	base_velocity = sqrtf(static_cast<float>(info_.velocity_x*info_.velocity_x + info_.velocity_y*info_.velocity_y));
	direction[0] = info_.velocity_x / base_velocity;
	direction[1] = info_.velocity_y / base_velocity;
	particles_.reserve(info_.number_of_particles);
	for (int i = 0; i < info_.number_of_particles; i++)
	{
		particle new_p;
		new_p.pos[0] = static_cast<float>(rand()%info_.repeat_period);
		new_p.pos[1] = static_cast<float>(rand()%info_.repeat_period);
		new_p.velocity = base_velocity + (info_.velocity_rand ? (rand() % info_.velocity_rand) : 0);
		particles_.push_back(new_p);
	}

	setShader(KRE::ShaderProgram::getProgram("line_shader"));
	auto as = KRE::DisplayDevice::createAttributeSet(true, false, false);
	as->setDrawMode(KRE::DrawMode::POINTS);
	attribs_ = std::make_shared<KRE::Attribute<glm::vec2>>(KRE::AccessFreqHint::DYNAMIC);
	attribs_->addAttributeDesc(KRE::AttributeDesc(KRE::AttrType::POSITION, 2, KRE::AttrFormat::FLOAT, false));
	as->addAttribute(attribs_);
	addAttributeSet(as);
}

void WeatherParticleSystem::process(const Entity& e)
{
	++cycle_;
	
	for(particle& p : particles_)
	{
		p.pos[0] = static_cast<float>(static_cast<int>(p.pos[0]+direction[0] * p.velocity) % info_.repeat_period);
		p.pos[1] = static_cast<float>(static_cast<int>(p.pos[1]+direction[1] * p.velocity) % info_.repeat_period);
	}

	// XXX set line width uniform from "info_.line_width" here
	static auto u_line_width = getShader()->getUniform("line_width");
	if(u_line_width != KRE::ShaderProgram::INALID_UNIFORM) {
		getShader()->setUniformValue(u_line_width, info_.line_width);
	}
	setColor(info_.color);
}

void WeatherParticleSystem::draw(const KRE::WindowPtr& wm, const rect& area, const Entity& e) const
{
	int offset_x = area.x() - area.x()%info_.repeat_period;
	if (area.x() < 0) offset_x -= info_.repeat_period;
	int offset_y = area.y() - area.y()%info_.repeat_period;
	if (area.y() < 0) offset_y -= info_.repeat_period;
	std::vector<glm::vec2> vertices;
	for(const particle& p : particles_)
	{
		float my_y = p.pos[1]+offset_y;
		do
		{
			float my_x = p.pos[0]+offset_x;
			do
			{
				vertices.emplace_back(my_x, my_y);
				vertices.emplace_back(my_x+direction[0]*info_.line_length, my_y+direction[1]*info_.line_length);
				my_x += info_.repeat_period;
			} while (my_x < area.x()+area.w());
			my_y += info_.repeat_period;
		} while (my_y < area.y()+area.h());
	}
	getAttributeSet().back()->setCount(vertices.size());
	attribs_->update(&vertices);

	wm->render(this);
}

BEGIN_DEFINE_CALLABLE(WeatherParticleSystem, ParticleSystem)
	DEFINE_FIELD(velocity_x, "decimal|int")
		return variant(decimal(obj.direction[0]));
	DEFINE_SET_FIELD
		obj.direction[0] = value.as_float();	

	DEFINE_FIELD(velocity_y, "decimal|int")
		return variant(decimal(obj.direction[1]));
	DEFINE_SET_FIELD
		obj.direction[1] = value.as_float();	
END_DEFINE_CALLABLE(WeatherParticleSystem)
