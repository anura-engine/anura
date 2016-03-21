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

#include "ClipScope.hpp"
#include "DisplayDevice.hpp"

namespace KRE
{
	ClipScope::ClipScope(const rect& r)
		: area_(r.as_type<float>())
	{
	}

	ClipScope::~ClipScope()
	{
	}

	ClipScopePtr ClipScope::create(const rect& r)
	{
		return DisplayDevice::getCurrent()->createClipScope(r);
	}

	ClipShapeScope::ClipShapeScope(const RenderablePtr& r)
		: r_(r)
	{
	}

	ClipShapeScope::~ClipShapeScope()
	{
	}

	ClipShapeScopePtr ClipShapeScope::create(const RenderablePtr& r)
	{
		return DisplayDevice::getCurrent()->createClipShapeScope(r);
	}
}
