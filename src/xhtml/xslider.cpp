/*
	Copyright (C) 2015-2016 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "xslider.hpp"
#include "easy_svg.hpp"

#include "DisplayDevice.hpp"

namespace xhtml
{
	using namespace KRE;

	namespace
	{
		static const int INT_SCALE = 1000;
		static const float INT_SCALEF = 1000.0f;
	}

	Slider::Slider(const rect& area, on_change_fn change)
		: SceneObject("Slider"),
		  min_range_(0 * INT_SCALE),
		  max_range_(100 * INT_SCALE),
		  step_(1 * INT_SCALE),
		  position_(0 * INT_SCALE),
		  on_change_(change),
		  loc_(area),
		  pos_changed_(true),
		  attr_(),
		  tex_coords_()
	{
		std::vector<std::string> image_files{ "slider_bar.svg", "slider_handle.svg" };
		std::vector<point> wh{ point(loc_.w(), loc_.h() / 2), point(loc_.h(), loc_.h()) };

		setTexture(svgs_to_single_texture(image_files, wh, &tex_coords_));

		auto ab = DisplayDevice::createAttributeSet(false, false, false);
		attr_ = std::make_shared<Attribute<KRE::vertex_texcoord>>(AccessFreqHint::STATIC, AccessTypeHint::DRAW);
		attr_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, vtx)));
		attr_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, tc)));
		ab->addAttribute(attr_);

		ab->setDrawMode(DrawMode::TRIANGLE_STRIP);
		addAttributeSet(ab);

		init();
	}

	void Slider::init()
	{
		pos_changed_ = true;
	}

	void Slider::setPosition(float value)
	{
		position_ = static_cast<int>(value * INT_SCALE);
		position_ = (position_ / step_) * step_;
		position_ = std::max(min_range_, std::min(position_, max_range_));
		if(on_change_) {
			on_change_(position_ / INT_SCALEF);
		}
		pos_changed_ = true;
	}

	void Slider::preRender(const WindowPtr& wm)
	{
		static std::vector<vertex_texcoord> vt;
		if(pos_changed_) {
			pos_changed_ = false;

			vt.clear();
			const int posx = static_cast<int>((static_cast<float>(position_) / (max_range_ - min_range_)) * loc_.w());
			const auto& t = tex_coords_[1];
			const rect r{posx, 0, loc_.h(), loc_.h()};
			vt.emplace_back(glm::vec2(r.x1(), r.y1()), glm::vec2(t.x1(), t.y1()));
			vt.emplace_back(glm::vec2(r.x2(), r.y1()), glm::vec2(t.x2(), t.y1()));
			vt.emplace_back(glm::vec2(r.x1(), r.y2()), glm::vec2(t.x1(), t.y2()));
			vt.emplace_back(glm::vec2(r.x2(), r.y2()), glm::vec2(t.x2(), t.y2()));
			vt.emplace_back(glm::vec2(r.x2(), r.y2()), glm::vec2(t.x2(), t.y2())); // degenerate
			
			const auto& t1 = tex_coords_[0];
			const rect r1{ 0, loc_.h() / 4, loc_.w(), loc_.h() / 2 };
			vt.emplace_back(glm::vec2(r1.x1(), r1.y1()), glm::vec2(t1.x1(), t1.y1())); // degenerate
			vt.emplace_back(glm::vec2(r1.x1(), r1.y1()), glm::vec2(t1.x1(), t1.y1()));
			vt.emplace_back(glm::vec2(r1.x2(), r1.y1()), glm::vec2(t1.x2(), t1.y1()));
			vt.emplace_back(glm::vec2(r1.x1(), r1.y2()), glm::vec2(t1.x1(), t1.y2()));
			vt.emplace_back(glm::vec2(r1.x2(), r1.y2()), glm::vec2(t1.x2(), t1.y2()));
			attr_->update(&vt);
		}
	}

	void Slider::setDimensions(int w, int h)
	{
		loc_ = rect(loc_.x(), loc_.y(), w, h);
		init();
	}

	bool Slider::handleMouseMotion(bool claimed, const point& p, unsigned keymod, bool in_rect) 
	{
		//LOG_INFO("p:" << p.x << "," << p.y << "; " << (in_rect ? "true" : "false"));
		if(in_rect) {
			// position of thumb
			const int posx = static_cast<int>((static_cast<float>(position_) / (max_range_ - min_range_)) * loc_.w());
			const rect r{posx + loc_.x(), loc_.y(), loc_.h(), loc_.h()};
			//LOG_INFO("p:" << p.x << "," << p.y << "; " << r);
			if(geometry::pointInRect(p, r)) {
				// XXX
			}
		}
		
		return false;
	}

	bool Slider::handleMouseButtonUp(bool claimed, const point& p, unsigned buttons, unsigned keymod, bool in_rect) 
	{

		return false;
	}
	
	bool Slider::handleMouseButtonDown(bool claimed, const point& p, unsigned buttons, unsigned keymod, bool in_rect) 
	{
		LOG_INFO("point: " << p);
		if(in_rect) {
			const int posx = static_cast<int>((static_cast<float>(position_) / (max_range_ - min_range_)) * loc_.w());
			const rect r{posx + loc_.x(), loc_.y(), loc_.h(), loc_.h()};
			if(geometry::pointInRect(p, r)) {
				// XXX handle mouse down on handle
				LOG_INFO("on handle: true");
			} else {
				// handle mouse down somewhere along bar.
				LOG_INFO("p: " << (p.x - loc_.x()) << "; ");
				setPosition(((static_cast<float>(p.x - loc_.x()) / loc_.w()) * (max_range_ - min_range_) + min_range_) / INT_SCALEF);
			}
			return true;
		}
		return false;
	}
	
	bool Slider::handleMouseWheel(bool claimed, const point& p, const point& delta, int direction, bool in_rect)
	{

		return false;
	}
		
	bool Slider::handleKeyDown(bool claimed, const SDL_Keysym& keysym, bool repeat, bool pressed) 
	{

		return false;
	}
	
	bool Slider::handleKeyUp(bool claimed, const SDL_Keysym& keysym, bool repeat, bool pressed) 
	{

		return false;
	}

	void Slider::setRange(float mn, float mx)
	{
		min_range_ = static_cast<int>(mn * INT_SCALE);
		max_range_ = static_cast<int>(mx * INT_SCALE);
		ASSERT_LOG(min_range_ != max_range_, "min and max ranges are equal.");
		if(min_range_ < max_range_) {
			std::swap(min_range_, max_range_);
		}
		position_ = std::max(min_range_, std::min(position_, max_range_));
	}

	float Slider::getMin() const
	{
		return min_range_ / INT_SCALEF;
	}
	
	float Slider::getMax() const
	{
		return max_range_ / INT_SCALEF;
	}
	
	float Slider::getPosition() const
	{
		return position_ / INT_SCALEF;
	}

	void Slider::setStep(float step)
	{
		step_ = static_cast<int>(step * INT_SCALE);
	}
}
