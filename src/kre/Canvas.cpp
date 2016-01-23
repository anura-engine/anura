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

#include "Canvas.hpp"
#include "DisplayDevice.hpp"
#include "ModelMatrixScope.hpp"

namespace KRE
{
	namespace 
	{
		// These are purely for 2D
		std::stack<glm::vec2>& get_translation_stack()
		{
			static std::stack<glm::vec2> res;
			return res;
		}

		std::stack<float>& get_rotation_stack()
		{
			static std::stack<float> res;
			return res;
		}

		std::stack<glm::vec2>& get_scale_stack()
		{
			static std::stack<glm::vec2> res;
			return res;
		}
	}

	Canvas::Canvas()
		: width_(0),
		  height_(0),
		  model_matrix_(1.0f),
		  model_changed_(false),
		  window_(WindowManager::getMainWindow()),
		  size_change_key_(-1),
		  camera_(nullptr),
		  pv_(1.0f)
	{
		width_ = getWindow()->width();
		height_ = getWindow()->height();			
		LOG_DEBUG("canvas dimensions set to: " << width_ << " x " << height_);
		auto wnd = window_.lock();
		camera_ = Camera::createInstance("canvas_camera", 0, width_, 0, height_);
		camera_->setOrthoWindow(0, width_, 0, height_);
		pv_ = camera_->getProjectionMat() * camera_->getViewMat();
		if(wnd) {
			size_change_key_ = wnd->registerSizeChangeObserver([this](int w, int h, int flags) {
				this->setDimensions(w, h);
			});
		}
	}

	void Canvas::setDimensions(unsigned w, unsigned h)
	{
		width_ = w;
		height_ = h;
		camera_->setOrthoWindow(0, width_, 0, height_);
		pv_ = camera_->getProjectionMat() * camera_->getViewMat();
		handleDimensionsChanged();
		LOG_DEBUG("canvas dimensions set to: " << width_ << " x " << height_);
	}

	Canvas::~Canvas()
	{
	}

	CanvasPtr Canvas::getInstance()
	{
		return DisplayDevice::getCurrent()->getCanvas();
	}

	void Canvas::drawVectorContext(const Vector::ContextPtr& context)
	{
		// XXX This almost feels like a little hack.
		// XXX Probably want to call a context->Draw() function or something similar.
		// XXX maybe. It's kind of tricky and I feel the abstraction isn't quite right yet.
		// Since we may have a more efficient path using opengl to draw stuff, rather than blitting a cairo texture.
		// Maybe renderable should have an overridable draw function that can be called?
		// then in DisplayDevice::Render() we check it and run it.
		//DisplayDevice::getCurrent()->render(context);
		ASSERT_LOG(false, "drawVectorContext fixme");
	}

	void Canvas::blitTexture(const TexturePtr& tex, float rotation, const rect& dst, const Color& color) const
	{
		blitTexture(tex, rect(0,0,0,0), rotation, dst, color);
	}

	void Canvas::blitTexture(const TexturePtr& tex, float rotation, int x, int y, const Color& color) const
	{
		blitTexture(tex, rect(0,0,0,0), rotation, rect(x,y), color);
	}

	void generate_color_wheel(int num_points, std::vector<glm::u8vec4>* color_array, const Color& centre, float start_hue, float end_hue)
	{
		ASSERT_LOG(num_points > 0, "Must be more than one point in call to generate_color_wheel()");
		color_array->emplace_back(centre.ri(), centre.gi(), centre.bi(), centre.ai()); // center color.
		float hue = start_hue;
		const float sat = 1.0f;
		const float value = 1.0f;
		for(int n = 0; n != num_points; n++) {
			auto c = Color::from_hsv(hue, sat, value);
			color_array->emplace_back(c.ri(), c.gi(), c.bi(), c.ai());
			hue += (end_hue - start_hue)/static_cast<float>(num_points);
		}
		color_array->emplace_back((*color_array)[1]);
	}

	WindowPtr Canvas::getWindow() const
	{
		auto wnd = window_.lock();
		ASSERT_LOG(wnd != nullptr, "The window attached to this canvas is no longer valid.");
		return wnd;
	}

	void Canvas::setWindow(WindowPtr wnd)
	{
		window_ = wnd;
		if(wnd) {
			if(size_change_key_ >= 0) {
				wnd->registerSizeChangeObserver(size_change_key_, [this](int w, int h, int flags) {
					this->setDimensions(w, h);
				});	
			} else {
				size_change_key_ = wnd->registerSizeChangeObserver([this](int w, int h, int flags) {
					this->setDimensions(w, h);
				});	
			}
		}
	}
}
