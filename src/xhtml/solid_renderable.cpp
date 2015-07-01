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

#include "AttributeSet.hpp"
#include "Blittable.hpp"
#include "DisplayDevice.hpp"
#include "SceneObject.hpp"
#include "Shaders.hpp"

#include "solid_renderable.hpp"
#include "xhtml_layout_engine.hpp"

namespace xhtml
{
	SolidRenderable::SolidRenderable() 
		: KRE::SceneObject("SolidRenderable"),
		  color_(std::make_shared<KRE::Color>())
	{
		init();
	}

	SolidRenderable::SolidRenderable(const rect& r, const KRE::ColorPtr& color)
		: KRE::SceneObject("SolidRenderable"),
		  color_(color == nullptr ? std::make_shared<KRE::Color>() : color)
	{
		init();

		const float vx1 = static_cast<float>(r.x1());
		const float vy1 = static_cast<float>(r.y1());
		const float vx2 = static_cast<float>(r.x2());
		const float vy2 = static_cast<float>(r.y2());

		std::vector<KRE::vertex_color> vc;
		KRE::Color col = KRE::Color::colorWhite();
		vc.emplace_back(glm::vec2(vx1, vy2), col.as_u8vec4());
		vc.emplace_back(glm::vec2(vx1, vy1), col.as_u8vec4());
		vc.emplace_back(glm::vec2(vx2, vy1), col.as_u8vec4());

		vc.emplace_back(glm::vec2(vx2, vy1), col.as_u8vec4());
		vc.emplace_back(glm::vec2(vx2, vy2), col.as_u8vec4());
		vc.emplace_back(glm::vec2(vx1, vy2), col.as_u8vec4());
		attribs_->update(&vc);
	}

	SolidRenderable::SolidRenderable(const rectf& r, const KRE::ColorPtr& color)
		: KRE::SceneObject("SolidRenderable"),
		  color_(color == nullptr ? std::make_shared<KRE::Color>() : color)
	{
		init();

		const float vx1 = r.x1();
		const float vy1 = r.y1();
		const float vx2 = r.x2();
		const float vy2 = r.y2();

		std::vector<KRE::vertex_color> vc;
		KRE::Color col = KRE::Color::colorWhite();
		vc.emplace_back(glm::vec2(vx1, vy2), col.as_u8vec4());
		vc.emplace_back(glm::vec2(vx1, vy1), col.as_u8vec4());
		vc.emplace_back(glm::vec2(vx2, vy1), col.as_u8vec4());

		vc.emplace_back(glm::vec2(vx2, vy1), col.as_u8vec4());
		vc.emplace_back(glm::vec2(vx2, vy2), col.as_u8vec4());
		vc.emplace_back(glm::vec2(vx1, vy2), col.as_u8vec4());
		attribs_->update(&vc);
	}

	void SolidRenderable::setDrawMode(KRE::DrawMode draw_mode)
	{
		getAttributeSet().back()->setDrawMode(draw_mode);
	}

	void SolidRenderable::init()
	{
		using namespace KRE;
		setShader(ShaderProgram::getProgram("attr_color_shader"));

		auto as = DisplayDevice::createAttributeSet();
		attribs_.reset(new KRE::Attribute<KRE::vertex_color>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
		attribs_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_color), offsetof(vertex_color, vertex)));
		attribs_->addAttributeDesc(AttributeDesc(AttrType::COLOR,  4, AttrFormat::UNSIGNED_BYTE, true, sizeof(vertex_color), offsetof(vertex_color, color)));
		as->addAttribute(AttributeBasePtr(attribs_));
		as->setDrawMode(DrawMode::TRIANGLES);
		
		addAttributeSet(as);
	}

	void SolidRenderable::preRender(const KRE::WindowPtr& wnd)
	{
		ASSERT_LOG(color_ != nullptr, "SolidRenderable without valid color pointer.");
		setColor(*color_);
	}

	void SolidRenderable::update(std::vector<KRE::vertex_color>* coords)
	{
		attribs_->update(coords);
	}

	SimpleRenderable::SimpleRenderable()
		: KRE::SceneObject("SimpleRenderable")
	{
		init();
	}

	SimpleRenderable::SimpleRenderable(KRE::DrawMode draw_mode)
		: KRE::SceneObject("SimpleRenderable")
	{
		init(draw_mode);
	}

	void SimpleRenderable::init(KRE::DrawMode draw_mode)
	{
		using namespace KRE;
		setShader(ShaderProgram::getProgram("simple"));

		auto as = DisplayDevice::createAttributeSet();
		attribs_.reset(new KRE::Attribute<glm::vec2>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
		attribs_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false));
		as->addAttribute(AttributeBasePtr(attribs_));
		as->setDrawMode(draw_mode);
		
		addAttributeSet(as);
	}

	void SimpleRenderable::update(std::vector<glm::vec2>* coords)
	{
		attribs_->update(coords);
	}

	void SimpleRenderable::setDrawMode(KRE::DrawMode draw_mode)
	{
		getAttributeSet().back()->setDrawMode(draw_mode);
	}

}
