#include "DisplayDevice.hpp"
#include "Shaders.hpp"

#include "rectangle_rotator.hpp"
#include "rect_renderable.hpp"

RectRenderable::RectRenderable(bool strips, bool blend)
	: SceneObject("RectRenderable")
{
	using namespace KRE;

	setShader(ShaderProgram::getProgram("simple"));
	auto ab = DisplayDevice::createAttributeSet(false, false, false);
	r_ = std::make_shared<Attribute<glm::u16vec2>>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW);
	r_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::SHORT, false));
	ab->addAttribute(r_);

	ab->setDrawMode(strips ? DrawMode::TRIANGLE_STRIP : DrawMode::POINTS);
	addAttributeSet(ab);

	ab->setBlendState(blend);
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

void RectRenderable::update(const rect& r, float rotation, const KRE::Color& color)
{
	setColor(color);

	const int center_x = r.x() + r.w()/2;
	const int center_y = r.y() + r.h()/2;

	geometry::Point<int> p;

	std::vector<glm::u16vec2> vc;

	p = rotate_point_around_origin_with_offset(r.x(), r.y(), rotation, center_x, center_y);
	vc.emplace_back(glm::u16vec2(p.x, p.y));

	p = rotate_point_around_origin_with_offset(r.x2(), r.y(), rotation, center_x, center_y);
	vc.emplace_back(glm::u16vec2(p.x, p.y));

	p = rotate_point_around_origin_with_offset(r.x(), r.y2(), rotation, center_x, center_y);
	vc.emplace_back(glm::u16vec2(p.x, p.y));

	p = rotate_point_around_origin_with_offset(r.x2(), r.y2(), rotation, center_x, center_y);
	vc.emplace_back(glm::u16vec2(p.x, p.y));

	r_->update(&vc);
}

void RectRenderable::update(int x, int y, int w, int h, const KRE::Color& color)
{
	setColor(color);

	std::vector<glm::u16vec2> vc;
	vc.emplace_back(glm::u16vec2(x, y));
	vc.emplace_back(glm::u16vec2(x + w, y));
	vc.emplace_back(glm::u16vec2(x, y + h));
	vc.emplace_back(glm::u16vec2(x + w, y + h));

	r_->update(&vc);
}

void RectRenderable::update(const std::vector<glm::u16vec2>& rs, const KRE::Color& color)
{
	setColor(color);
	r_->update(rs);
}

void RectRenderable::update(std::vector<glm::u16vec2>* rs, const KRE::Color& color)
{
	setColor(color);
	r_->update(rs);
}
