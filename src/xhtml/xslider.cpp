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

	Slider::Slider(const rect& area, on_change_fn change)
		: SceneObject("Slider"),
		  min_range_(0.f),
		  max_range_(100.f),
		  step_(1.f),
		  position_(0.f),
		  on_change_(change),
		  loc_(area),
		  pos_changed_(true),
		  attr_(),
		  tex_coords_(),
		  dragging_(false)
	{
		std::vector<std::string> image_files{ "slider_bar.svg", "slider_handle.svg" };
		std::vector<point> wh{ point(loc_.w(), loc_.h() / 2), point((2 * loc_.h()) / 3, loc_.h()) };

		setColor(Color::colorLightgrey());
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

	Slider::~Slider()
	{
		if(dragging_) {
			dragging_ = false;
			SDL_CaptureMouse(SDL_FALSE);
		}
	}

	void Slider::init()
	{
		pos_changed_ = true;
	}

	void Slider::setHandlePosition(float value)
	{
		position_ = value;
		position_ = static_cast<int>(position_ / step_) * step_;
		position_ = std::max(min_range_, std::min(position_, max_range_));
		if(on_change_) {
			on_change_(position_);
		}
		pos_changed_ = true;
	}

	void Slider::preRender(const WindowPtr& wm)
	{
		static std::vector<vertex_texcoord> vt;
		if(pos_changed_) {
			pos_changed_ = false;

			vt.clear();
			const auto& t1 = tex_coords_[0];
			const rect r1{ 0, loc_.h() / 4, loc_.w(), loc_.h() / 2 };
			// bar co-ords
			vt.emplace_back(glm::vec2(r1.x1(), r1.y1()), glm::vec2(t1.x1(), t1.y1()));
			vt.emplace_back(glm::vec2(r1.x2(), r1.y1()), glm::vec2(t1.x2(), t1.y1()));
			vt.emplace_back(glm::vec2(r1.x1(), r1.y2()), glm::vec2(t1.x1(), t1.y2()));
			vt.emplace_back(glm::vec2(r1.x2(), r1.y2()), glm::vec2(t1.x2(), t1.y2()));
			vt.emplace_back(glm::vec2(r1.x2(), r1.y2()), glm::vec2(t1.x2(), t1.y2())); // degenerate

			const int posx = static_cast<int>((position_ - min_range_) / (max_range_ - min_range_) * loc_.w());
			const auto& t = tex_coords_[1];
			const int handle_width = (2 * loc_.h()) / 3;
			const rect r{posx - handle_width / 2, 0, handle_width, loc_.h()};
			// handle co-ords
			vt.emplace_back(glm::vec2(r.x1(), r.y1()), glm::vec2(t.x1(), t.y1()));	// degenerate
			vt.emplace_back(glm::vec2(r.x1(), r.y1()), glm::vec2(t.x1(), t.y1()));
			vt.emplace_back(glm::vec2(r.x2(), r.y1()), glm::vec2(t.x2(), t.y1()));
			vt.emplace_back(glm::vec2(r.x1(), r.y2()), glm::vec2(t.x1(), t.y2()));
			vt.emplace_back(glm::vec2(r.x2(), r.y2()), glm::vec2(t.x2(), t.y2()));
			
			attr_->update(&vt);
		}
	}

	void Slider::setDimensions(int w, int h)
	{
		loc_ = rect(loc_.x(), loc_.y(), w, h);
		init();
	}

	float Slider::positionFromPixelPos(int px)
	{
		float pixel_fraction = static_cast<float>(px - loc_.x()) / loc_.w();
		return pixel_fraction * (max_range_ - min_range_) + min_range_;
	}

	bool Slider::handleMouseMotion(bool claimed, const point& p, unsigned keymod, bool in_rect) 
	{
		if(dragging_) {
			setHandlePosition(positionFromPixelPos(p.x));
			return true;
		}	
		return false;
	}

	bool Slider::handleMouseButtonUp(bool claimed, const point& p, unsigned buttons, unsigned keymod, bool in_rect) 
	{
		if(dragging_) {
			dragging_ = false;
			SDL_CaptureMouse(SDL_FALSE);
			claimed = true;
		}
		return claimed;
	}
	
	bool Slider::handleMouseButtonDown(bool claimed, const point& p, unsigned buttons, unsigned keymod, bool in_rect) 
	{
		if(claimed) {
			return claimed;
		}
		if(in_rect) {
			const int posx = static_cast<int>(((position_ - min_range_) / (max_range_ - min_range_)) * loc_.w());
			const int handle_width = (2 * loc_.h()) / 3;
			const rect r{posx - handle_width / 2 + loc_.x(), loc_.y(), handle_width, loc_.h()};
			if(geometry::pointInRect(p, r)) {
				dragging_ = true;
				SDL_CaptureMouse(SDL_TRUE);
			} else {
				// handle mouse down somewhere along bar.
				setHandlePosition(positionFromPixelPos(p.x));
			}
			return true;
		}
		return false;
	}
	
	bool Slider::handleMouseWheel(bool claimed, const point& p, const point& delta, int direction, bool in_rect)
	{
		// XXX
		return claimed;
	}
		
	bool Slider::handleKeyDown(bool claimed, const SDL_Keysym& keysym, bool repeat, bool pressed) 
	{
		// XXX
		return claimed;
	}
	
	bool Slider::handleKeyUp(bool claimed, const SDL_Keysym& keysym, bool repeat, bool pressed) 
	{
		// XXX
		return claimed;
	}

	void Slider::setRange(float mn, float mx)
	{
		min_range_ = mn;
		max_range_ = mx;
		ASSERT_LOG(min_range_ != max_range_, "min and max ranges are equal.");
		if(min_range_ > max_range_) {
			std::swap(min_range_, max_range_);
		}
		position_ = static_cast<int>(position_ / step_) * step_;
		position_ = std::max(min_range_, std::min(position_, max_range_));
	}
}
