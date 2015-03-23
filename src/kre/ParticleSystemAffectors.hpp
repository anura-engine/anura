/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include "ParticleSystemFwd.hpp"

namespace KRE
{
	namespace Particles
	{
		class Affector : public EmitObject
		{
		public:
			explicit Affector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			virtual ~Affector();
			virtual AffectorPtr clone() const = 0;

			bool isEnabled() const { return enabled_; }
			void enable(bool en) { enabled_ = en; }
			TechniquePtr getTechnique() const;
			void setParentTechnique(std::weak_ptr<Technique> tq) { technique_ = tq; }

			float mass() const { return mass_; }
			const glm::vec3& getPosition() const { return position_; }
			void setPosition(const glm::vec3& pos) { position_ = pos; }
			const glm::vec3& getScale() const { return scale_; }
			bool isEmitterExcluded(const std::string& name) const;

			static AffectorPtr factory(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
		protected:
			virtual void handleEmitProcess(float t);
			virtual void internalApply(Particle& p, float t) = 0;
		private:
			bool enabled_;
			float mass_;
			glm::vec3 position_;
			std::vector<std::string> excluded_emitters_;
			glm::vec3 scale_;
			std::weak_ptr<Technique> technique_;
			Affector();
		};
	}
}
