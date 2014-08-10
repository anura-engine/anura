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

#include "kre/DisplayDevice.hpp"
#include "kre/WindowManager.hpp"

#include <cstdio>
#include <cmath>
#include <vector>

#include "preferences.hpp"
#include "water_particle_system.hpp"

WaterParticleSystemInfo::WaterParticleSystemInfo(variant node)
	: number_of_particles(node["number_of_particles"].as_int(1500)),
	repeat_period(node["repeat_period"].as_int(1000)),
	velocity_x(node["velocity_x"].as_int()),
	velocity_y(node["velocity_y"].as_int(-5)),
	velocity_rand(node["velocity_rand"].as_int(3)),
	dot_size(node["dot_size"].as_int(1)*(preferences::double_scale() ? 2 : 1))
{
	color = KRE::Color(node["color"]);

	if(dot_size > 1 && preferences::xypos_draw_mask) {
		//if we are clipping our drawing granularity, then we have a small
		//enough screen that we want to shrink the particles.
		dot_size /= 2;
	}
}

WaterParticleSystemFactory::WaterParticleSystemFactory(variant node)
	: info(node)
{
	
}

ParticleSystemPtr WaterParticleSystemFactory::create(const Entity& e) const
{
	return ParticleSystemPtr(new WaterParticleSystem(e, *this));
}


WaterParticleSystem::WaterParticleSystem(const Entity& e, const WaterParticleSystemFactory& factory)
	: factory_(factory), 
	info_(factory.info), 
	velocity_x_(factory.info.velocity_x), 
	velocity_y_(factory.info.velocity_y), 
	cycle_(0)
{
	area_ = rect("0,0,1,1");
	base_velocity = sqrtf(static_cast<float>(info_.velocity_x*info_.velocity_x + info_.velocity_y*info_.velocity_y));
	direction[0] = velocity_x_ / base_velocity;
	direction[1] = velocity_y_ / base_velocity;
	particles_.reserve(info_.number_of_particles);
	for (int i = 0; i < info_.number_of_particles; i++)
	{
		particle new_p;
		new_p.pos[0] = static_cast<float>(rand()%info_.repeat_period);
		new_p.pos[1] = static_cast<float>(rand()%info_.repeat_period);
		new_p.velocity = base_velocity + (info_.velocity_rand ? (rand() % info_.velocity_rand) : 0);
		particles_.push_back(new_p);
	}

	auto as = KRE::DisplayDevice::createAttributeSet(true, false, true);
	as->setDrawMode(KRE::DrawMode::POINTS);
	addAttributeSet(as);
	attribs_ = std::make_shared<KRE::Attribute<glm::u16vec2>>(KRE::AccessFreqHint::DYNAMIC);
	attribs_->addAttributeDesc(KRE::AttributeDesc(KRE::AttrType::POSITION, 2, KRE::AttrFormat::UNSIGNED_SHORT, false));
	as->addAttribute(attribs_);
}

void WaterParticleSystem::process(const Entity& e)
{
	++cycle_;
	
	for(particle& p : particles_)
	{		
		p.pos[0] = fmod(p.pos[0]+direction[0] * p.velocity, static_cast<float>(info_.repeat_period));
		p.pos[1] = fmod(p.pos[1]+direction[1] * p.velocity, static_cast<float>(info_.repeat_period));
	}
	

	// XXX set point size (radius) uniform from "info_.dot_size" here
	// XXX set is_circlular uniform to false/true
	setColor(info_.color);

}

void WaterParticleSystem::doAttach(const KRE::DisplayDevicePtr& dd, KRE::DisplayDeviceDef* def) {
	def->setHint("shader", "point_shader");
}


void WaterParticleSystem::draw(const KRE::WindowManagerPtr& wm, const rect& screen_area, const Entity& e) const
{
	const rect area = intersection_rect(screen_area, area_);
	if(area.w() == 0 || area.h() == 0 || particles_.empty()) {
		return;
	}

	int offset_x = area.x() - area.x()%info_.repeat_period;
	if (area.x() < 0) offset_x -= info_.repeat_period;
	int offset_y = area.y() - area.y()%info_.repeat_period;
	if (area.y() < 0) offset_y -= info_.repeat_period;
	std::vector<glm::u16vec2> vertices;
	for(const particle& p : particles_)
	{
		short my_y = static_cast<short>(p.pos[1]+offset_y);
		short xpos = static_cast<short>(p.pos[0]+offset_x);
		while(my_y < area_.y()) {
			my_y += info_.repeat_period;
		}

		while(xpos < area_.x()) {
			xpos += info_.repeat_period;
		}

		if(my_y > area_.y2() || xpos > area_.x2()) {
			continue;
		}

		while(my_y <= area.y2()) {
			short my_x = xpos;

			while (my_x <= area.x2()) {
				vertices.emplace_back(my_x, my_y);
				my_x += info_.repeat_period;
			}
			my_y += info_.repeat_period;
		}
	}
	attribs_->update(&vertices);

	wm->render(this);
}

BEGIN_DEFINE_CALLABLE(WaterParticleSystem, ParticleSystem)
	DEFINE_FIELD(area, "[int]")
		return obj.area_.write();
	DEFINE_SET_FIELD_TYPE("[int]|string")
		obj.area_ = rect(value);
		
	DEFINE_FIELD(velocity_x, "int")
		return variant(obj.velocity_x_);
	DEFINE_SET_FIELD
		obj.velocity_x_ = value.as_int();
		obj.direction[0] = obj.velocity_x_ / obj.base_velocity;
		obj.direction[1] = obj.velocity_y_ / obj.base_velocity;

	DEFINE_FIELD(velocity_y, "int")
		return variant(obj.velocity_y_);
	DEFINE_SET_FIELD
		obj.velocity_y_ = value.as_int();
		obj.direction[0] = obj.velocity_x_ / obj.base_velocity;
		obj.direction[1] = obj.velocity_y_ / obj.base_velocity;
END_DEFINE_CALLABLE(WaterParticleSystem)
