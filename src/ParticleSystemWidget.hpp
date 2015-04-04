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

#include "widget.hpp"

namespace gui
{
	class ParticleSystemWidget : public Widget
	{
		public:
			explicit ParticleSystemWidget(const variant& v, game_logic::FormulaCallable* e);
			WidgetPtr clone() const override;
		private:
			DECLARE_CALLABLE(ParticleSystemWidget);
			virtual void handleDraw() const override;
			virtual void handleProcess() override;

			int last_process_time_;
			KRE::Particles::ParticleSystemContainerPtr container_;
			KRE::SceneGraphPtr scene_;
			KRE::SceneNodePtr root_;
			KRE::RenderManagerPtr rmanager_;

			ParticleSystemWidget() = delete;
			void operator=(const ParticleSystemWidget&);
	};
}

