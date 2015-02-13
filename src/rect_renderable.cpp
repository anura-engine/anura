#include "DisplayDevice.hpp"
#include "rect_renderable.hpp"

RectRenderable::RectRenderable()
	: SceneObject("RectRenderable")
{
	using namespace KRE;

	auto ab = DisplayDevice::createAttributeSet(false, false, false);
	r_ = std::make_shared<Attribute<short_vertex_color>>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW);
	r_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::UNSIGNED_SHORT, false, sizeof(short_vertex_color), offsetof(vertex_color, vertex)));
	ab->addAttribute(r_);

	ab->setDrawMode(DrawMode::TRIANGLE_STRIP);
	addAttributeSet(ab);
}

void RectRenderable::update(const rect& r, const KRE::Color& color)
{
	setColor(color);

	std::vector<glm::u16vec2> vc;
	vc.emplace_back(glm::u16vec2(r.x(), r.y()));
	vc.emplace_back(glm::u16vec2(r.x2(), r.y()));
	vc.emplace_back(glm::u16vec2(r.x(), r.y2()));
	vc.emplace_back(glm::u16vec2(r.x2(), r.y2()));

	r_->update(&vc);
}

