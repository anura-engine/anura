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

#include <cairo.h>
#include <memory>
#include <vector>

#include "svg_attribs.hpp"
#include "svg_length.hpp"

namespace KRE
{
	template<typename T>
	T clamp(T value, T min_val, T max_val) 
	{
		return std::max(std::min(max_val, value), min_val);
	}

	namespace SVG
	{
		class shapes;
		typedef std::shared_ptr<shapes> shapes_ptr;
		typedef std::shared_ptr<const shapes> const_shapes_ptr;

		class element;
		typedef std::shared_ptr<element> element_ptr;

		class transform;
		typedef std::shared_ptr<transform> transform_ptr;

		class container;
		typedef std::shared_ptr<container> container_ptr;

		typedef std::vector<std::pair<svg_length,svg_length>> point_list;

	}
}
