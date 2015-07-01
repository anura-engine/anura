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

#include "asserts.hpp"
#include "Gradients.hpp"

#include "css_styles.hpp"
#include "xhtml_render_ctx.hpp"

namespace css
{
	namespace 
	{
		const int fixed_point_scale = 65536;
		const float fixed_point_scale_float = 65536.0f;

		std::vector<float>& get_font_size_table(float ppi)
		{
			static std::vector<float> res;
			if(res.empty()) {
				// First guess implementation.
				float min_size = 9.0f / 72.0f * ppi;
				res.emplace_back(min_size);
				res.emplace_back(std::ceil(min_size * 1.1f));
				res.emplace_back(std::ceil(min_size * 1.3f));
				res.emplace_back(std::ceil(min_size * 1.45f));
				res.emplace_back(std::ceil(min_size * 1.6f));
				res.emplace_back(std::ceil(min_size * 1.8f));
				res.emplace_back(std::ceil(min_size * 2.0f));
				res.emplace_back(std::ceil(min_size * 2.3f));
			}
			return res;	
		}

		std::string print_border_style(BorderStyle bs)
		{
			switch(bs) {
				case css::BorderStyle::NONE:		return "none";
				case css::BorderStyle::HIDDEN:		return "hidden";
				case css::BorderStyle::DOTTED:		return "dotted";
				case css::BorderStyle::DASHED:		return "dashed";
				case css::BorderStyle::SOLID:		return "solid";
				case css::BorderStyle::DOUBLE:		return "double";
				case css::BorderStyle::GROOVE:		return "groove";
				case css::BorderStyle::RIDGE:		return "ridge";
				case css::BorderStyle::INSET:		return "inset";
				case css::BorderStyle::OUTSET:		return "outset";
				default: break;
			}
			return std::string();
		}

		std::string print_border_image_repeat(CssBorderImageRepeat bir)
		{
			switch(bir) {
				case CssBorderImageRepeat::STRETCH:	return "stretch";
				case CssBorderImageRepeat::REPEAT:	return "repeat";
				case CssBorderImageRepeat::ROUND:	return "round";
				case CssBorderImageRepeat::SPACE:	return "space";
				default: break;
			}
			return std::string();
		}

		std::string print_list_style_type(ListStyleType lst) {
			switch(lst) {
				case ListStyleType::NONE:					return "none";
				case ListStyleType::ARMENIAN:				return "armenian";
				case ListStyleType::CIRCLE:					return "circle";
				case ListStyleType::DECIMAL:				return "decimal";
				case ListStyleType::DECIMAL_LEADING_ZERO:	return "decimal-leading-zero";
				case ListStyleType::DISC:					return "disc";
				case ListStyleType::GEORGIAN:				return "georgian";
				case ListStyleType::LOWER_ALPHA:			return "lower-alpha";
				case ListStyleType::LOWER_GREEK:			return "lower-greek";
				case ListStyleType::LOWER_LATIN:			return "lower-latin";
				case ListStyleType::LOWER_ROMAN:			return "lower-roman";
				case ListStyleType::SQUARE:					return "square";
				case ListStyleType::UPPER_ALPHA:			return "upper-alpha";
				case ListStyleType::UPPER_LATIN:			return "upper-latin";
				case ListStyleType::UPPER_ROMAN:			return "upper-roman";
				default: break;
			}
			return std::string();
		}

		bool point_compare(const glm::vec2& p, const float x1, const float y1) 
		{
			return std::abs(p.x - x1) < FLT_EPSILON && std::abs(p.y - y1) < FLT_EPSILON;
		}
	}

	bool Style::operator==(const StylePtr& style) const
	{
		if(id_ != style->id_) {
			return false;
		}
		return isEqual(style);
	}

	bool Style::isEqual(const StylePtr& style) const
	{
		ASSERT_LOG(stored_enum_ != false, "Called Style::isEqual and stored_enum_==false, this is a bug.");
		return enumeration_ == style->enumeration_;
	}
	
	bool Style::requiresLayout(Property p) const
	{
		if(!stored_enum_) {
			return true;
		}
		switch(p) {
			case Property::BACKGROUND_ATTACHMENT:	return false;
			case Property::BACKGROUND_REPEAT:		return false;
			case Property::OUTLINE_STYLE:			return false;
			case Property::BORDER_TOP_STYLE:		return false;
			case Property::BORDER_LEFT_STYLE:		return false;
			case Property::BORDER_BOTTOM_STYLE:		return false;
			case Property::BORDER_RIGHT_STYLE:		return false;
			case Property::LIST_STYLE_TYPE:			return false;
			case Property::LIST_STYLE_POSITION:		return false;
			case Property::TEXT_ALIGN:				return false;
			case Property::TEXT_DECORATION:			return false;
			case Property::BACKGROUND_CLIP:			return false;
			default:  break;
		}
		return true;
	}

	bool Style::requiresRender(Property p) const
	{
		return true;
	}

	std::string Style::toString(Property p) const
	{
		if(isInherited()) {
			return "inherit";
		}
		if(!stored_enum_) {
			ASSERT_LOG(false, "Base version of Style::toString() called on a non-enum. This needs to be overloaded by derived classes.");
			return std::string();
		}
		switch(p) {
			case Property::BACKGROUND_ATTACHMENT:
				switch(static_cast<BackgroundAttachment>(enumeration_)) {
					case BackgroundAttachment::FIXED:	return "fixed";
					case BackgroundAttachment::SCROLL:	return "scroll";
					default: break;
				}
				break;
			case Property::BACKGROUND_REPEAT:
				switch(static_cast<BackgroundRepeat>(enumeration_)) {
					case BackgroundRepeat::REPEAT:		return "repeat";
					case BackgroundRepeat::NO_REPEAT:	return "no-repeat";
					case BackgroundRepeat::REPEAT_X:	return "repeat-x";
					case BackgroundRepeat::REPEAT_Y:	return "repeat-y";
					default: break;
				}
				break;
			case Property::OUTLINE_STYLE:
			case Property::BORDER_TOP_STYLE:
			case Property::BORDER_LEFT_STYLE:
			case Property::BORDER_BOTTOM_STYLE:
			case Property::BORDER_RIGHT_STYLE:
				return print_border_style(static_cast<BorderStyle>(enumeration_));
				break;
			case Property::CLEAR:
				switch(static_cast<Clear>(enumeration_)) {
					case Clear::NONE:	return "none";
					case Clear::LEFT:	return "left";
					case Clear::RIGHT:	return "right";
					default: break;
				}
				break;
			case Property::DIRECTION:
				switch(static_cast<Direction>(enumeration_)) {
					case Direction::LTR:	return "ltr";
					case Direction::RTL:	return "rtl";
					default: break;
				}
				break;
			case Property::DISPLAY:
				switch(static_cast<Display>(enumeration_)) {
					case Display::BLOCK:				return "block";
					case Display::INLINE:				return "inline";
					case Display::INLINE_BLOCK:			return "inline-block";
					case Display::INLINE_TABLE:			return "inline-table";
					case Display::TABLE:				return "table";
					case Display::TABLE_CAPTION:		return "table-caption";
					case Display::TABLE_CELL:			return "table-cell";
					case Display::TABLE_COLUMN:			return "table-column";
					case Display::TABLE_COLUMN_GROUP:	return "table-column-group";
					case Display::TABLE_FOOTER_GROUP:	return "table-footer-group";
					case Display::TABLE_HEADER_GROUP:	return "table-header-group";
					case Display::TABLE_ROW:			return "table-row";
					case Display::TABLE_ROW_GROUP:		return "table-row-group";
					case Display::LIST_ITEM:			return "list-item";
					case Display::NONE:					return "none";
					default: break;
				}
				break;
			case Property::FLOAT:
				switch(static_cast<Float>(enumeration_)) {
					case Float::NONE:		return "none";
					case Float::LEFT:		return "left";
					case Float::RIGHT:		return "right";
					default: break;
				}
				break;
			case Property::FONT_STYLE:
				switch(static_cast<FontStyle>(enumeration_)) {
					case FontStyle::NORMAL:		return "normal";
					case FontStyle::ITALIC:		return "italic";
					case FontStyle::OBLIQUE:	return "oblique";
					default: break;
				}
				break;
			case Property::FONT_VARIANT:
				switch(static_cast<FontVariant>(enumeration_)) {
					case FontVariant::NORMAL:		return "normal";
					case FontVariant::SMALL_CAPS:	return "small-caps";
					default: break;
				}
				break;
			case Property::LIST_STYLE_TYPE:
				return print_list_style_type(static_cast<ListStyleType>(enumeration_));
			case Property::LIST_STYLE_POSITION:
				switch(static_cast<ListStylePosition>(enumeration_)) {
					case ListStylePosition::INSIDE:		return "inside";
					case ListStylePosition::OUTSIDE:	return "outside";
					default: break;
				}
				break;
			case Property::CSS_OVERFLOW:
				switch(static_cast<Overflow>(enumeration_)) {
					case Overflow::AUTO:		return "auto";
					case Overflow::CLIP:		return "clip";
					case Overflow::HIDDEN:		return "hidden";
					case Overflow::SCROLL:		return "scroll";
					case Overflow::VISIBLE:		return "visible";
					default: break;
				}
				break;
			case Property::POSITION:
				switch(static_cast<Position>(enumeration_)) {
					case Position::STATIC:			return "static";
					case Position::ABSOLUTE_POS:	return "absolute";
					case Position::RELATIVE_POS:	return "relative";
					case Position::FIXED:			return "fixed";
					default: break;
				}
				break;
			case Property::TEXT_ALIGN:
				switch(static_cast<TextAlign>(enumeration_)) {
					case TextAlign::NORMAL:		return "normal";
					case TextAlign::CENTER:		return "center";
					case TextAlign::JUSTIFY:	return "justify";
					case TextAlign::LEFT:		return "left";
					case TextAlign::RIGHT:		return "right";
					default: break;
				}
				break;
			case Property::TEXT_DECORATION:
				switch(static_cast<TextDecoration>(enumeration_)) {
					case TextDecoration::NONE:			return "none";
					case TextDecoration::OVERLINE:		return "overline";
					case TextDecoration::UNDERLINE:		return "underline";
					case TextDecoration::LINE_THROUGH:	return "line-through";
					case TextDecoration::BLINK:			return "blink";
					default: break;
				}
				break;
			case Property::TEXT_TRANSFORM:
				switch(static_cast<TextTransform>(enumeration_)) {
					case TextTransform::NONE:		return "none";
					case TextTransform::LOWERCASE:	return "lowercase";
					case TextTransform::UPPERCASE:	return "uppercase";
					case TextTransform::CAPITALIZE:	return "capitalize";
					default: break;
				}
				break;
			case Property::UNICODE_BIDI:
				switch(static_cast<UnicodeBidi>(enumeration_)) {
					case UnicodeBidi::NORMAL:			return "normal";
					case UnicodeBidi::EMBED:			return "embed";
					case UnicodeBidi::BIDI_OVERRIDE:	return "bidi-override";
					default: break;
				}
				break;
			case Property::VISIBILITY:
				switch(static_cast<Visibility>(enumeration_)) {
					case Visibility::COLLAPSE:		return "collapse";
					case Visibility::HIDDEN:		return "hidden";
					case Visibility::VISIBLE:		return "visible";
					default: break;
				}
				break;
			case Property::WHITE_SPACE:
				switch(static_cast<Whitespace>(enumeration_)) {
					case Whitespace::NORMAL:		return "normal";
					case Whitespace::NOWRAP:		return "nowrap";
					case Whitespace::PRE_LINE:		return "pre-line";
					case Whitespace::PRE:			return "pre";
					case Whitespace::PRE_WRAP:		return "pre-wrap";
					default: break;
				}
				break;
			case Property::BACKGROUND_CLIP:
				switch(static_cast<BackgroundClip>(enumeration_)) {
					case BackgroundClip::BORDER_BOX:	return "border-box";
					case BackgroundClip::CONTENT_BOX:	return "content-box";
					case BackgroundClip::PADDING_BOX:	return "padding-box";
					default: break;
				}
				break;
			default: 
				LOG_ERROR("Style::toString() called on property: '" << get_property_name(p) << "'");
				break;
		}
		return std::string();
	}

	CssColor::CssColor()
		: Style(StyleId::COLOR),
		  param_(CssColorParam::VALUE),
		  color_(std::make_shared<KRE::Color>(KRE::Color::colorWhite()))
	{
	}

	CssColor::CssColor(CssColorParam param, const KRE::Color& color)
		: Style(StyleId::COLOR),
		  param_(param),
		  color_(std::make_shared<KRE::Color>(color))
	{
	}

	void CssColor::setParam(CssColorParam param)
	{
		param_ = param;
		if(param_ != CssColorParam::VALUE) {
			*color_ = KRE::Color(0, 0, 0, 0);
		}
	}

	void CssColor::setColor(const KRE::Color& color)
	{
		*color_ = color;
		setParam(CssColorParam::VALUE);
	}

	KRE::ColorPtr CssColor::compute() const
	{
		if(param_ == CssColorParam::VALUE) {
			return color_;
		} else if(param_ == CssColorParam::CURRENT) {
			// XXX this is broken.
			auto& ctx = xhtml::RenderContext::get();
			auto current_color = ctx.getComputedValue(Property::COLOR)->asType<CssColor>();
			ASSERT_LOG(current_color->getParam() != CssColorParam::CURRENT, "Computing color of current color would cause infinite loop.");
			return current_color->compute();
		}
		*color_ = KRE::Color(0, 0, 0, 0);
		return color_;
	}

	bool CssColor::isEqual(const StylePtr& a) const
	{		
		auto p = std::dynamic_pointer_cast<CssColor>(a);
		return *color_ == *p->color_;
	}

	Length::Length(xhtml::FixedPoint value, const std::string& units) 
		: Style(StyleId::LENGTH),
		  value_(value), 
		  units_(LengthUnits::NUMBER) 
	{
		if(units == "em") {
			units_ = LengthUnits::EM;
		} else if(units == "ex") {
			units_ = LengthUnits::EX;
		} else if(units == "in") {
			units_ = LengthUnits::INCHES;
		} else if(units == "cm") {
			units_ = LengthUnits::CM;
		} else if(units == "mm") {
			units_ = LengthUnits::MM;
		} else if(units == "pt") {
			units_ = LengthUnits::PT;
		} else if(units == "pc") {
			units_ = LengthUnits::PC;
		} else if(units == "px") {
			units_ = LengthUnits::PX;
		} else if(units == "%") {
			units_ = LengthUnits::PERCENT;
			// normalize to range 0.0 -> 1.0
			value_ = fixed_point_scale;
		} else {
			LOG_ERROR("unrecognised units value: '" << units << "'");
		}
	}

	xhtml::FixedPoint Length::compute(xhtml::FixedPoint scale) const
	{
		auto& ctx = xhtml::RenderContext::get();
		const int dpi = ctx.getDPI();
		xhtml::FixedPoint res = 0;
		switch(units_) {
			case LengthUnits::NUMBER:
				res = value_;
				break;
			case LengthUnits::PX:
				res = static_cast<int>((static_cast<float>(value_)/fixed_point_scale_float) * dpi * 3.0f / (72.0f * 4.0f) * fixed_point_scale_float);
				break;
			case LengthUnits::EM: {				
				float fs = ctx.getFontHandle()->getFontSize() / 72.0f;
				res = static_cast<xhtml::FixedPoint>(fs * static_cast<float>(value_ * dpi));
				break;
			}
			case LengthUnits::EX:
				res = static_cast<xhtml::FixedPoint>(ctx.getFontHandle()->getFontXHeight() / 72.0f * (value_ * dpi));
				break;
			case LengthUnits::INCHES:
				res = value_ * dpi;
				break;
			case LengthUnits::CM:
				res = (value_ * dpi * 254) / 100;
				break;
			case LengthUnits::MM:
				res = (value_ * dpi * 254) / 10;
				break;
			case LengthUnits::PT:
				res = (value_ * dpi) / 72;
				break;
			case LengthUnits::PC:
				res = (12 * value_ * dpi) / 72;
				break;
			case LengthUnits::PERCENT:
				res = (value_ / fixed_point_scale) * (scale / 100);
				break; 
			default: 
				ASSERT_LOG(false, "Unrecognised units value: " << static_cast<int>(units_));
				break;
		}
		return res;
	}

	bool Length::operator==(const Length& a) const
	{
		return units_ == a.units_ && value_ == a.value_;
	}

	bool Length::isEqual(const StylePtr& a) const
	{		
		auto p = std::dynamic_pointer_cast<Length>(a);
		return units_ == p->units_ && value_ == p->value_;
	}

	xhtml::FixedPoint FontSize::compute(xhtml::FixedPoint parent_fs, int dpi) const
	{
		float res = 0;
		if(is_absolute_) {
			res = get_font_size_table(static_cast<float>(dpi))[static_cast<int>(absolute_)];
		} else if(is_relative_) {
			// XXX hack
			if(relative_ == FontSizeRelative::LARGER) {
				res = parent_fs * 1.15f;
			} else {
				res = parent_fs / 1.15f;
			}
		} else if(is_length_) {
			return length_.compute(parent_fs);
		} else {
			ASSERT_LOG(false, "FontSize has no definite size defined!");
		}
		return static_cast<xhtml::FixedPoint>(res * fixed_point_scale);
	}

	FontFamily::FontFamily() 
		: Style(StyleId::FONT_FAMILY), 
		  fonts_() 
	{ 
		fonts_.emplace_back("sans-serif");
	}

	bool FontFamily::isEqual(const StylePtr& a) const
	{
		auto p = std::dynamic_pointer_cast<FontFamily>(a);
		return fonts_ == p->fonts_;
	}

	int FontWeight::compute(int fw) const
	{
		if(is_relative_) {
			if(relative_ == FontWeightRelative::BOLDER) {
				// bolder
				fw += 100;
			} else {
				// lighter
				fw -= 100;
			}
			if(fw > 900) {
				fw = 900;
			} else if(fw < 100) {
				fw = 100;
			}
			fw = (fw / 100) * 100;
			return fw;
		}		
		return weight_;
	}

	bool FontWeight::isEqual(const StylePtr& a) const
	{
		auto p = std::dynamic_pointer_cast<FontWeight>(a);
		return is_relative_ == p->is_relative_ ? is_relative_ ? relative_ == p->relative_ : weight_ == p->weight_ : false;
	}

	BackgroundPosition::BackgroundPosition() 
		: Style(StyleId::BACKGROUND_POSITION),
		  left_(0, true),
		  top_(0, true)
	{
	}

	void BackgroundPosition::setLeft(const Length& left) 
	{
		left_ = left;
	}

	void BackgroundPosition::setTop(const Length& top) 
	{
		top_ = top;
	}

	bool BackgroundPosition::isEqual(const StylePtr& a) const
	{
		auto p = std::dynamic_pointer_cast<BackgroundPosition>(a);
		return left_ == p->left_ && top_ == p->top_;
	}

	ContentType::ContentType(CssContentType type)
		: type_(type),
		  str_(),
		  uri_(),
		  counter_name_(),
		  counter_seperator_(),
		  counter_style_(ListStyleType::DISC),
		  attr_()
	{
	}

	ContentType::ContentType(CssContentType type, const std::string& name)
		: type_(type),
		  str_(),
		  uri_(),
		  counter_name_(),
		  counter_seperator_(),
		  counter_style_(ListStyleType::DISC),
		  attr_()
	{
		switch(type)
		{
			case css::CssContentType::STRING:		str_ = name; break;
			case css::CssContentType::URI:			uri_ = name; break;
			case css::CssContentType::ATTRIBUTE:	attr_ = name; break;
			default: break;
		}
	}

	ContentType::ContentType(ListStyleType lst, const std::string& name)
		: type_(CssContentType::COUNTER),
		  str_(),
		  uri_(),
		  counter_name_(name),
		  counter_seperator_(),
		  counter_style_(lst),
		  attr_()
	{
	}

	ContentType::ContentType(ListStyleType lst, const std::string& name, const std::string& sep)
		: type_(CssContentType::COUNTERS),
		  str_(),
		  uri_(),
		  counter_name_(name),
		  counter_seperator_(sep),
		  counter_style_(lst),
		  attr_()
	{
	}

	BoxShadow::BoxShadow()
		: inset_(false),
		  x_offset_(),
		  y_offset_(),
		  blur_radius_(),
		  spread_radius_(),
		  color_()
	{
	}

	BoxShadow::BoxShadow(bool inset, const Length& x, const Length& y, const Length& blur, const Length& spread, const CssColor& color)
		: inset_(inset),
		  x_offset_(x),
		  y_offset_(y),
		  blur_radius_(blur),
		  spread_radius_(spread),
		  color_(color)
	{
	}

	WidthList::WidthList(const std::vector<Width>& widths)
		: Style(StyleId::WIDTH_LIST),
		  widths_{}
	{
		setWidths(widths);
	}

	WidthList::WidthList(float value)
		: Style(StyleId::WIDTH_LIST),
		  widths_{}
	{
		for(int side = 0; side != 4; ++side) {
			widths_[side] = Width(Length(static_cast<int>(value * fixed_point_scale_float), false));
		}
	}

	void WidthList::setWidths(const std::vector<Width>& widths)
	{
		switch(widths.size()) {
			case 0:
				widths_[0] = widths_[1] = widths_[2] = widths_[3] = Width(Length(fixed_point_scale, false));
				break;
			case 1:
				for(int n = 0; n != 4; ++n) {
					widths_[n] = widths[0];
				}
				break;
			case 2:
				widths_[0] = widths[0];		// top    -- top
				widths_[1] = widths[1];		// left   -- right
				widths_[2] = widths[2];		// bottom -- bottom
				widths_[3] = widths[1];		// right  -- right
			case 3:
				widths_[0] = widths[0];		// top    -- top
				widths_[1] = widths[1];		// left   -- right
				widths_[2] = widths[0];		// bottom -- top
				widths_[3] = widths[1];		// right  -- right
				break;
			default:
				for(int n = 0; n != 4; ++n) {
					widths_[n] = widths[n];
				}
				break;
		}
	}

	bool WidthList::isEqual(const StylePtr& a) const
	{
		auto p = std::dynamic_pointer_cast<WidthList>(a);
		return widths_ == p->widths_;
	}

	void BorderImageSlice::setWidths(const std::vector<Width>& widths)
	{
		switch(widths.size()) {
			case 0:
				slices_[0] = slices_[1] = slices_[2] = slices_[3] = Width(Length(100, true));
				break;
			case 1:
				for(int n = 0; n != 4; ++n) {
					slices_[n] = widths[0];
				}
				break;
			case 2:
				slices_[0] = widths[0];		// top    -- top
				slices_[1] = widths[1];		// left   -- right
				slices_[2] = widths[2];		// bottom -- bottom
				slices_[3] = widths[1];		// right  -- right
			case 3:
				slices_[0] = widths[0];		// top    -- top
				slices_[1] = widths[1];		// left   -- right
				slices_[2] = widths[0];		// bottom -- top
				slices_[3] = widths[1];		// right  -- right
				break;
			default:
				for(int n = 0; n != 4; ++n) {
					slices_[n] = widths[n];
				}
				break;
		}
	}

	BorderImageSlice::BorderImageSlice(const std::vector<Width>& widths, bool fill)
		: Style(StyleId::BORDER_IMAGE_SLICE),
		  slices_(),
		  fill_(fill)
	{
		setWidths(widths);
	}

	bool BorderImageSlice::isEqual(const StylePtr& a) const
	{
		auto p = std::dynamic_pointer_cast<BorderImageSlice>(a);
		return slices_ == p->slices_ && fill_ == p->fill_;
	}

	Angle::Angle(float angle, const std::string& units)
		: value_(angle),
		  units_(AngleUnits::DEGREES)
	{
		if(units == "deg") {
			units_ = AngleUnits::DEGREES;
		} else if(units == "rad") {
			units_ = AngleUnits::RADIANS;
		} else if(units == "grad") {
			units_ = AngleUnits::GRADIANS;
		} else if(units == "turn") {
			units_ = AngleUnits::TURNS;		
		} else {
			ASSERT_LOG(false, "Unrecognised angle units value: " << units);
		}
	}

	float Angle::getAngle(AngleUnits units)
	{
		// early return if units are the same.
		if(units == units_) {
			return value_;
		}

		// convert to degrees. Probably not the most elegant way of doing it.
		float angle = value_;
		switch(units_) {
			case AngleUnits::RADIANS:	angle = 180.0f / static_cast<float>(M_PI) * value_; break;
			case AngleUnits::GRADIANS:	angle = 0.9f * value_; break;
			case AngleUnits::TURNS:		angle = 360.0f * value_; break;
			case AngleUnits::DEGREES:	
			default:
				// no conversion required.
				break;
		}

		// convert to requested format.
		switch(units) {
			case AngleUnits::RADIANS:	angle = static_cast<float>(M_PI) / 180.0f * angle; break;
			case AngleUnits::GRADIANS:	angle = angle / 0.9f; break;
			case AngleUnits::TURNS:		angle = angle / 360.0f; break;
			case AngleUnits::DEGREES:
			default:
				// no conversion required.
				break;
		}
		return angle;
	}

	Time::Time(float t, const std::string& units)
		: value_(t),
		  units_(TimeUnits::SECONDS)
	{
		if(units == "s") {
			units_ = TimeUnits::SECONDS;
		} else if(units == "ms") {
			units_ = TimeUnits::MILLISECONDS;
		} else {
			ASSERT_LOG(false, "Unrecognised angle units value: " << units);
		}
	}

	float Time::getTime(TimeUnits units)
	{
		// early return if units are the same.
		if(units == units_) {
			return value_;
		}

		// convert to degrees. Probably not the most elegant way of doing it.
		float time_value = value_;
		switch(units_) {
			case TimeUnits::MILLISECONDS:	time_value /= 1000.0f; break;
			case TimeUnits::SECONDS:	
			default:
				// no conversion required.
				break;
		}

		// convert to requested format.
		switch(units) {
			case TimeUnits::MILLISECONDS:	time_value *= 1000.0f; break;
			case TimeUnits::SECONDS:
			default:
				// no conversion required.
				break;
		}
		return time_value;
	}

	KRE::TexturePtr UriStyle::getTexture(xhtml::FixedPoint width, xhtml::FixedPoint height)
	{
		// width/height are only suggestions, since we should have intrinsic width
		if(is_none_ || uri_.empty()) {
			return nullptr;
		}
		return KRE::Texture::createTexture(uri_);
	}

	// Convert a length value, either dimension or percentage into a value, 
	// 0.0 -> 1.0 on a line.
	float calculate_color_stop_length(const Length& len, const float len_gradient_line)
	{
		if(len.isPercent()) {
			return len.compute() / 65536.0f;
		} else if(len.isLength()) {
			return (len.compute() / 65536.0f) / len_gradient_line;
		} else {
			ASSERT_LOG(false, "Something went wrong with color stop length value, must be percentage or dimension value.");
		}
		return 0.0f;
	}

	KRE::TexturePtr LinearGradient::getTexture(xhtml::FixedPoint w, xhtml::FixedPoint h)
	{
		KRE::LinearGradient lg;
		lg.setAngle(angle_);

		const float width = static_cast<float>(w) / 65536.0f;
		const float height = static_cast<float>(h) / 65536.0f;

		// calculate length of gradient line from one side of box to the other.
		// Is the actual value in pixels.
		const float s_theta = std::abs(sin(angle_ / 180.0f * static_cast<float>(M_PI)));
		const float c_theta = std::abs(cos(angle_ / 180.0f * static_cast<float>(M_PI)));
		const float len_gradient_line = std::min(c_theta < FLT_EPSILON ? FLT_MAX : width / c_theta,  
			s_theta < FLT_EPSILON ? FLT_MAX : height / s_theta);

		if(color_stops_.empty()) {
			LOG_ERROR("No linear-gradient color stops defined.");
			return nullptr;
		}
		float previous_len = 0.0f;
		if(color_stops_.front().length.isNumber()) {
			// numbers are treated as a no-value-given. i.e. 0%
			lg.addColorStop(*color_stops_.front().color->compute(), 0.0f);
		} else {
			previous_len = calculate_color_stop_length(color_stops_.front().length, len_gradient_line);
			lg.addColorStop(*color_stops_.front().color->compute(), previous_len);
		}

		float last_len = 1.0f;
		KRE::Color last_color;
		if(color_stops_.size() != 1) {
			last_color =*color_stops_.back().color->compute();
			if(!color_stops_.back().length.isNumber()) {
				last_len = calculate_color_stop_length(color_stops_.back().length, len_gradient_line);
			}
		}

		std::vector<KRE::ColorStop> unresolved_list;
		for(auto it = color_stops_.begin() + 1; it != color_stops_.end() - 1; ++it) {
			auto& cs = *it;
			if(cs.length.isNumber()) {
				unresolved_list.emplace_back(*cs.color->compute(), 0.0f);
			} else {
				// XXX scan unresolved list and resolve.
				float len = calculate_color_stop_length(cs.length, len_gradient_line);
				if(len < previous_len) {
					len = previous_len;
				}
				int index = 1;
				for(auto& ur : unresolved_list) {
					ur.length = (len - previous_len) * static_cast<float>(index) / static_cast<float>(unresolved_list.size() + 1);
					lg.addColorStop(ur.color, ur.length);
					++index;
				}
				lg.addColorStop(*cs.color->compute(), len);
				unresolved_list.clear();
				previous_len = len;
			}
		}
		int index = 1;
		for(auto& ur : unresolved_list) {
			ur.length = (last_len - previous_len) * static_cast<float>(index) / static_cast<float>(unresolved_list.size() + 1);
			lg.addColorStop(ur.color, ur.length);
			++index;
		}

		lg.addColorStop(last_color, last_len);

		// XXX we should cache the texture, in-case
		// to do.
		return lg.createAsTexture(static_cast<int>(width), static_cast<int>(height));
	}

	bool Width::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<Width>(style);
		return false;
	}

	bool UriStyle::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<UriStyle>(style);
		return false;
	}

	bool FontSize::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<FontSize>(style);
		return false;
	}

	bool Clip::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<Clip>(style);
		return false;
	}

	bool Cursor::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<Cursor>(style);
		return false;
	}
	bool Content::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<Content>(style);
		return false;
	}

	bool Counter::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<Counter>(style);
		return false;
	}

	bool Quotes::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<Quotes>(style);
		return false;
	}

	bool VerticalAlign::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<VerticalAlign>(style);
		return false;
	}

	bool Zindex::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<Zindex>(style);
		return false;
	}

	bool BoxShadowStyle::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<BoxShadowStyle>(style);
		return false;
	}

	bool BorderImageRepeat::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<BorderImageRepeat>(style);
		return false;
	}

	bool BorderRadius::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<BorderRadius>(style);
		return false;
	}

	bool LinearGradient::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<LinearGradient>(style);
		return false;
	}

	bool TransitionProperties::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<TransitionProperties>(style);
		return false;
	}

	bool TransitionTiming::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<TransitionTiming>(style);
		return false;
	}

	bool TransitionTimingFunctions::isEqual(const StylePtr& style) const
	{
		auto p = std::dynamic_pointer_cast<TransitionTimingFunctions>(style);
		return false;
	}

	TextShadow::TextShadow(const Length& offset_x, const Length& offset_y) 
		: color_(), 
		  offset_{}, 
		  blur_radius_(0, LengthUnits::PX) 
	{
		offset_[0] = offset_x;
		offset_[1] = offset_y;
	}

	TextShadow::TextShadow(const Length& offset_x, const Length& offset_y, const CssColor& color, const Length& blur) 
		: color_(color), 
		  offset_{}, 
		  blur_radius_(blur) 
	{
		offset_[0] = offset_x;
		offset_[1] = offset_y;
	}

	TextShadow::TextShadow(const std::vector<Length>& len, const CssColor& color)
		: color_(color),
		offset_{},
		  blur_radius_(len.size() > 2 ? len[2] : Length(0, LengthUnits::PX))
	{
		ASSERT_LOG(len.size() != 2 || len.size() != 3, "Wrong number of lengths in TextShadow constructor.");
		offset_[0] = len[0];
		offset_[1] = len[1];
	}


    std::string BackgroundPosition::toString(Property p) const
    {
		return left_.toString(p) + " " + top_.toString(p);
    }

    std::string BorderImageRepeat::toString(Property p) const
    {
		return print_border_image_repeat(image_repeat_horiz_) + " " + print_border_image_repeat(image_repeat_vert_);
    }

    std::string BorderImageSlice::toString(Property p) const
    {
		std::string s;
		for(auto& side : slices_) {
			s += (s.empty() ? "" : " ") + side.toString(p);
		}
		if(isFilled()) {
			s += " fill";
		}
		return s;
    }

    std::string BorderRadius::toString(Property p) const
    {
		return horiz_.toString(p) + " " + vert_.toString(p);
    }

    std::string BoxShadowStyle::toString(Property p) const
    {
		if(shadows_.empty()) {
			return "none";
		}
		std::string s;
		for(auto it = shadows_.begin(); it != shadows_.end(); ++it) {
			auto& shadow = *it;
			s += shadow.getX().toString(p) + " " + shadow.getY().toString(p);
			if(shadow.getBlur().compute() != 0) {
				s += " " + shadow.getBlur().toString(p);
				if(shadow.getSpread().compute() != 0) {
					s += " " + shadow.getSpread().toString(p);
				}
			}
			s += " " + shadow.getColor().toString(p);
			if(shadow.inset()) {
				s += " inset";
			}
			if(it + 1 != shadows_.end()) {
				s += ", ";
			}
		}
		return s;
    }

    std::string Clip::toString(Property p) const
    {
		if(isAuto()) {
			return "auto";
		}
		std::stringstream ss;
		ss << "rect(" << rect_.y/65536 << "," << rect_.x/65536 << "," << rect_.height/65536 << "," << rect_.width/65536 << ")";
		return ss.str();
	}

	std::string ContentType::toString() const
	{
		switch(type_) {
			case CssContentType::STRING:			return "\"" + str_ + "\"";
			case CssContentType::URI:				return "uri(" + uri_ + ")";
			case CssContentType::COUNTER:			
				return "counter(" 
					+ counter_name_ 
					+ (counter_style_ == ListStyleType::DECIMAL ? "" : (", " + print_list_style_type(counter_style_))) 
					+ ")";
			case CssContentType::COUNTERS:
				return "counter(" 
					+ counter_name_ 
					+ ", " + counter_seperator_
					+ (counter_style_ == ListStyleType::DECIMAL ? "" : (", " + print_list_style_type(counter_style_))) 
					+ ")";
			case CssContentType::OPEN_QUOTE:		return "open-quote";
			case CssContentType::CLOSE_QUOTE:		return "close-quote";
			case CssContentType::NO_OPEN_QUOTE:		return "no-open-quote";
			case CssContentType::NO_CLOSE_QUOTE:	return "no-close-quote";
			case CssContentType::ATTRIBUTE:			return "attr(" + attr_ + ")";
			default: break;
		}
		return std::string();
	}

    std::string Content::toString(Property p) const
    {
		if(content_.empty()) {
			return "normal";	// XXX this should be normal if on a pseudo event else "none" on an element.
		}
		std::stringstream ss;
		for(auto it = content_.begin(); it != content_.end(); ++it) {
			auto& content = *it;

			ss << content.toString();

			if(it + 1 != content_.end()) {
				ss << ", ";
			}
		}
		return ss.str();
    }

    std::string Counter::toString(Property p) const
    {
		if(counters_.empty()) {
			return "none";
		}
		std::stringstream ss;
		for(auto it = counters_.begin(); it != counters_.end(); ++it) {
			auto& counter = *it;

			ss << counter.first;
			if(counter.second != 0) {
				ss << " " << counter.second;
			}

			if(it + 1 != counters_.end()) {
				ss << ", ";
			}
		}
		return ss.str();

    }

    std::string CssColor::toString(Property p) const
    {
		switch(param_) {
			case CssColorParam::NONE:				return "none";
			case CssColorParam::CSS_TRANSPARENT:	return "transparent";
			case CssColorParam::VALUE: {
				// XXX we should have color return a keyword color there is one?
				std::stringstream ss; ss << color_; return ss.str();
			}
			case CssColorParam::CURRENT:			return "current";
			default:
				break;
		}
		return std::string();
    }

    std::string Cursor::toString(Property p) const
    {
		std::stringstream ss;
		for(auto& uri : uris_) {
			ss << uri->toString(p) << ", ";
		}
		switch(cursor_) {
			case css::CssCursor::AUTO:			ss << "auto"; break;
			case css::CssCursor::CROSSHAIR:		ss << "crosshair"; break;
			case css::CssCursor::DEFAULT:		ss << "default"; break;
			case css::CssCursor::POINTER:		ss << "pointer"; break;
			case css::CssCursor::MOVE:			ss << "move"; break;
			case css::CssCursor::E_RESIZE:		ss << "e-resize"; break;
			case css::CssCursor::NE_RESIZE:		ss << "ne-resize"; break;
			case css::CssCursor::NW_RESIZE:		ss << "nw-resize"; break;
			case css::CssCursor::N_RESIZE:		ss << "n-resize"; break;
			case css::CssCursor::SE_RESIZE:		ss << "se-resize"; break;
			case css::CssCursor::SW_RESIZE:		ss << "sw-resize"; break;
			case css::CssCursor::S_RESIZE:		ss << "s-resize"; break;
			case css::CssCursor::W_RESIZE:		ss << "w-resize"; break;
			case css::CssCursor::TEXT:			ss << "text"; break;
			case css::CssCursor::WAIT:			ss << "wait"; break;
			case css::CssCursor::PROGRESS:		ss << "progress"; break;
			case css::CssCursor::HELP:			ss << "help"; break;
			default: break;
		}
		return ss.str();
    }

    std::string FontFamily::toString(Property p) const
    {
		std::string s;
		for(auto it = fonts_.begin(); it != fonts_.end(); ++it) {
			s += *it;
			if(it + 1 != fonts_.end()) {
				s += ", ";
			}
		}
		return s;
    }

    std::string FontSize::toString(Property p) const
    {
		if(is_absolute_) {
			switch(absolute_) {
				case FontSizeAbsolute::NONE:		return "none";
				case FontSizeAbsolute::XX_SMALL:	return "xx-small";
				case FontSizeAbsolute::X_SMALL:		return "x-small";
				case FontSizeAbsolute::SMALL:		return "small";
				case FontSizeAbsolute::MEDIUM:		return "medium";
				case FontSizeAbsolute::LARGE:		return "large";
				case FontSizeAbsolute::X_LARGE:		return "x-large";
				case FontSizeAbsolute::XX_LARGE:	return "xx-large";
				case FontSizeAbsolute::XXX_LARGE:	return "xxx-large";
				default: break;
			}
		}
		if(is_relative_) {
			switch(relative_) {
				case FontSizeRelative::NONE:		return "none";
				case FontSizeRelative::LARGER:		return "larger";
				case FontSizeRelative::SMALLER:		return "smaller";
				default: break;
			}
		}
		return length_.toString(p);
    }

    std::string FontWeight::toString(Property p) const
    {
		if(is_relative_) {
			switch(relative_) {
				case FontWeightRelative::LIGHTER:	return "lighter";
				case FontWeightRelative::BOLDER:	return "bolder";
				default: break;
			}
		}
		std::stringstream ss;
		ss << weight_;
		return ss.str();
    }

    std::string Length::toString(Property p) const
    {
		std::stringstream ss;
		const float val = static_cast<float>(value_) / 65536.0f;
		switch(units_) {
			case LengthUnits::NUMBER:	ss << val; break;
			case LengthUnits::EM:		ss << val << "em"; break;
			case LengthUnits::EX:		ss << val << "ex"; break;
			case LengthUnits::INCHES:	ss << val << "in"; break;
			case LengthUnits::CM:		ss << val << "cm"; break;
			case LengthUnits::MM:		ss << val << "mm"; break;
			case LengthUnits::PT:		ss << val << "pt"; break;
			case LengthUnits::PC:		ss << val << "pc"; break;
			case LengthUnits::PX:		ss << val << "px"; break;
			case LengthUnits::PERCENT:	ss << val << "%"; break;
			default: break;
		}
		return ss.str();
    }

    std::string LinearGradient::toString(Property p) const
    {
		std::stringstream ss;
		ss << "linear-gradient(";

		if(std::abs(angle_) < FLT_EPSILON) {
			ss << "to top";
		} else if(std::abs(angle_ - 45.0f) < FLT_EPSILON) {
			ss << "to top right";
		} else if(std::abs(angle_ - 90.0f) < FLT_EPSILON) {
			ss << "to right";
		} else if(std::abs(angle_ - 135.0f) < FLT_EPSILON) {
			ss << "to bottom right";
		} else if(std::abs(angle_ - 180.0f) < FLT_EPSILON) {
			ss << "to bottom";
		} else if(std::abs(angle_ - 225.0f) < FLT_EPSILON) {
			ss << "to bottom left";
		} else if(std::abs(angle_ - 270.0f) < FLT_EPSILON) {
			ss << "to left";
		} else if(std::abs(angle_ - 315.0f) < FLT_EPSILON) {
			ss << "to top left";
		} else {
			ss << angle_;
		}

		for(auto it = color_stops_.cbegin(); it != color_stops_.cend(); ++it) {
			auto& cs = *it;
			ss << cs.color << cs.length.toString(p);
			if(it + 1 != color_stops_.end()) {
				ss << ",";
			}
		}

		ss << ")";
		return ss.str();
    }

    std::string Quotes::toString(Property p) const
    {
		if(isNone()) {
			return "none";
		}
		std::stringstream ss;
		for(auto it = quotes_.cbegin(); it != quotes_.cend(); ++it) {
			ss << it->first << " " << it->second;
		}
		return ss.str();
    }

    std::string TextShadowStyle::toString(Property p) const
    {
		if(shadows_.empty()) {
			return "none";
		}
		std::stringstream ss;
		for(auto it = shadows_.cbegin(); it != shadows_.cend(); ++it) {
			auto& shadow = *it;
			ss << shadow.getOffset()[0].toString(p) << " " << shadow.getOffset()[1].toString(p);
			if(shadow.getBlur().compute() != 0) {
				ss << " " << shadow.getBlur().toString(p);
			}
			ss << " " << shadow.getColor().toString(p);
			if(it + 1 != shadows_.end()) {
				ss << ",";
			}
		}
		return ss.str();
    }

    std::string TransitionProperties::toString(Property p) const
    {
		if(properties_.empty()) {
			return "none";
		}

		std::stringstream ss;
		for(auto it = properties_.cbegin(); it != properties_.cend(); ++it) {
			if(*it == Property::MAX_PROPERTIES) {
				ss << "all";
			} else {
				ss << get_property_name(*it);
			}
			if(it + 1 != properties_.end()) {
				ss << ", ";
			}
		}
		return ss.str();
    }

    std::string TransitionTiming::toString(Property p) const
    {
		if(timings_.empty()) {
			return "none";
		}

		std::stringstream ss;
		for(auto it = timings_.cbegin(); it != timings_.cend(); ++it) {
			if(*it < 1.0f) {
				ss << (*it * 1000.0f) << "ms";
			} else {
				ss << *it << "s";
			}
			if(it + 1 != timings_.end()) {
				ss << ", ";
			}
		}
		return ss.str();
    }

	std::string TimingFunction::toString() const
	{
		std::stringstream ss;
		switch(ttfn_) {
			case CssTransitionTimingFunction::STEPS:
				if(nintervals_ == 1) {
					if(poc_ == StepChangePoint::START) {
						ss << "step-start";
					} else {
						ss << "step-end";
					}
				} else {
					ss << "steps(" << nintervals_ << (poc_ == StepChangePoint::START ? ", start" : "") << ")";
				}
				break;
			case CssTransitionTimingFunction::CUBIC_BEZIER:
				if(point_compare(p1_, 0.25f, 0.1f) && point_compare(p2_, 0.25f, 1.0f)) {
					ss << "ease";
				} else if(point_compare(p1_, 0.0f, 0.0f) && point_compare(p2_, 1.0f, 1.0f)) {
					ss << "linear";
				} else if(point_compare(p1_, 0.42f, 0.0f) && point_compare(p2_, 1.0f, 1.0f)) {
					ss << "ease-in";
				} else if(point_compare(p1_, 0.0f, 0.0f) && point_compare(p2_, 0.58f, 1.0f)) {
					ss << "ease-out";
				} else if(point_compare(p1_, 0.42f, 0.0f) && point_compare(p2_, 0.58f, 1.0f)) {
					ss << "ease-in-out";
				} else {
					ss << "cubic-bezier(" << p1_.x << "," << p1_.y << "," << p2_.x << "," << p2_.y << ")";
				}
				break;
			default: break;
		}
		return ss.str();
	}

    std::string TransitionTimingFunctions::toString(Property p) const
    {
		if(ttfns_.empty()) {
			return "none";
		}

		std::stringstream ss;
		for(auto it = ttfns_.cbegin(); it != ttfns_.cend(); ++it) {
			ss << it->toString();
			if(it + 1 != ttfns_.end()) {
				ss << ", ";
			}
		}
		return ss.str();
    }

    std::string UriStyle::toString(Property p) const
    {
		if(isNone()) {
			return "none";
		}
		return "uri(" + uri_ + ")";
    }

    std::string VerticalAlign::toString(Property p) const
    {
		switch(va_) {
			case CssVerticalAlign::BASELINE:		return "baseline";
			case CssVerticalAlign::SUB:				return "sub";
			case CssVerticalAlign::SUPER:			return "super";
			case CssVerticalAlign::TOP:				return "top";
			case CssVerticalAlign::TEXT_TOP:		return "text-top";
			case CssVerticalAlign::MIDDLE:			return "middle";
			case CssVerticalAlign::BOTTOM:			return "bottom";
			case CssVerticalAlign::TEXT_BOTTOM:		return "text-bottom";
			case CssVerticalAlign::LENGTH:			return len_.toString(p);
			default: break;
		}
		return std::string();
    }

    std::string Width::toString(Property p) const
    {
		if(isAuto()) {
			return "auto";
		}
		return width_.toString(p);
    }

    std::string WidthList::toString(Property p) const
    {
		std::stringstream ss;
		for(auto& w : widths_) {
			ss << w.toString(p) << " ";
		}
		return ss.str();
    }

    std::string Zindex::toString(Property p) const
    {
		if(isAuto()) {
			return "auto";
		}
		std::stringstream ss;
		ss << index_;
		return ss.str();
    }

	/*
		// XXX roughly compute what the stops should be, there doesn't seem to be an algorithm specfied for this, so we
		// make one up.

		// Make first and last stops be 0% and 100% respectively if they weren't specified.
		if(stops.size() > 0 && stops.front().length.isNumber()) {
			stops.front().length = Length(0, true);
		}
		if(stops.size() > 1 && stops.back().length.isNumber()) {
			stops.back().length = Length(0, true);
		}
		if(stops.size() > 2) {
			auto it = stops.begin() + 1;
			auto end = stops.end() - 1;
			for(; it != end; ++it) {
				auto prev = it - 1;
				auto next = it + 1;
				if(it->length.isNumber()) {
					it->length = Length();
				}
			}
		}
	*/
}

void test()
{
	using namespace css;
	auto p = Style::create<Display>(StyleId::DISPLAY, Display::BLOCK);
}
