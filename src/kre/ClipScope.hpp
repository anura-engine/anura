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

#pragma once

#include <memory>
#include "geometry.hpp"
#include "CameraObject.hpp"
#include "DisplayDeviceFwd.hpp"
#include "Util.hpp"

namespace KRE
{
	class ClipScope
	{
	public:
		virtual ~ClipScope();
		static ClipScopePtr create(const rect& r);

		virtual void apply(const CameraPtr& cam) const = 0;
		virtual void clear() const = 0;

		struct Manager
		{
			Manager(const rect& r, const CameraPtr& cam=nullptr) : cs(ClipScope::create(r)) {
				cs->apply(cam);
			}
			~Manager() {
				cs->clear();
			}
			ClipScopePtr cs;
		};

		const rectf& area() const { return area_; }
	protected:
		ClipScope(const rect& r);
	private:
		DISALLOW_COPY_ASSIGN_AND_DEFAULT(ClipScope);
		rectf area_;
	};

	class ClipShapeScope
	{
	public:
		virtual ~ClipShapeScope();
		static ClipShapeScopePtr create(const RenderablePtr& r);

		virtual void apply(const CameraPtr& cam) const = 0;
		virtual void clear() const = 0;

		struct Manager
		{
			Manager(const RenderablePtr& r, const CameraPtr& cam=nullptr) 
				: cs(r != nullptr ? ClipShapeScope::create(r) : nullptr)
			{
				if(cs) {
					cs->apply(cam);
				}
			}
			~Manager() {
				if(cs) {
					cs->clear();
				}
			}
			ClipShapeScopePtr cs;
		};

		const RenderablePtr& getRenderable() const { return r_; }
	protected:
		ClipShapeScope(const RenderablePtr& r);
	private:
		DISALLOW_COPY_ASSIGN_AND_DEFAULT(ClipShapeScope);
		RenderablePtr r_;
	};
}
