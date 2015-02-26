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

namespace KRE
{
	Canvas::Canvas()
		: width_(0),
		  height_(0),
		  window_(WindowManager::getMainWindow())
	{
		width_ = getWindow()->logicalWidth();
		height_ = getWindow()->logicalHeight();			
		model_stack_.emplace(glm::mat4(1.0f));
	}

	void Canvas::setDimensions(unsigned w, unsigned h)
	{
		width_ = w;
		height_ = h;
		handleDimensionsChanged();
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

	Canvas::ModelManager::ModelManager()
		: canvas_(KRE::Canvas::getInstance())
	{
		canvas_->model_stack_.emplace(glm::mat4(1.0f));
	}

	Canvas::ModelManager::ModelManager(int tx, int ty, float rotation, float scale)
		: canvas_(KRE::Canvas::getInstance())
	{
		const glm::mat4 m_trans   = glm::translate(glm::mat4(1.0f), glm::vec3(static_cast<float>(tx), static_cast<float>(ty),0.0f));
		const glm::mat4 m_rotate  = glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0.0f,0.0f,1.0f));
		const glm::mat4 m_scale   = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
		glm::mat4 model = m_trans * m_rotate * m_scale;
		canvas_->model_stack_.emplace(model);
	}

	Canvas::ModelManager::~ModelManager() 
	{
		canvas_->model_stack_.pop();
	}

	void Canvas::ModelManager::setIdentity()
	{
		ASSERT_LOG(!canvas_->model_stack_.empty(), "Model stack was empty.");
		canvas_->model_stack_.top() = glm::mat4(1.0f);
	}

	void Canvas::ModelManager::translate(int tx, int ty)
	{
		ASSERT_LOG(!canvas_->model_stack_.empty(), "Model stack was empty.");
		canvas_->model_stack_.top() = glm::translate(canvas_->model_stack_.top(), glm::vec3(static_cast<float>(tx), static_cast<float>(ty),0.0f));
	}

	void Canvas::ModelManager::rotate(float angle)
	{
		ASSERT_LOG(!canvas_->model_stack_.empty(), "Model stack was empty.");
		canvas_->model_stack_.top() = glm::rotate(canvas_->model_stack_.top(), angle, glm::vec3(0.0f, 0.0f, 1.0f));
	}

	void Canvas::ModelManager::scale(float sx, float sy)
	{
		ASSERT_LOG(!canvas_->model_stack_.empty(), "Model stack was empty.");
		canvas_->model_stack_.top() = glm::scale(canvas_->model_stack_.top(), glm::vec3(static_cast<float>(sx), static_cast<float>(sy),1.0f));
	}

	void Canvas::ModelManager::scale(float s)
	{
		ASSERT_LOG(!canvas_->model_stack_.empty(), "Model stack was empty.");
		canvas_->model_stack_.top() = glm::scale(canvas_->model_stack_.top(), glm::vec3(static_cast<float>(s), static_cast<float>(s),1.0f));
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

	WindowManagerPtr Canvas::getWindow() const
	{
		auto wnd = window_.lock();
		ASSERT_LOG(wnd != nullptr, "The window attached to this canvas is no longer valid.");
		return wnd;
	}

	void Canvas::setWindow(WindowManagerPtr wnd)
	{
		window_ = wnd; 
	}
}

