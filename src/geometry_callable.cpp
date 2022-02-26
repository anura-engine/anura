/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>

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

#include "geometry_callable.hpp"

BEGIN_DEFINE_CALLABLE_NOBASE(RectCallable)
DEFINE_FIELD(x, "int")
	return variant(obj.rect_.x());
DEFINE_FIELD(y, "int")
	return variant(obj.rect_.y());
DEFINE_FIELD(x2, "int")
	return variant(obj.rect_.x2());
DEFINE_FIELD(y2, "int")
	return variant(obj.rect_.y2());
DEFINE_FIELD(w, "int")
	return variant(obj.rect_.w());
DEFINE_FIELD(h, "int")
	return variant(obj.rect_.h());
END_DEFINE_CALLABLE(RectCallable)
