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

#include "Blittable.hpp"
#include "DisplayDevice.hpp"

namespace KRE
{
	Blittable::Blittable()
		: SceneObject("blittable"),
		  centre_(Centre::TOP_LEFT),
		  centre_offset_(),
		  changed_(false),
		  horizontal_mirrored_(false),
		  vertical_mirrored_(false)
	{
		init();
	}

	Blittable::Blittable(const TexturePtr& tex)
		: SceneObject("blittable"),
		  centre_(Centre::TOP_LEFT),
		  centre_offset_(),
		  changed_(true),
		  horizontal_mirrored_(false),
		  vertical_mirrored_(false)
	{
		setTexture(tex);
		init();
	}

	Blittable::Blittable(const variant& node)
		: SceneObject(node),
		  centre_(Centre::TOP_LEFT),
		  centre_offset_(),
		  changed_(true),
		  horizontal_mirrored_(false),
		  vertical_mirrored_(false)
	{
		init();
	}

	Blittable::~Blittable()
	{
	}

	void Blittable::init()
	{
		auto as = DisplayDevice::createAttributeSet();
		attribs_.reset(new Attribute<vertex_texcoord>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
		attribs_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, vtx)));
		attribs_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE,  2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, tc)));
		as->addAttribute(AttributeBasePtr(attribs_));
		as->setDrawMode(DrawMode::TRIANGLE_STRIP);
		
		addAttributeSet(as);
	}

	void Blittable::preRender(const WindowPtr& wm)
	{
		if(changed_) {
			changed_ = false;

			if(draw_rect_.w() == 0 || draw_rect_.h() == 0) {
				draw_rect_ = rectf(0.0f, 0.0f, static_cast<float>(getTexture()->surfaceWidth()), static_cast<float>(getTexture()->surfaceHeight()));
			}

			float offs_x = 0.0f;
			float offs_y = 0.0f;
			switch(centre_) {
				case Centre::MIDDLE:		
					offs_x = -draw_rect_.w()/2.0f;
					offs_y = -draw_rect_.h()/2.0f;
					break;
				case Centre::TOP_LEFT: break;
				case Centre::TOP_RIGHT:
					offs_x = -draw_rect_.w();
					offs_y = 0;
					break;
				case Centre::BOTTOM_LEFT:
					offs_x = 0;
					offs_y = -draw_rect_.h();
					break;
				case Centre::BOTTOM_RIGHT:
					offs_x = -draw_rect_.w();
					offs_y = -draw_rect_.h();
					break;
				case Centre::MANUAL:
					offs_x = centre_offset_.x;
					offs_y = centre_offset_.y;
					break;
			}

			const float vx1 = (vertical_mirrored_ ? draw_rect_.x2() : draw_rect_.x()) + offs_x;
			const float vy1 = (horizontal_mirrored_ ? draw_rect_.y2() : draw_rect_.y()) + offs_y;
			const float vx2 = (vertical_mirrored_ ? draw_rect_.x() : draw_rect_.x2()) + offs_x;
			const float vy2 = (horizontal_mirrored_ ? draw_rect_.y() : draw_rect_.y2()) + offs_y;

			const rectf& r = getTexture()->getSourceRectNormalised();

			std::vector<vertex_texcoord> vertices;
			vertices.emplace_back(glm::vec2(vx1,vy1), glm::vec2(r.x(),r.y()));
			vertices.emplace_back(glm::vec2(vx2,vy1), glm::vec2(r.x2(),r.y()));
			vertices.emplace_back(glm::vec2(vx1,vy2), glm::vec2(r.x(),r.y2()));
			vertices.emplace_back(glm::vec2(vx2,vy2), glm::vec2(r.x2(),r.y2()));
			getAttributeSet().back()->setCount(vertices.size());
			attribs_->update(&vertices);
		}
	}

	void Blittable::setCentre(Centre c)
	{
		centre_  = c;
		centre_offset_ = pointf();
		changed_ = true;
	}

	void Blittable::update(std::vector<vertex_texcoord>* queue)
	{
		attribs_->update(queue);
	}

	void Blittable::setDrawMode(DrawMode mode)
	{
		getAttributeSet().back()->setDrawMode(mode);
	}
}
