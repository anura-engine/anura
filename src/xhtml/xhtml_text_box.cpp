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

#include "BlendModeScope.hpp"
#include "CameraObject.hpp"
#include "RenderTarget.hpp"
#include "Shaders.hpp"
#include "WindowManager.hpp"

#include "xhtml_layout_engine.hpp"
#include "xhtml_text_box.hpp"
#include "xhtml_text_node.hpp"

namespace xhtml
{
	TextBox::TextBox(BoxPtr parent, StyleNodePtr node)
		: Box(BoxId::TEXT, parent, node),
		  line_(),
		  txt_(std::dynamic_pointer_cast<Text>(node->getNode())),
		  it_(),
		  justification_(0),
		  shadows_()
	{
		ASSERT_LOG(parent->getParent() != nullptr, "Something bad happened a text box with no parent->parent valid reference.");
		auto shadows = parent->getParent()->getStyleNode()->getTextShadow();
		if(shadows) {
			// Process shadows in reverse order.
			for(auto it = shadows->getShadows().crbegin(); it != shadows->getShadows().crend(); ++it) {
				const css::TextShadow& shadow = *it;
				float xo = shadow.getOffset()[0].compute() / LayoutEngine::getFixedPointScaleFloat();
				float yo = shadow.getOffset()[1].compute() / LayoutEngine::getFixedPointScaleFloat();
				float br = shadow.getBlur().compute() / LayoutEngine::getFixedPointScaleFloat();
				KRE::ColorPtr color = shadow.getColor().compute();
				shadows_.emplace_back(xo, yo, br, color);
			}
		}
	}

	std::string TextBox::toString() const
	{
		std::ostringstream ss;
		ss << "TextBox: " << getDimensions().content_;
		for(auto& word : line_->line) {
			ss << " " << word.word;
		}
		if(isEOL()) {
			ss << " ; end-of-line";
		}
		return ss.str();
	}

	Text::iterator TextBox::reflow(LayoutEngine& eng, point& cursor, Text::iterator it)
	{
		it_ = it;
		auto parent = getParent();
		FixedPoint y1 = cursor.y + getParent()->getOffset().y;
		FixedPoint width = eng.getWidthAtPosition(y1, y1 + getLineHeight(), parent->getWidth()) - cursor.x + eng.getXAtPosition(y1, y1 + getLineHeight());

		ASSERT_LOG(it != txt_->end(), "Given an iterator at end of text.");

		bool done = false;
		while(!done) {
			LinePtr line = txt_->reflowText(it, width, getStyleNode()->getFont());
			if(line != nullptr && !line->line.empty()) {
				// is the line larger than available space and are there floats present?
				if(line->line.back().advance.back().x > width && eng.hasFloatsAtPosition(y1, y1 + getLineHeight())) {
					cursor.y += getLineHeight();
					y1 = cursor.y + getParent()->getOffset().y;
					cursor.x = eng.getXAtPosition(y1, y1 + getLineHeight());
					it = it_;
					width = eng.getWidthAtPosition(y1, y1 + getLineHeight(), parent->getWidth());
					continue;
				}
				line_ = line;
			}
			
			done = true;
		}

		if(line_ != nullptr) {
			setContentWidth(calculateWidth());
		}

		return it;
	}

	Text::iterator TextBox::end()
	{
		return txt_->end();
	}

	FixedPoint TextBox::calculateWidth() const
	{
		ASSERT_LOG(line_ != nullptr, "Calculating width of TextBox with no line_ (=nullptr).");
		FixedPoint width = 0;
		for(auto& word : line_->line) {
			width += word.advance.back().x;
		}
		width += line_->space_advance * line_->line.size();
		return width;
	}

	void TextBox::handleLayout(LayoutEngine& eng, const Dimensions& containing)
	{
		// TextBox's have no children to deal with, by definition.	
		//setContentWidth(calculateWidth());
		setContentHeight(getLineHeight());

		calculateHorzMPB(containing.content_.width);
		calculateVertMPB(containing.content_.height);
	}

	void TextBox::justify(FixedPoint containing_width)
	{
		int word_count = line_->line.size() - 1;
		if(word_count <= 2) {
			return;
		}
		justification_ = (containing_width - calculateWidth()) / word_count;
	}

	void TextBox::handleRenderBackground(DisplayListPtr display_list, const point& offset) const
	{
		point offs = offset - point(0, getDimensions().content_.height);
		Box::handleRenderBackground(display_list, offs);
	}

	void TextBox::handleRenderBorder(DisplayListPtr display_list, const point& offset) const
	{
		point offs = offset - point(0, getDimensions().content_.height);
		Box::handleRenderBorder(display_list, offs);
	}

	void TextBox::handleRenderShadow(DisplayListPtr display_list, const point& offset, KRE::FontRenderablePtr fontr, float w, float h) const
	{
		// make a copy of the font object.
		//KRE::FontRenderablePtr shadow_font(new KRE::FontRenderable(*fontr));
		std::vector<KRE::RenderablePtr> shadow_list;
		KRE::WindowPtr wnd = KRE::WindowManager::getMainWindow();
		const int kernel_radius = 7;

		for(auto shadow : shadows_) {
			if(std::abs(shadow.blur) < FLT_EPSILON || 
				!KRE::DisplayDevice::checkForFeature(KRE::DisplayDeviceCapabilties::RENDER_TO_TEXTURE)) {
				// no blur
				KRE::FontRenderablePtr shadow_font(new KRE::FontRenderable(*fontr));
				shadow_font->setPosition(shadow.x_offset + offset.x / LayoutEngine::getFixedPointScaleFloat(), 
					shadow.y_offset + offset.y / LayoutEngine::getFixedPointScaleFloat());
				shadow_font->setColor(shadow.color != nullptr ? *shadow.color : *getStyleNode()->getColor());
				display_list->addRenderable(shadow_font);
			} else {
				using namespace KRE;
				// more complex case where we need to blur, so we render the text to a 
				// RenderTarget, then render that to another render target with a blur filter.
				const float extra_border = (kernel_radius) * 2.0f + 20.0f;
				const float width = w + extra_border * 2.0f;
				const float height = h + extra_border * 2.0f;

				const int iwidth = static_cast<int>(std::round(width));
				const int iheight = static_cast<int>(std::round(height));

				auto shader_blur = ShaderProgram::createGaussianShader(kernel_radius)->clone();
				const int blur_two = shader_blur->getUniform("texel_width_offset");
				const int blur_tho = shader_blur->getUniform("texel_height_offset");
				const int u_gaussian = shader_blur->getUniform("gaussian");
				std::vector<float> gaussian = generate_gaussian(shadow.blur / 2.0f, kernel_radius);

				CameraPtr rt_cam = std::make_shared<Camera>("ortho_blur", 0, iwidth, 0, iheight);
				KRE::FontRenderablePtr shadow_font(new KRE::FontRenderable(*fontr));
				const float xheight = getStyleNode()->getFont()->getFontXHeight() / LayoutEngine::getFixedPointScaleFloat();
				shadow_font->setPosition(extra_border, extra_border + xheight);
				shadow_font->setCamera(rt_cam);
				shadow_font->setColor(shadow.color != nullptr ? *shadow.color : *getStyleNode()->getColor());
				int u_ignore_alpha = shadow_font->getShader()->getUniform("ignore_alpha");
				UniformSetFn old_fn = shadow_font->getShader()->getUniformDrawFunction();
				shadow_font->getShader()->setUniformDrawFunction([u_ignore_alpha](ShaderProgramPtr shader) {
					shader->setUniformValue(u_ignore_alpha, 1);
				});				

				RenderTargetPtr rt_blur_h = RenderTarget::create(iwidth, iheight);
				rt_blur_h->getTexture()->setFiltering(-1, Texture::Filtering::LINEAR, Texture::Filtering::LINEAR, Texture::Filtering::POINT);
				rt_blur_h->getTexture()->setAddressModes(-1, Texture::AddressMode::CLAMP, Texture::AddressMode::CLAMP);
				rt_blur_h->setCentre(Blittable::Centre::TOP_LEFT);
				rt_blur_h->setClearColor(Color(0,0,0,0));
				{
					RenderTarget::RenderScope rs(rt_blur_h, rect(0, 0, iwidth, iheight));
					shadow_font->preRender(wnd);
					wnd->render(shadow_font.get());
				}
				shadow_font->getShader()->setUniformDrawFunction(old_fn);
				rt_blur_h->setCamera(rt_cam);
				rt_blur_h->setShader(shader_blur);
				shader_blur->setUniformDrawFunction([blur_two, blur_tho, iwidth, gaussian, u_gaussian](ShaderProgramPtr shader){ 
					shader->setUniformValue(u_gaussian, &gaussian[0]);
					shader->setUniformValue(blur_two, 1.0f / (iwidth - 1.0f));
					shader->setUniformValue(blur_tho, 0.0f);
				});

				RenderTargetPtr rt_blur_v = RenderTarget::create(iwidth, iheight);
				rt_blur_v->getTexture()->setFiltering(-1, Texture::Filtering::LINEAR, Texture::Filtering::LINEAR, Texture::Filtering::POINT);
				rt_blur_v->getTexture()->setAddressModes(-1, Texture::AddressMode::CLAMP, Texture::AddressMode::CLAMP);
				rt_blur_v->setCentre(Blittable::Centre::TOP_LEFT);
				rt_blur_v->setClearColor(Color(0,0,0,0));
				{
					RenderTarget::RenderScope rs(rt_blur_v, rect(0, 0, iwidth, iheight));
					rt_blur_h->preRender(wnd);
					wnd->render(rt_blur_h.get());
				}
				rt_blur_v->setShader(shader_blur);
				shader_blur->setUniformDrawFunction([blur_two, blur_tho, iheight, gaussian, u_gaussian](ShaderProgramPtr shader){ 
					shader->setUniformValue(u_gaussian, &gaussian[0]);
					shader->setUniformValue(blur_two, 0.0f);
					shader->setUniformValue(blur_tho, 1.0f / (iheight - 1.0f));
				});

				rt_blur_v->setPosition(shadow.x_offset + offset.x / LayoutEngine::getFixedPointScaleFloat() - extra_border, 
					shadow.y_offset + (offset.y) / LayoutEngine::getFixedPointScaleFloat() - xheight - extra_border);
				display_list->addRenderable(rt_blur_v);
				// XXX isnstead of adding all the textures here, we should add them to an array, then
				// render them all to an FBO so we only have one final texture.
				/*shadow_list.emplace_back(rt_blur_v);
				rt_blur_v->setPosition(shadow.x_offset, shadow.y_offset);*/
			}
		}

		/*if(!shadow_list.empty()) {
			using namespace KRE;
			int width = shadow_list.front()->getTexture()->width();
			int height = shadow_list.front()->getTexture()->height();
			RenderTargetPtr rt_blurred = RenderTarget::create(width, height);
			rt_blurred->getTexture()->setFiltering(-1, Texture::Filtering::LINEAR, Texture::Filtering::LINEAR, Texture::Filtering::POINT);
			rt_blurred->getTexture()->setAddressModes(-1, Texture::AddressMode::CLAMP, Texture::AddressMode::CLAMP);
			rt_blurred->setCentre(Blittable::Centre::TOP_LEFT);
			rt_blurred->setClearColor(Color(0,0,0,0));
			{
				RenderTarget::RenderScope rs(rt_blurred, rect(0, 0, width, height));
				for(auto& r : shadow_list) {
					r->preRender(wnd);
					wnd->render(r.get());
				}
			}
			rt_blurred->setPosition(offset.x / LayoutEngine::getFixedPointScaleFloat() - kernel_radius,
				(offset.y + getStyleNode()->getFont()->getDescender()) / LayoutEngine::getFixedPointScaleFloat() - h - kernel_radius);
			display_list->addRenderable(rt_blurred);
		}*/
	}

	void TextBox::handleRender(DisplayListPtr display_list, const point& offset) const
	{
		ASSERT_LOG(line_ != nullptr, "TextBox has not had layout done. line_ is nullptr");
		std::vector<point> path;
		std::string text;
		int dim_x = 0;
		int dim_y = getStyleNode()->getFont()->getDescender();
		for(auto& word : line_->line) {
			for(auto it = word.advance.begin(); it != word.advance.end()-1; ++it) {
				path.emplace_back(it->x + dim_x, it->y + dim_y);
			}
			dim_x += word.advance.back().x + line_->space_advance + justification_;
			text += word.word;
		}

		auto& ctx = RenderContext::get();
		auto fontr = getStyleNode()->getFont()->createRenderableFromPath(nullptr, text, path);

		if(!shadows_.empty()) {
			float w = fontr->getWidth() + (line_->space_advance + justification_) * line_->line.size() / LayoutEngine::getFixedPointScaleFloat();
			float h = static_cast<float>(fontr->getHeight());
			handleRenderShadow(display_list, offset, fontr, w, h);
		}
		//handleRenderTextDecoration -- underlines, then overlines

		fontr->setColorPointer(getStyleNode()->getColor());
		fontr->setPosition(offset.x / LayoutEngine::getFixedPointScaleFloat(), offset.y / LayoutEngine::getFixedPointScaleFloat());
		display_list->addRenderable(fontr);

		//handleRenderEmphasis -- text-emphasis
		//handleRenderTextDecoration -- line-through
	}
}
