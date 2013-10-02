/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "SDL.h"

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

namespace haptic
{
	class HapticEffectCallable : public game_logic::formula_callable
	{
	public:
		HapticEffectCallable(const std::string& name, const variant& eff);
		virtual ~HapticEffectCallable();

		void load(const std::string& name, const variant& eff);
	private:
		DECLARE_CALLABLE(HapticEffectCallable);

		SDL_HapticEffect effect_;
	};

	typedef boost::intrusive_ptr<HapticEffectCallable> HapticEffectCallablePtr;

	void play(const std::string&, int iters);
	void stop(const std::string&);
	void stop_all();
}
