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

#include "AttributeSet.hpp"
#include "SceneObject.hpp"

class RectRenderable : public KRE::SceneObject
{
public:
	explicit RectRenderable(bool strips=true, bool blend=false);
	void update(int x, int y, int w, int h, const KRE::Color& color);
	void update(const rect& r, const KRE::Color& color);
	void update(const rect& r, float rotation, const KRE::Color& color);
	void update(const std::vector<glm::u16vec2>& r, const KRE::Color& color);
	void update(std::vector<glm::u16vec2>* r, const KRE::Color& color);
private:
	std::shared_ptr<KRE::Attribute<glm::u16vec2>> r_;
};
