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

#pragma once

#include <array>

#include "SceneTree.hpp"

#include "css_transition.hpp"
#include "xhtml_node.hpp"
#include "xhtml_render_ctx.hpp"

namespace xhtml
{
	class StyleNode;
	typedef std::shared_ptr<StyleNode> StyleNodePtr;
	typedef std::weak_ptr<StyleNode> WeakStyleNodePtr;

	class StyleNode : public std::enable_shared_from_this<StyleNode>
	{
	public:
		StyleNode(const NodePtr& node);
		NodePtr getNode() const { return node_.lock(); }
		void parseNode(StyleNodePtr parent, const NodePtr& node);
		static StyleNodePtr createStyleTree(const DocumentPtr& doc);
		bool preOrderTraversal(std::function<bool(StyleNodePtr)> fn);
		const std::vector<StyleNodePtr>& getChildren() const { return children_; }
		void process(float dt);
		void addTransitionEffect(const css::TransitionPtr& tx);

		KRE::SceneTreePtr getSceneTree() const { return scene_tree_; }
		KRE::SceneTreePtr createSceneTree(KRE::SceneTreePtr scene_parent);

		css::BackgroundAttachment getBackgroundAttachment() const { return background_attachment_; }
		const KRE::ColorPtr& getBackgroundColor() const { return background_color_; }
		 const std::shared_ptr<css::ImageSource> getBackgroundImage() const { return background_image_; }
		// Stored as [0] top, [1] left
		const std::array<css::Length, 2>& getBackgroundPosition() const { return background_position_; }
		css::BackgroundRepeat getBackgroundRepeat() const { return background_repeat_; }
		// Stored as top, left, bottom, right
		const std::array<KRE::ColorPtr, 4>& getBorderColor() const { return border_color_; }
		const std::array<css::BorderStyle, 4>& getBorderStyle() const { return border_style_; }
		const std::array<std::shared_ptr<css::Length>, 4>& getBorderWidths() const { return border_width_; }
		const std::shared_ptr<css::Width>& getTop() const { return tlbr_[0]; }
		const std::shared_ptr<css::Width>& getLeft() const { return tlbr_[1]; }
		const std::shared_ptr<css::Width>& getBottom() const { return tlbr_[2]; }
		const std::shared_ptr<css::Width>& getRight() const { return tlbr_[3]; }
		css::Clear getClear() const { return clear_; }
		const std::shared_ptr<css::Clip>& getClip() const { return clip_; }
		const KRE::ColorPtr& getColor() const { return color_; }
		const std::shared_ptr<css::Content>& getContent() const { return content_; }
		const std::shared_ptr<css::Counter>& getCounterIncr() const { return counter_increment_; }
		const std::shared_ptr<css::Counter>& getCounterReset() const { return counter_reset_; }
		const std::shared_ptr<css::Cursor>& getCursor() const { return cursor_; }
		css::Direction getDirection() const { return direction_; }
		css::Display getDisplay() const { return display_; }
		css::Float getFloat() const { return float_; }
		const KRE::FontHandlePtr& getFont() const { return font_handle_; }
		const std::shared_ptr<css::Width>& getWidth() const { return width_height_[0]; }
		const std::shared_ptr<css::Width>& getHeight() const { return width_height_[1]; }
		const std::shared_ptr<css::Length>& getLetterSpacing() const { return letter_spacing_; }
		const std::shared_ptr<css::Length>& getLineHeight() const { return line_height_; }
		const std::shared_ptr<css::ImageSource>& getListStyleImage() const { return list_style_image_; }
		css::ListStylePosition getListStylePosition() const { return list_style_position_; }
		css::ListStyleType getListStyleType() const { return list_style_type_; }
		const std::array<std::shared_ptr<css::Width>, 4>& getMargin() const { return margin_; }
		const std::shared_ptr<css::Width>& getMinHeight() const { return minmax_height_[0]; }
		const std::shared_ptr<css::Width>& getMaxHeight() const { return minmax_height_[1]; }
		const std::shared_ptr<css::Width>& getMinWidth() const { return minmax_width_[0]; }
		const std::shared_ptr<css::Width>& getMaxWidth() const { return minmax_width_[1]; }
		const KRE::ColorPtr& getOutlineColor() const { return outline_color_; }
		css::BorderStyle getOutlineStyle() const { return outline_style_; }
		const std::shared_ptr<css::Length>& getOutlineWidth() const { return outline_width_; }
		css::Overflow getOverflow() const { return overflow_; }
		const std::array<std::shared_ptr<css::Length>, 4>& getPadding() const { return padding_; }
		css::Position getPosition() const { return position_; }
		const std::shared_ptr<css::Quotes>& getQuotes() const { return quotes_; }
		css::TextAlign getTextAlign() const { return text_align_; }
		css::TextDecoration getTextDecoration() const { return text_decoration_; }
		const std::shared_ptr<css::Width>& getTextIndent() const { return text_indent_; }
		css::TextTransform getTextTransform() { return text_transform_; }
		css::UnicodeBidi getUnicodeBidi() const { return unicode_bidi_; }
		const std::shared_ptr<css::VerticalAlign>& getVerticalAlign() const { return vertical_align_; }
		css::Visibility getVisibility() const { return visibility_; }
		css::Whitespace getWhitespace() const { return white_space_; }
		const std::shared_ptr<css::Length>& getWordSpacing() const { return word_spacing_; }
		const std::shared_ptr<css::Zindex>& getZindex() const { return zindex_; }

		const std::shared_ptr<css::BoxShadowStyle>& getBoxShadow() const { return box_shadow_; }
		const std::shared_ptr<css::TextShadowStyle>& getTextShadow() const { return text_shadow_; }
		const std::shared_ptr<css::TransitionProperties>& getTransitionProperties() const { return transition_properties_; }
		const std::shared_ptr<css::TransitionTiming>& getTransitionDuration() const { return transition_duration_; }
		const std::shared_ptr<css::TransitionTimingFunctions>& getTransitionTimingFunction() const { return transition_timing_function_; }
		const std::shared_ptr<css::TransitionTiming>& getTransitionDelay() const { return transition_delay_; }
		const std::array<std::shared_ptr<css::BorderRadius>, 4>& getBorderRadius() const { return border_radius_; }
		float getOpacity() const { return opacity_; }
		const std::shared_ptr<css::ImageSource>& getBorderImage() const { return border_image_; }
		bool isBorderImageFilled() const { return border_image_fill_; }
		const std::array<css::Width, 4>& getBorderImageSlice() const { return border_image_slice_; }
		const std::array<css::Width, 4>& getBorderImageWidth() const { return border_image_width_; }
		const std::array<css::Width, 4>& getBorderImageOutset() const { return border_image_outset_; }
		css::CssBorderImageRepeat getBorderImageRepeatHoriz() const { return border_image_repeat_horiz_; }
		css::CssBorderImageRepeat getBorderImageRepeatVert() const { return border_image_repeat_vert_; }
		css::BackgroundClip getBackgroundClip() const { return background_clip_; }
		std::shared_ptr<css::FilterStyle> getFilters() const { return filters_; }
		const std::shared_ptr<css::TransformStyle>& getTransform() const { return transform_; }

		const css::StylePtr& getBackgroundAttachmentStyle() const { return background_attachment_style_; }
		const css::StylePtr& getBackgroundRepeatStyle() const { return background_repeat_style_; }
		const css::StylePtr& getBorderTopStyle() const { return border_style_style_[0]; }
		const css::StylePtr& getBorderBottomStyle() const { return border_style_style_[2]; }
		const css::StylePtr& getBorderLeftStyle() const { return border_style_style_[1]; }
		const css::StylePtr& getBorderRightStyle() const { return border_style_style_[3]; }
		const css::StylePtr& getClearStyle() const { return clear_style_; }
		const css::StylePtr& getDirectionStyle() const { return direction_style_; }
		const css::StylePtr& getFloatStyle() const { return float_style_; }
		const css::StylePtr& getListStylePositionStyle() const { return list_style_position_style_; }
		const css::StylePtr& getListStyleTypeStyle() const { return list_style_type_style_; }
		const css::StylePtr& getOutlineStyleStyle() const { return outline_style_style_; }
		const css::StylePtr& getOverflowStyle() const { return overflow_style_; }
		const css::StylePtr& getTextAlignStyle() const { return text_align_style_; }
		const css::StylePtr& getTextDecorationStyle() const { return text_decoration_style_; }
		const css::StylePtr& getTextTransformStyle() const { return text_transform_style_; }
		const css::StylePtr& getUnicodeBidiStyle() const { return unicode_bidi_style_; }
		const css::StylePtr& getVisibilityStyle() const { return visibility_style_; }
		const css::StylePtr& getWhitespaceStyle() const { return white_space_style_; }
		const css::StylePtr& getBorderImageSliceStyle() const { return border_image_slice_style_; }
		const css::StylePtr& getBorderImageRepeatStyle() const { return border_image_repeat_style_; }
		const css::StylePtr& getBackgroundClipStyle() const { return background_clip_style_; }
		const css::StylePtr& getDisplayStyle() const { return display_style_; }
		const css::StylePtr& getPositionStyle() const { return position_style_; }

		// set properties. may trigger re-layout
		void setPropertyFromString(css::Property p, const std::string& value);

		void updateStyles();
		void inheritProperties(const StyleNodePtr& new_styles);
	private:
		void processStyles(bool created);
		void processColor(bool created, css::Property p, KRE::ColorPtr& color);
		void processFilter(bool created);
		void processTransform(bool created);
		WeakNodePtr node_;
		std::vector<StyleNodePtr> children_;
		std::vector<css::TransitionPtr> transitions_;
		float acc_;
		KRE::SceneTreePtr scene_tree_;

		//BACKGROUND_ATTACHMENT
		css::StylePtr background_attachment_style_;
		css::BackgroundAttachment background_attachment_;
		//BACKGROUND_COLOR
		KRE::ColorPtr background_color_;
		//BACKGROUND_IMAGE
		std::shared_ptr<css::ImageSource> background_image_;
		//BACKGROUND_POSITION -- stored as top/left
		css::StylePtr background_position_style_;
		std::array<css::Length, 2> background_position_;
		//BACKGROUND_REPEAT
		css::StylePtr background_repeat_style_;
		css::BackgroundRepeat background_repeat_;
		//BORDER_TOP_COLOR / BORDER_LEFT_COLOR / BORDER_BOTTOM_COLOR / BORDER_RIGHT_COLOR
		std::array<KRE::ColorPtr, 4> border_color_;
		//BORDER_TOP_STYLE / BORDER_LEFT_STYLE / BORDER_BOTTOM_STYLE / BORDER_RIGHT_STYLE
		std::array<css::StylePtr, 4> border_style_style_;
		std::array<css::BorderStyle, 4> border_style_;
		//BORDER_TOP_WIDTH / BORDER_LEFT_WIDTH / BORDER_BOTTOM_WIDTH / BORDER_RIGHT_WIDTH
		std::array<std::shared_ptr<css::Length>, 4> border_width_;
		//TOP / LEFT / BOTTOM / RIGHT
		std::array<std::shared_ptr<css::Width>, 4> tlbr_;
		//CLEAR
		css::StylePtr clear_style_;
		css::Clear clear_;
		//CLIP
		std::shared_ptr<css::Clip> clip_;
		//COLOR
		KRE::ColorPtr color_;
		//CONTENT
		std::shared_ptr<css::Content> content_;
		//COUNTER_INCREMENT
		std::shared_ptr<css::Counter> counter_increment_;
		//COUNTER_RESET
		std::shared_ptr<css::Counter> counter_reset_;
		//CURSOR
		std::shared_ptr<css::Cursor> cursor_;
		//DIRECTION
		css::StylePtr direction_style_;
		css::Direction direction_;
		//DISPLAY
		css::StylePtr display_style_;
		css::Display display_;
		//FLOAT
		css::StylePtr float_style_;
		css::Float float_;
		//FONT_FAMILY / FONT_SIZE / FONT_STYLE / FONT_VARIANT / FONT_WEIGHT
		KRE::FontHandlePtr font_handle_;
		//WIDTH / HEIGHT
		std::array<std::shared_ptr<css::Width>, 2> width_height_;
		//LETTER_SPACING
		std::shared_ptr<css::Length> letter_spacing_;
		//LINE_HEIGHT
		std::shared_ptr<css::Length> line_height_;
		//LIST_STYLE_IMAGE
		std::shared_ptr<css::ImageSource> list_style_image_;
		//LIST_STYLE_POSITION
		css::StylePtr list_style_position_style_;
		css::ListStylePosition list_style_position_;
		//LIST_STYLE_TYPE
		css::StylePtr list_style_type_style_;
		css::ListStyleType list_style_type_;
		//MARGIN_TOP / MARGIN_LEFT / MARGIN_BOTTOM / MARGIN_RIGHT
		std::array<std::shared_ptr<css::Width>, 4> margin_;
		//MIN_HEIGHT / MAX_HEIGHT
		std::array<std::shared_ptr<css::Width>, 2> minmax_height_;
		//MIN_WIDTH / MAX_WIDTH
		std::array<std::shared_ptr<css::Width>, 2> minmax_width_;
		//OUTLINE_COLOR
		KRE::ColorPtr outline_color_;
		//OUTLINE_STYLE
		css::StylePtr outline_style_style_;
		css::BorderStyle outline_style_;
		//OUTLINE_WIDTH
		std::shared_ptr<css::Length> outline_width_;
		//CSS_OVERFLOW
		css::StylePtr overflow_style_;
		css::Overflow overflow_;
		//PADDING_TOP/PADDING_LEFT/PADDING_RIGHT/PADDING_BOTTOM
		std::array<std::shared_ptr<css::Length>, 4> padding_;
		//POSITION
		css::StylePtr position_style_;
		css::Position position_;
		//QUOTES
		std::shared_ptr<css::Quotes> quotes_;
		//TEXT_ALIGN
		css::StylePtr text_align_style_;
		css::TextAlign text_align_;
		//TEXT_DECORATION
		css::StylePtr text_decoration_style_;
		css::TextDecoration text_decoration_;
		//TEXT_INDENT
		std::shared_ptr<css::Width> text_indent_;
		//TEXT_TRANSFORM
		css::StylePtr text_transform_style_;
		css::TextTransform text_transform_;
		//UNICODE_BIDI
		css::StylePtr unicode_bidi_style_;
		css::UnicodeBidi unicode_bidi_;
		//VERTICAL_ALIGN
		std::shared_ptr<css::VerticalAlign> vertical_align_;
		//VISIBILITY
		css::StylePtr visibility_style_;
		css::Visibility visibility_;
		//WHITE_SPACE
		css::StylePtr white_space_style_;
		css::Whitespace white_space_;
		//WORD_SPACING
		std::shared_ptr<css::Length> word_spacing_;
		//Z_INDEX
		std::shared_ptr<css::Zindex> zindex_;

		//BORDER_COLLAPSE
		//CAPTION_SIDE
		//EMPTY_CELLS
		//ORPHANS
		//TABLE_LAYOUT
		//WIDOWS
		//BORDER_SPACING

		//BOX_SHADOW
		std::shared_ptr<css::BoxShadowStyle> box_shadow_;
		//TEXT_SHADOW
		std::shared_ptr<css::TextShadowStyle> text_shadow_;
		//TRANSITION_PROPERTY
		std::shared_ptr<css::TransitionProperties> transition_properties_;
		//TRANSITION_DURATION
		std::shared_ptr<css::TransitionTiming> transition_duration_;
		//TRANSITION_TIMING_FUNCTION
		std::shared_ptr<css::TransitionTimingFunctions> transition_timing_function_;
		//TRANSITION_DELAY
		std::shared_ptr<css::TransitionTiming> transition_delay_;
		//BORDER_TOP_LEFT_RADIUS / BORDER_TOP_RIGHT_RADIUS / BORDER_BOTTOM_RIGHT_RADIUS / BORDER_BOTTOM_LEFT_RADIUS
		std::array<std::shared_ptr<css::BorderRadius>, 4> border_radius_;
		//OPACITY
		css::StylePtr opacity_style_;
		float opacity_;
		//BORDER_IMAGE_SOURCE
		std::shared_ptr<css::ImageSource> border_image_;
		//BORDER_IMAGE_SLICE
		css::StylePtr border_image_slice_style_;
		bool border_image_fill_;
		std::array<css::Width, 4> border_image_slice_;
		//BORDER_IMAGE_WIDTH
		std::array<css::Width, 4> border_image_width_;
		//BORDER_IMAGE_OUTSET
		std::array<css::Width, 4> border_image_outset_;
		//BORDER_IMAGE_REPEAT
		css::StylePtr border_image_repeat_style_;
		css::CssBorderImageRepeat border_image_repeat_horiz_;
		css::CssBorderImageRepeat border_image_repeat_vert_;
		//BACKGROUND_CLIP
		css::StylePtr background_clip_style_;
		css::BackgroundClip background_clip_;
		//FILTER
		std::shared_ptr<css::FilterStyle> filters_;
		//TRANSFORM
		std::shared_ptr<css::TransformStyle> transform_;
	};
}
