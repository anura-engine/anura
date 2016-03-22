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
	TextBox::TextBox(const BoxPtr& parent, const StyleNodePtr& node, const RootBoxPtr& root)
		: Box(BoxId::TEXT, parent, node, root),
		  txt_(std::dynamic_pointer_cast<Text>(node->getNode())),
		  lines_(),
		  shadows_()
	{
		auto shadows = parent->getStyleNode()->getTextShadow();
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
		txt_->transformText(getStyleNode(), true);
	}

	std::string TextBox::toString() const
	{
		std::ostringstream ss;
		ss << "TextBox: " << getDimensions().content_;
		for(auto& line : lines_) {
			ss  << "\n    "
				<< (line.offset_.x / LayoutEngine::getFixedPointScaleFloat()) 
				<< "," 
				<< (line.offset_.y / LayoutEngine::getFixedPointScaleFloat()) 
				<< ": ";
			for(auto& word : line.line_->line) {
				ss << " " << word.word;
			}
			ss << "\n";
		}
		return ss.str();
	}

	point TextBox::reflowText(LayoutEngine& eng, const Dimensions& containing, const point& current_cursor)
	{
		// clear old data so we can re-calculate it.
		// XXX Future note.
		// unless the text has changed or the width of the containing box changes we shouldn't
		// have to recalculate this.
		lines_.clear();

		point cursor = current_cursor;

		FixedPoint y1 = cursor.y + getOffset().y;
		// XXX if padding left/border left applies should reduce width and move cursor position if isFirstInlineChild() is set.
		// Simlarly the last line width should be reduced by padding right/border right.
		FixedPoint width = eng.getWidthAtPosition(y1, y1 + getLineHeight(), containing.content_.width) - cursor.x + eng.getXAtPosition(y1, y1 + getLineHeight());

		Text::iterator last_it = txt_->begin();
		Text::iterator it = last_it;

		bool done = false;
		while(it != txt_->end()) {
			LinePtr line = txt_->reflowText(it, width, getStyleNode());
			if(line != nullptr && !line->line.empty()) {
				// is the line larger than available space and are there floats present?
				FixedPoint last_x = line->line.back().advance.back().x;
				if(last_x > width && eng.hasFloatsAtPosition(y1, y1 + getLineHeight())) {
					cursor.y += getLineHeight();
					y1 = cursor.y + getOffset().y;
					cursor.x = eng.getXAtPosition(y1, y1 + getLineHeight());
					it = last_it;
					width = eng.getWidthAtPosition(y1, y1 + getLineHeight(), containing.content_.width);
					continue;
				}

				lines_.emplace_back(line, cursor);
				lines_.back().width_ = calculateWidth(lines_.back());
				// XXX This height needs to be modified later if we have inline elements with a different lineheight
				lines_.back().height_ = getLineHeight();
				cursor.x += lines_.back().width_;

				if(line->is_end_line) {
					// update the cursor for the next line
					cursor.y += getLineHeight();
					y1 = cursor.y + getOffset().y;
					cursor.x = eng.getXAtPosition(y1, y1 + getLineHeight());
				}
			}					
		}

		int max_w = 0;
		for(auto& line : lines_) {
			max_w = std::max(max_w, line.width_);
		}
		setContentWidth(max_w);
		
		if(!lines_.empty()) {
			setContentHeight(lines_.back().offset_.y + getLineHeight());
		}

		return cursor;
	}

	FixedPoint TextBox::calculateWidth(const LineInfo& line) const
	{
		ASSERT_LOG(line.line_ != nullptr, "Calculating width of TextBox with no line_ (=nullptr).");
		FixedPoint width = 0;
		for(auto& word : line.line_->line) {
			width += word.advance.back().x;
		}
		width += line.line_->space_advance * line.line_->line.size();
		return width;
	}

	void TextBox::handleLayout(LayoutEngine& eng, const Dimensions& containing)
	{
		// TextBox's have no children to deal with, by definition.	
		// XXX fix the point() to be the actual last point, say from LayoutEngine
		point cursor = reflowText(eng, containing, eng.getCursor());
		eng.setCursor(cursor);

		calculateHorzMPB(containing.content_.width);
		calculateVertMPB(containing.content_.height);
	}

	void TextBox::setRightAlign(FixedPoint containing_width)
	{
		for(auto& line : lines_) {
			// XXX what about case of floats?
			line.offset_.x = containing_width - line.width_;
		}
	}

	void TextBox::setCenterAlign(FixedPoint containing_width)
	{
		for(auto& line : lines_) {
			// XXX what about case of floats?
			line.offset_.x = (containing_width - line.width_ - line.offset_.x) / 2;
		}
	}

	void TextBox::postParentLayout(LayoutEngine& eng, const Dimensions& containing)
	{
		FixedPoint containing_width = containing.content_.width;
		// perform text-align calculation.
		const css::TextAlign ta = getStyleNode()->getTextAlign();
		switch(ta) {
			case css::TextAlign::RIGHT:
				setRightAlign(containing_width);
				break;
			case css::TextAlign::CENTER:	
				setCenterAlign(containing_width);
				break;
			case css::TextAlign::JUSTIFY:	
					setJustify(containing_width);
				break;
			case css::TextAlign::NORMAL:	
				if(getStyleNode()->getDirection() == css::Direction::RTL) {
					setRightAlign(containing_width);
				}
				break;
			case css::TextAlign::LEFT:
			default:
				// use default value.
				break;
		}

		// set vertical alignment
		auto& vertical_align = getStyleNode()->getVerticalAlign();
		css::CssVerticalAlign va = vertical_align->getAlign();

		FixedPoint baseline = getLineHeight();//static_cast<FixedPoint>((getStyleNode()->getFont()->getFontXHeight() + getStyleNode()->getFont()->getLineGap()) * LayoutEngine::getFixedPointScaleFloat()) - getStyleNode()->getFont()->getDescender();

		for(auto& line : lines_) {
			FixedPoint child_y = line.offset_.y;
			// XXX we should implement this fully.
			switch(va) {
				case css::CssVerticalAlign::BASELINE:
					// Align the baseline of the box with the baseline of the parent box. 
					// If the box does not have a baseline, align the bottom margin edge 
					// with the parent's baseline.
					child_y += baseline;
					break;
				case css::CssVerticalAlign::MIDDLE:
					// Align the vertical midpoint of the box with the baseline of the 
					// parent box plus half the x-height of the parent.
					//child_y += height / 2;
					break;
				case css::CssVerticalAlign::BOTTOM:
					// Align the bottom of the aligned subtree with the bottom of the line box.
					//child_y += getBottomOffset();
					break;
				case css::CssVerticalAlign::SUB:
					// Lower the baseline of the box to the proper position for subscripts of the 
					// parent's box. (This value has no effect on the font size of the element's text.)
				case css::CssVerticalAlign::SUPER:
					// Raise the baseline of the box to the proper position for superscripts of the 
					// parent's box. (This value has no effect on the font size of the element's text.)
				case css::CssVerticalAlign::TOP:
					// Align the top of the aligned subtree with the top of the line box.
				case css::CssVerticalAlign::TEXT_TOP:
					// Align the top of the box with the top of the parent's content area
				case css::CssVerticalAlign::TEXT_BOTTOM:
					// Align the bottom of the box with the bottom of the parent's content area
					break;
				case css::CssVerticalAlign::LENGTH: {
					// Offset align by length value. Percentages reference the line-height of the element.
					FixedPoint len = vertical_align->getLength().compute(getLineHeight());
					// 0 for len is the baseline.
					child_y += baseline - len;
				}
				default:  break;
			}

			line.offset_.y = child_y;
		}
	}

	void TextBox::setJustify(FixedPoint containing_width)
	{
		// N.B. we don't justify last line.
		for(auto it = lines_.begin(); it != lines_.end()-1; ++it) {
			auto& line = *it;
			int word_count = line.line_->line.size() - 1;
			if(word_count <= 2) {
				return;
			}
			line.justification_ = (containing_width - line.width_) / word_count;
		}
	}

	void TextBox::handleRenderBackground(const KRE::SceneTreePtr& scene_tree, const point& offset) const
	{
		for(auto it = lines_.begin(); it != lines_.end(); ++it) {
			auto& line = *it;
			Dimensions dims = getDimensions();
			dims.content_.width = line.width_;
			dims.content_.height = line.height_;
			point offs = line.offset_;
			offs.y -= line.height_;
			getBackgroundInfo().render(scene_tree, dims, offs);
		}
	}

	void TextBox::handleRenderBorder(const KRE::SceneTreePtr& scene_tree, const point& offset) const
	{
		for(auto it = lines_.begin(); it != lines_.end(); ++it) {
			auto& line = *it;
			Dimensions dims = getDimensions();
			dims.content_.width = line.width_;
			dims.content_.height = line.height_;
			point offs = line.offset_;
			offs.y -= line.height_;
			if(isFirstInlineChild() && it == lines_.begin()) {
				if(!(isLastInlineChild() && it == lines_.end()-1)) {
					dims.border_.right = 0;
				}
			} else {
				if(isLastInlineChild() && it == lines_.end()-1) {
					dims.border_.left = 0;
				} else {
					dims.border_.left = dims.border_.right = 0;
				}
			}
			getBorderInfo().render(scene_tree, dims, offs);
		}
	}

	void TextBox::handleRenderShadow(const KRE::SceneTreePtr& scene_tree, KRE::FontRenderablePtr fontr, float w, float h) const
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
				shadow_font->setPosition(shadow.x_offset, shadow.y_offset);
				shadow_font->setColor(shadow.color != nullptr ? *shadow.color : *getStyleNode()->getColor());
				scene_tree->addObject(shadow_font);
			} else {
				using namespace KRE;
				// more complex case where we need to blur, so we render the text to a 
				// RenderTarget, then render that to another render target with a blur filter.
				const float extra_border = (kernel_radius) * 2.0f + 20.0f;
				const float width = w + extra_border * 2.0f;
				const float height = h + extra_border * 2.0f;

				const int iwidth = getRootDimensions().content_.width / LayoutEngine::getFixedPointScale();//static_cast<int>(std::round(width));
				const int iheight = getRootDimensions().content_.height / LayoutEngine::getFixedPointScale();//static_cast<int>(std::round(height));

				auto shader_blur = ShaderProgram::createGaussianShader(kernel_radius)->clone();
				const int blur_two = shader_blur->getUniform("texel_width_offset");
				const int blur_tho = shader_blur->getUniform("texel_height_offset");
				const int u_gaussian = shader_blur->getUniform("gaussian");
				std::vector<float> gaussian = generate_gaussian(shadow.blur / 2.0f, kernel_radius);

				//CameraPtr rt_cam = std::make_shared<Camera>("ortho_blur", 0, iwidth, 0, iheight);
				KRE::FontRenderablePtr shadow_font(new KRE::FontRenderable(*fontr));
				const float xheight = getStyleNode()->getFont()->getFontXHeight() / LayoutEngine::getFixedPointScaleFloat();
				shadow_font->setPosition(extra_border, extra_border + xheight);
				//shadow_font->setCamera(rt_cam);
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
				//rt_blur_h->setCamera(rt_cam);
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

				const float offs_x = 0;//getLeft() / LayoutEngine::getFixedPointScaleFloat();
				const float offs_y = 0;//getTop() / LayoutEngine::getFixedPointScaleFloat();
				rt_blur_v->setPosition(offs_x + shadow.x_offset - extra_border, offs_y + shadow.y_offset - xheight - extra_border);
				scene_tree->addObject(rt_blur_v);
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

	void TextBox::handleRender(const KRE::SceneTreePtr& scene_tree, const point& offset) const
	{
		if(lines_.empty()) {
			return;
		}

		//handleRenderTextDecoration -- underlines, then overlines
		KRE::FontRenderablePtr fontr = nullptr;
		for(auto& line : lines_) {
			std::vector<point> path;
			std::string text;
			int dim_x = line.offset_.x;
			int dim_y = getStyleNode()->getFont()->getDescender() + line.offset_.y;
			for(auto& word : line.line_->line) {
				for(auto it = word.advance.begin(); it != word.advance.end()-1; ++it) {
					path.emplace_back(it->x + dim_x, it->y + dim_y);
				}
				dim_x += word.advance.back().x + line.line_->space_advance + line.justification_;
				text += word.word;
			}

			if(!text.empty()) {
				fontr = getStyleNode()->getFont()->createRenderableFromPath(nullptr, text, path);
fontr->setColorPointer(getStyleNode()->getColor());
scene_tree->addObject(fontr);
			}
		}

		if(!shadows_.empty()) {
			ASSERT_LOG(false, "FIXME: handle text shadows");
			//float w = fontr->getWidth() + (line.line_->space_advance + line.justification_) * line.line_->line.size() / LayoutEngine::getFixedPointScaleFloat();
			//float h = static_cast<float>(fontr->getHeight());
			//handleRenderShadow(scene_tree, fontr, w, h);
		}

//		fontr->setColorPointer(getStyleNode()->getColor());
//		scene_tree->addObject(fontr);
		//handleRenderEmphasis -- text-emphasis
		//handleRenderTextDecoration -- line-through
	}
}
