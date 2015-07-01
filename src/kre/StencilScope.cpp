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
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include "DisplayDevice.hpp"
#include "StencilScope.hpp"

namespace KRE
{
	const StencilSettings& get_stencil_mask_settings() 
	{
		static const KRE::StencilSettings ss(true, 
			KRE::StencilFace::FRONT_AND_BACK, 
			KRE::StencilFunc::NOT_EQUAL, 
			0xff, 0x00, 0xff, 
			KRE::StencilOperation::INCREMENT, 
			KRE::StencilOperation::KEEP, 
			KRE::StencilOperation::KEEP);
		return ss;
	}

	const StencilSettings& get_stencil_keep_settings()
	{
		static const StencilSettings keep_stencil_settings(true,
			StencilFace::FRONT_AND_BACK, 
			StencilFunc::EQUAL, 
			0xff,
			0x01,
			0x00,
			StencilOperation::KEEP,
			StencilOperation::KEEP,
			StencilOperation::KEEP);
		return keep_stencil_settings;
	};


	StencilScope::StencilScope(const StencilSettings& settings)
		: settings_(settings)
	{
	}

	StencilScope::~StencilScope()
	{
	}

	StencilScopePtr StencilScope::create(const StencilSettings& settings)
	{
		return DisplayDevice::getCurrent()->createStencilScope(settings);
	}
}
