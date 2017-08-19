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
#include "RenderFwd.hpp"
#include "SceneFwd.hpp"
#include "WindowManagerFwd.hpp"

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

namespace graphics
{
	class ParticleSystemContainerProxy : public game_logic::FormulaCallable
	{
	public:
		ParticleSystemContainerProxy(const variant& node);

		void draw(const KRE::WindowPtr& wnd) const;
		void process();

		void surrenderReferences(GarbageCollector* collector) override;

		glm::vec3& get_last_translation() { return last_translation_; }
	private:
		DECLARE_CALLABLE(ParticleSystemContainerProxy);

		const KRE::Particles::Emitter& getActiveEmitter() const;
		KRE::Particles::Emitter& getActiveEmitter();

		KRE::Particles::ParticleSystemContainerPtr particle_system_container_;
		KRE::SceneGraphPtr scene_;
		KRE::SceneNodePtr root_;
		KRE::RenderManagerPtr rmanager_;
		glm::vec3 last_translation_;
		int last_process_time_;

		bool running_;
		mutable bool enable_mouselook_;
		mutable bool invert_mouselook_;
	};

	typedef ffl::IntrusivePtr<ParticleSystemContainerProxy> ParticleSystemContainerProxyPtr; 
}

