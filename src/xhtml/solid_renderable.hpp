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
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include "geometry.hpp"

#include "AttributeSet.hpp"
#include "SceneObject.hpp"

namespace xhtml
{
	class SimpleRenderable : public KRE::SceneObject
	{
	public:
		SimpleRenderable();
		explicit SimpleRenderable(KRE::DrawMode draw_mode);
		void init(KRE::DrawMode draw_mode = KRE::DrawMode::TRIANGLES);
		void update(std::vector<glm::vec2>* coords);
		void setDrawMode(KRE::DrawMode draw_mode);
	private:
		std::shared_ptr<KRE::Attribute<glm::vec2>> attribs_;
	};
	typedef std::shared_ptr<SimpleRenderable> SimpleRenderablePtr;

	class SolidRenderable : public KRE::SceneObject
	{
	public:
		SolidRenderable();
		explicit SolidRenderable(const rect& r, const KRE::ColorPtr& color=nullptr);
		explicit SolidRenderable(const rectf& r, const KRE::ColorPtr& color=nullptr);
		void init();
		void update(std::vector<KRE::vertex_color>* coords);
		void setColorPointer(const KRE::ColorPtr& color) { color_ = color; }
		void setDrawMode(KRE::DrawMode draw_mode);
		void preRender(const KRE::WindowPtr& wnd);
	private:
		std::shared_ptr<KRE::Attribute<KRE::vertex_color>> attribs_;
		KRE::ColorPtr color_;
	};
	typedef std::shared_ptr<SolidRenderable> SolidRenderablePtr;
}