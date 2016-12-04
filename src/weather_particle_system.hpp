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

#include <deque>

#include "AttributeSet.hpp"

#include "particle_system.hpp"

struct WeatherParticleSystemInfo 
{
	WeatherParticleSystemInfo(variant node)
	: number_of_particles(node["number_of_particles"].as_int(1500)),
	repeat_period(node["repeat_period"].as_int(1000)),
	velocity_x(node["velocity_x"].as_int()),
	velocity_y(node["velocity_y"].as_int(5)),
	velocity_rand(node["velocity_rand"].as_int(3)),
	line_width(node["line_width"].as_int(1)),
	line_length(node["line_length"].as_int(8))
	{
		color = KRE::Color(node["color"]);
	}
	int number_of_particles;
	int repeat_period;
	int velocity_x, velocity_y;
	int velocity_rand;
	int line_width, line_length;
	
	KRE::Color color;
};

class WeatherParticleSystemFactory : public ParticleSystemFactory 
{
public:
	explicit WeatherParticleSystemFactory(variant node);
	~WeatherParticleSystemFactory() {}
	
	ParticleSystemPtr create(const Entity& e) const override;
	WeatherParticleSystemInfo info;
};

class WeatherParticleSystem : public ParticleSystem
{
public:
	WeatherParticleSystem(const Entity& e, const WeatherParticleSystemFactory& factory);
	
	bool isDestroyed() const override { return false; }
	void process(const Entity& e) override;
	void draw(const KRE::WindowPtr& wm, const rect& area, const Entity& e) const override;
private:
	DECLARE_CALLABLE(WeatherParticleSystem);
	
	const WeatherParticleSystemFactory& factory_;
	const WeatherParticleSystemInfo& info_;
	
	int cycle_;
	
	struct particle {
		float pos[2];
		float velocity;
	};
	
	float direction[2];
	float base_velocity;

	std::shared_ptr<KRE::Attribute<glm::vec2>> attribs_;

	std::vector<particle> particles_;
};
