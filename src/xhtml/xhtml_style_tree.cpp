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

#include "xhtml_render_ctx.hpp"
#include "xhtml_style_tree.hpp"

namespace xhtml
{
	using namespace css;

	StyleNode::StyleNode(const NodePtr& node)
		: node_(node),
		  children_(),
		  transitions_(),
		  acc_(0.0f),
		  background_attachment_(BackgroundAttachment::SCROLL),
		  background_color_(nullptr),
		  background_image_(nullptr),
		  background_position_{},
		  background_repeat_(BackgroundRepeat::REPEAT),
		  border_color_{},
		  border_style_{},
		  border_width_{},
		  tlbr_{},
		  clear_(Clear::NONE),
		  clip_(nullptr),
		  color_(nullptr),
		  content_(nullptr),
		  counter_increment_(nullptr),
		  counter_reset_(nullptr),
		  cursor_(nullptr),
		  direction_(Direction::LTR),
		  display_(Display::BLOCK),
		  float_(Float::NONE),
		  font_handle_(nullptr),
		  width_height_{},
		  letter_spacing_(nullptr),
		  line_height_(nullptr),
		  list_style_image_(nullptr),
		  list_style_position_(ListStylePosition::OUTSIDE),
		  list_style_type_(ListStyleType::DISC),
		  margin_{},
		  minmax_height_{},
		  minmax_width_{},
		  outline_color_(nullptr),
		  outline_style_(BorderStyle::NONE),
		  outline_width_(nullptr),
		  overflow_(Overflow::AUTO),
		  padding_{},
		  position_(Position::STATIC),
		  quotes_(nullptr),
		  text_align_(TextAlign::NORMAL),
		  text_decoration_(TextDecoration::NONE),
		  text_indent_(nullptr),
		  text_transform_(TextTransform::NONE),
		  unicode_bidi_(UnicodeBidi::NORMAL),
		  vertical_align_(nullptr),
		  visibility_(Visibility::VISIBLE),
		  white_space_(Whitespace::NORMAL),
		  word_spacing_(nullptr),
		  zindex_(nullptr),
		  //BORDER_COLLAPSE
		  //CAPTION_SIDE
		  //EMPTY_CELLS
		  //ORPHANS
		  //TABLE_LAYOUT
		  //WIDOWS
		  //BORDER_SPACING
		  box_shadow_(nullptr),
		  text_shadow_(nullptr),
		  transition_properties_(nullptr),
		  transition_duration_(nullptr),
		  transition_timing_function_(nullptr),
		  transition_delay_(nullptr),
		  border_radius_{},
		  opacity_(1.0f),
		  border_image_(nullptr),
		  border_image_fill_(false),
		  border_image_slice_{},
		  border_image_width_{},
		  border_image_outset_{},
		  border_image_repeat_horiz_(CssBorderImageRepeat::REPEAT),
		  border_image_repeat_vert_(CssBorderImageRepeat::REPEAT),
		  background_clip_(BackgroundClip::BORDER_BOX)
	{
	}

	void StyleNode::parseNode(StyleNodePtr parent, const NodePtr& node)
	{
		std::unique_ptr<RenderContext::Manager> rcm;
		bool is_element = node->id() == NodeId::ELEMENT;
		bool is_text = node->id() == NodeId::TEXT;
		if(is_element || is_text) {
			rcm.reset(new RenderContext::Manager(node->getProperties()));
		}
		StyleNodePtr style_child = std::make_shared<StyleNode>(node);
		if(is_element || is_text) {
			style_child->processStyles(true);
		}

		parent->children_.emplace_back(style_child);

		for(auto& child : node->getChildren()) {
			style_child->parseNode(style_child, child);
		}
	}

	bool StyleNode::preOrderTraversal(std::function<bool(StyleNodePtr)> fn)
	{
		// Visit node, visit children.
		if(!fn(shared_from_this())) {
			return false;
		}
		for(auto& c : children_) {
			if(!c->preOrderTraversal(fn)) {
				return false;
			}
		}
		return true;
	}

	void StyleNode::updateStyles()
	{
		std::unique_ptr<RenderContext::Manager> rcm;
		auto node = node_.lock();
		if(node != nullptr) {
			bool is_element = node->id() == NodeId::ELEMENT;
			bool is_text = node->id() == NodeId::TEXT;
			if(is_element || is_text) {
				rcm.reset(new RenderContext::Manager(node->getProperties()));
				processStyles(false);
			}
		}

		for(auto& child : getChildren()) {
			child->updateStyles();
		}
	}

	void StyleNode::process(float dt)
	{
		acc_ += dt;
		// process any transitions.
		for(auto& tx : transitions_) {
			//LOG_DEBUG("A " << tx->toString() << ", acc: " << acc_ << " " << intptr_t(tx.get()));
			if(!tx->isStarted()) {
				tx->start(acc_);
			}
			if(!tx->isStopped()) {
				tx->process(acc_);
			}
			//LOG_DEBUG("B " << tx->toString() << ", acc: " << acc_ << " " << intptr_t(tx.get()));
		}
		// remove any transitions that have stopped.
		// XXX move to a temporary holding list?
		transitions_.erase(std::remove_if(transitions_.begin(), transitions_.end(), [](TransitionPtr tx){ 
			return tx->isStopped(); 
		}), transitions_.end());

		for(auto& child : children_) {
			child->process(dt);
		}
	}

	void StyleNode::addTransitionEffect(const css::TransitionPtr& tx)
	{
		transitions_.emplace_back(tx);
	}

	void StyleNode::processColor(bool created, Property p, KRE::ColorPtr& color)
	{
		RenderContext& ctx = RenderContext::get();
		std::shared_ptr<CssColor> color_style = ctx.getComputedValue(p)->asType<CssColor>();
		KRE::ColorPtr new_color = color_style->compute();
		if(color_style->hasTransition() && !created) {
			// color_ will be the current computed value.
			for(auto& tx : color_style->getTransitions()) {
				ColorTransitionPtr ct = ColorTransition::create(tx.ttfn, tx.duration, tx.delay);
				ct->setStartColor(color == nullptr ? KRE::Color::colorWhite() : *color);
				ct->setEndColor(*new_color);
				addTransitionEffect(ct);
				color = ct->getColor();
			}
		} else {
			color = new_color;
		}
	}

	void StyleNode::processStyles(bool created)
	{
		RenderContext& ctx = RenderContext::get();
		background_attachment_ = ctx.getComputedValue(Property::BACKGROUND_ATTACHMENT)->getEnum<BackgroundAttachment>();
		
		processColor(created, Property::BACKGROUND_COLOR, background_color_);
		
		auto back_img = ctx.getComputedValue(Property::BACKGROUND_IMAGE);
		background_image_ = back_img != nullptr ? back_img->asType<ImageSource>() : nullptr;
		auto bp = ctx.getComputedValue(Property::BACKGROUND_POSITION)->asType<BackgroundPosition>();
		background_position_[0] = bp->getTop();
		background_position_[1] = bp->getLeft();
		background_repeat_ = ctx.getComputedValue(Property::BACKGROUND_REPEAT)->getEnum<BackgroundRepeat>();
		processColor(created, Property::BORDER_TOP_COLOR, border_color_[0]);
		processColor(created, Property::BORDER_LEFT_COLOR, border_color_[1]);
		processColor(created, Property::BORDER_BOTTOM_COLOR, border_color_[2]);
		processColor(created, Property::BORDER_RIGHT_COLOR, border_color_[3]);
		border_style_[0] = ctx.getComputedValue(Property::BORDER_TOP_STYLE)->getEnum<BorderStyle>();
		border_style_[1] = ctx.getComputedValue(Property::BORDER_LEFT_STYLE)->getEnum<BorderStyle>();
		border_style_[2] = ctx.getComputedValue(Property::BORDER_BOTTOM_STYLE)->getEnum<BorderStyle>();
		border_style_[3] = ctx.getComputedValue(Property::BORDER_RIGHT_STYLE)->getEnum<BorderStyle>();
		border_width_[0] = ctx.getComputedValue(Property::BORDER_TOP_WIDTH)->asType<Length>();
		border_width_[1] = ctx.getComputedValue(Property::BORDER_LEFT_WIDTH)->asType<Length>();
		border_width_[2] = ctx.getComputedValue(Property::BORDER_BOTTOM_WIDTH)->asType<Length>();
		border_width_[3] = ctx.getComputedValue(Property::BORDER_RIGHT_WIDTH)->asType<Length>();
		tlbr_[0] = ctx.getComputedValue(Property::TOP)->asType<Width>();
		tlbr_[1] = ctx.getComputedValue(Property::LEFT)->asType<Width>();
		tlbr_[2] = ctx.getComputedValue(Property::BOTTOM)->asType<Width>();
		tlbr_[3] = ctx.getComputedValue(Property::RIGHT)->asType<Width>();
		clear_ = ctx.getComputedValue(Property::CLEAR)->getEnum<Clear>();
		clip_ = ctx.getComputedValue(Property::CLIP)->asType<Clip>();

		processColor(created, Property::COLOR, color_);

		content_ = ctx.getComputedValue(Property::CONTENT)->asType<Content>();
		counter_increment_ = ctx.getComputedValue(Property::COUNTER_INCREMENT)->asType<Counter>();
		counter_reset_ = ctx.getComputedValue(Property::COUNTER_RESET)->asType<Counter>();
		cursor_ = ctx.getComputedValue(Property::CURSOR)->asType<Cursor>();
		direction_ = ctx.getComputedValue(Property::DIRECTION)->getEnum<Direction>();
		display_ = ctx.getComputedValue(Property::DISPLAY)->getEnum<Display>();
		float_ = ctx.getComputedValue(Property::FLOAT)->getEnum<Float>();
		font_handle_ = RenderContext::get().getFontHandle();
		width_height_[0] = ctx.getComputedValue(Property::WIDTH)->asType<Width>();
		width_height_[1] = ctx.getComputedValue(Property::HEIGHT)->asType<Width>();
		letter_spacing_ = ctx.getComputedValue(Property::LETTER_SPACING)->asType<Length>();
		line_height_ = ctx.getComputedValue(Property::LINE_HEIGHT)->asType<Length>();
		auto list_img = ctx.getComputedValue(Property::LIST_STYLE_IMAGE);
		list_style_image_ = list_img != nullptr ? list_img->asType<ImageSource>() : nullptr;
		list_style_position_ = ctx.getComputedValue(Property::LIST_STYLE_POSITION)->getEnum<ListStylePosition>();
		list_style_type_ = ctx.getComputedValue(Property::LIST_STYLE_TYPE)->getEnum<ListStyleType>();
		margin_[0] = ctx.getComputedValue(Property::MARGIN_TOP)->asType<Width>();
		margin_[1] = ctx.getComputedValue(Property::MARGIN_LEFT)->asType<Width>();
		margin_[2] = ctx.getComputedValue(Property::MARGIN_BOTTOM)->asType<Width>();
		margin_[3] = ctx.getComputedValue(Property::MARGIN_RIGHT)->asType<Width>();
		minmax_height_[0] = ctx.getComputedValue(Property::MIN_HEIGHT)->asType<Width>();
		minmax_height_[1] = ctx.getComputedValue(Property::MAX_HEIGHT)->asType<Width>();
		minmax_width_[0] = ctx.getComputedValue(Property::MIN_WIDTH)->asType<Width>();
		minmax_width_[1] = ctx.getComputedValue(Property::MAX_WIDTH)->asType<Width>();
		
		processColor(created, Property::OUTLINE_COLOR, outline_color_);
		
		outline_style_ = ctx.getComputedValue(Property::OUTLINE_STYLE)->getEnum<BorderStyle>();
		outline_width_ = ctx.getComputedValue(Property::OUTLINE_WIDTH)->asType<Length>();
		overflow_ = ctx.getComputedValue(Property::CSS_OVERFLOW)->getEnum<Overflow>();
		padding_[0] = ctx.getComputedValue(Property::PADDING_TOP)->asType<Length>();
		padding_[1] = ctx.getComputedValue(Property::PADDING_LEFT)->asType<Length>();
		padding_[2] = ctx.getComputedValue(Property::PADDING_BOTTOM)->asType<Length>();
		padding_[3] = ctx.getComputedValue(Property::PADDING_RIGHT)->asType<Length>();
		position_ = ctx.getComputedValue(Property::POSITION)->getEnum<Position>();
		quotes_ = ctx.getComputedValue(Property::QUOTES)->asType<Quotes>();
		text_align_ = ctx.getComputedValue(Property::TEXT_ALIGN)->getEnum<TextAlign>();
		text_decoration_ = ctx.getComputedValue(Property::TEXT_DECORATION)->getEnum<TextDecoration>();
		text_indent_ = ctx.getComputedValue(Property::TEXT_INDENT)->asType<Width>();
		text_transform_ = ctx.getComputedValue(Property::TEXT_TRANSFORM)->getEnum<TextTransform>();
		unicode_bidi_ = ctx.getComputedValue(Property::UNICODE_BIDI)->getEnum<UnicodeBidi>();
		visibility_ = ctx.getComputedValue(Property::VISIBILITY)->getEnum<Visibility>();
		white_space_ = ctx.getComputedValue(Property::WHITE_SPACE)->getEnum<Whitespace>();
		vertical_align_ = ctx.getComputedValue(Property::VERTICAL_ALIGN)->asType<VerticalAlign>();
		word_spacing_ = ctx.getComputedValue(Property::WORD_SPACING)->asType<Length>();
		zindex_ = ctx.getComputedValue(Property::Z_INDEX)->asType<Zindex>();

		box_shadow_ = ctx.getComputedValue(Property::BOX_SHADOW)->asType<BoxShadowStyle>();
		auto ts = ctx.getComputedValue(Property::TEXT_SHADOW);
		if(ts != nullptr) {
			text_shadow_ = ts->asType<TextShadowStyle>();
		}
		// XXX should we turn these styles into the proper list of properties here?
		transition_properties_ = ctx.getComputedValue(Property::TRANSITION_PROPERTY)->asType<TransitionProperties>();
		transition_duration_ = ctx.getComputedValue(Property::TRANSITION_DURATION)->asType<TransitionTiming>();
		transition_timing_function_ = ctx.getComputedValue(Property::TRANSITION_TIMING_FUNCTION)->asType<TransitionTimingFunctions>();
		transition_delay_ = ctx.getComputedValue(Property::TRANSITION_DELAY)->asType<TransitionTiming>();
		border_radius_[0] = ctx.getComputedValue(Property::BORDER_TOP_LEFT_RADIUS)->asType<BorderRadius>();
		border_radius_[1] = ctx.getComputedValue(Property::BORDER_TOP_RIGHT_RADIUS)->asType<BorderRadius>();
		border_radius_[2] = ctx.getComputedValue(Property::BORDER_BOTTOM_RIGHT_RADIUS)->asType<BorderRadius>();
		border_radius_[3] = ctx.getComputedValue(Property::BORDER_BOTTOM_LEFT_RADIUS)->asType<BorderRadius>();
		opacity_ = ctx.getComputedValue(Property::OPACITY)->asType<Length>()->compute() / 65536.0f;
		auto bord_img = ctx.getComputedValue(Property::BORDER_IMAGE_SOURCE);
		border_image_ = bord_img != nullptr ? bord_img->asType<ImageSource>() : nullptr;
		auto bis = ctx.getComputedValue(Property::BORDER_IMAGE_SLICE)->asType<BorderImageSlice>();
		border_image_fill_ = bis->isFilled();
		border_image_slice_ = bis->getWidths();
		border_image_width_ = ctx.getComputedValue(Property::BORDER_IMAGE_WIDTH)->asType<WidthList>()->getWidths();
		border_image_outset_ = ctx.getComputedValue(Property::BORDER_IMAGE_OUTSET)->asType<WidthList>()->getWidths();
		auto bir = ctx.getComputedValue(Property::BORDER_IMAGE_REPEAT)->asType<BorderImageRepeat>();
		border_image_repeat_horiz_ = bir->image_repeat_horiz_;
		border_image_repeat_vert_ = bir->image_repeat_vert_;
		background_clip_ = ctx.getComputedValue(Property::BACKGROUND_CLIP)->getEnum<BackgroundClip>();
	}

	void StyleNode::inheritProperties(const StyleNodePtr& new_styles)
	{
		background_attachment_ = new_styles->background_attachment_; 
		background_color_ = new_styles->background_color_; 
		background_image_ = new_styles->background_image_; 
		background_position_ = new_styles->background_position_; 
		background_repeat_ = new_styles->background_repeat_; 
		border_color_ = new_styles->border_color_; 
		border_style_ = new_styles->border_style_; 
		border_width_ = new_styles->border_width_; 
		tlbr_ = new_styles->tlbr_; 
		clear_ = new_styles->clear_; 
		clip_ = new_styles->clip_; 
		color_ = new_styles->color_; 
		content_ = new_styles->content_; 
		counter_increment_ = new_styles->counter_increment_; 
		counter_reset_ = new_styles->counter_reset_; 
		cursor_ = new_styles->cursor_; 
		direction_ = new_styles->direction_; 
		display_ = new_styles->display_; 
		float_ = new_styles->float_; 
		font_handle_ = new_styles->font_handle_; 
		width_height_ = new_styles->width_height_; 
		letter_spacing_ = new_styles->letter_spacing_; 
		line_height_ = new_styles->line_height_; 
		list_style_image_ = new_styles->list_style_image_; 
		list_style_position_ = new_styles->list_style_position_; 
		list_style_type_ = new_styles->list_style_type_; 
		margin_ = new_styles->margin_; 
		minmax_height_ = new_styles->minmax_height_; 
		minmax_width_ = new_styles->minmax_width_; 
		outline_color_ = new_styles->outline_color_; 
		outline_style_ = new_styles->outline_style_; 
		outline_width_ = new_styles->outline_width_; 
		overflow_ = new_styles->overflow_; 
		padding_ = new_styles->padding_; 
		position_ = new_styles->position_; 
		quotes_ = new_styles->quotes_; 
		text_align_ = new_styles->text_align_; 
		text_decoration_ = new_styles->text_decoration_; 
		text_indent_ = new_styles->text_indent_; 
		text_transform_ = new_styles->text_transform_; 
		unicode_bidi_ = new_styles->unicode_bidi_; 
		vertical_align_ = new_styles->vertical_align_; 
		visibility_ = new_styles->visibility_; 
		white_space_ = new_styles->white_space_; 
		word_spacing_ = new_styles->word_spacing_; 
		zindex_ = new_styles->zindex_; 
		//BORDER_COLLAPSE
		//CAPTION_SIDE
		//EMPTY_CELLS
		//ORPHANS
		//TABLE_LAYOUT
		//WIDOWS
		//BORDER_SPACING
		box_shadow_ = new_styles->box_shadow_; 
		//text_shadow_ = new_styles->text_shadow_; 
		transition_properties_ = new_styles->transition_properties_; 
		transition_duration_ = new_styles->transition_duration_; 
		transition_timing_function_ = new_styles->transition_timing_function_; 
		transition_delay_ = new_styles->transition_delay_; 
		border_radius_ = new_styles->border_radius_; 
		opacity_ = new_styles->opacity_; 
		border_image_ = new_styles->border_image_; 
		border_image_fill_ = new_styles->border_image_fill_; 
		border_image_slice_ = new_styles->border_image_slice_; 
		border_image_width_ = new_styles->border_image_width_; 
		border_image_outset_ = new_styles->border_image_outset_; 
		border_image_repeat_horiz_ = new_styles->border_image_repeat_horiz_; 
		border_image_repeat_vert_ = new_styles->border_image_repeat_vert_; 
		background_clip_ = new_styles->background_clip_; 
	}

	StyleNodePtr StyleNode::createStyleTree(const DocumentPtr& doc)
	{
		StyleNodePtr root = std::make_shared<StyleNode>(doc);
		for(auto& child : doc->getChildren()) {
			root->parseNode(root, child);
		}
		return root;
	}
}
