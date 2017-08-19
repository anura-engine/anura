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

#include "particle_system.hpp"

struct WaterParticleSystemInfo 
{
	WaterParticleSystemInfo(variant node);

	int number_of_particles;
	int repeat_period;
	int velocity_x, velocity_y;
	int velocity_rand;
	int dot_size;
	KRE::Color color;
};

class WaterParticleSystemFactory : public ParticleSystemFactory 
{
public:
	explicit WaterParticleSystemFactory(variant node);
	~WaterParticleSystemFactory() {}
	
	ParticleSystemPtr create(const Entity& e) const override;
	WaterParticleSystemInfo info;
};

class WaterParticleSystem : public ParticleSystem
{
public:
	WaterParticleSystem(const Entity& e, const WaterParticleSystemFactory& factory);
	
	bool isDestroyed() const override { return false; }
	void process(const Entity& e) override;
	void draw(const KRE::WindowPtr& wm, const rect& area, const Entity& e) const override;

	void executeOnDraw();
private:
	DECLARE_CALLABLE(WaterParticleSystem);

	const WaterParticleSystemFactory& factory_;
	const WaterParticleSystemInfo& info_;
	
	int cycle_;
	
	rect area_;
	
	struct particle {
		float pos[2];
		float velocity;
	};
	
	float direction[2];
	float base_velocity;
	int velocity_x_, velocity_y_;
	
	std::shared_ptr<KRE::Attribute<glm::u16vec2>> attribs_;
	
	std::vector<particle> particles_;
	int u_point_size_;
};
