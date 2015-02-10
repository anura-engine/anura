/*
	Copyright (C) 2003-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "geometry.hpp"
#include <memory>

namespace KRE
{
	class Scissor;
	typedef std::shared_ptr<Scissor> ScissorPtr;

	class Scissor
	{
	public:
		Scissor(const rect& area);
		virtual ~Scissor();
		void setArea(const rect& area) { area_ = area; }
		const rect& getArea() const { return area_; }

		virtual void apply() = 0;
		virtual void clear() = 0;

		class Manager
		{
			ScissorPtr instance_;
		public:
			Manager(const rect& area);
			~Manager();
		};
	private:
		rect area_;
		static ScissorPtr getInstance(const rect& area);
	};
}
