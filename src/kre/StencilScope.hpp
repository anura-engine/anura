/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <memory>
#include "DisplayDeviceFwd.hpp"
#include "StencilSettings.hpp"
#include "Util.hpp"

namespace KRE
{
	class StencilScope
	{
	public:
		virtual ~StencilScope();
		void applyNewSettings(const StencilSettings& settings) { settings_ = settings; handleUpdatedSettings(); }
		void updateMask(unsigned mask) { settings_.setMask(mask); handleUpdatedMask();} 
		const StencilSettings& getSettings() { return settings_; }
		static StencilScopePtr create(const StencilSettings& settings);
	protected:
		StencilScope(const StencilSettings& settings);
	private:
		virtual void handleUpdatedMask() = 0;
		virtual void handleUpdatedSettings() = 0;

		DISALLOW_COPY_AND_ASSIGN(StencilScope);
		StencilSettings settings_;
	};
}
