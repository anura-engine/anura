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

#include <set>

#include "asserts.hpp"
#include "formatter.hpp"
#include "css_parser.hpp"
#include "css_properties.hpp"

namespace css
{
	namespace 
	{
		const int fixed_point_scale = 65536;

		const xhtml::FixedPoint border_width_thin = 2 * fixed_point_scale;
		const xhtml::FixedPoint border_width_medium = 4 * fixed_point_scale;
		const xhtml::FixedPoint border_width_thick = 10 * fixed_point_scale;

		const xhtml::FixedPoint line_height_scale = (120 * fixed_point_scale) / 100;

		const xhtml::FixedPoint default_font_size = 12 * fixed_point_scale;

		typedef std::function<void(PropertyParser*, const std::string&, const std::string&)> ParserFunction;
		struct PropertyNameInfo
		{
			PropertyNameInfo() : value(Property::MAX_PROPERTIES), fn() {}
			PropertyNameInfo(Property p, std::function<void(PropertyParser*)> f) : value(p), fn(f) {}
			Property value;
			std::function<void(PropertyParser*)> fn;
		};

		typedef std::map<std::string, PropertyNameInfo> property_map;
		property_map& get_property_table()
		{
			static property_map res;
			return res;
		}

		typedef std::vector<PropertyInfo> property_info_list;
		property_info_list& get_property_info_table()
		{
			static property_info_list res;
			if(res.empty()) {
				res.resize(static_cast<int>(Property::MAX_PROPERTIES));
			}
			return res;
		}

		std::vector<std::string>& get_default_fonts()
		{
			static std::vector<std::string> res;
			if(res.empty()) {
				res.emplace_back("arial.ttf");
				res.emplace_back("FreeSerif.ttf");
			}
			return res;
		}

		KRE::Color hsla_to_color(float h, float s, float l, float a)
		{
			const float hue_upper_limit = 360.0f;
			float c = 0.0f, m = 0.0f, x = 0.0f;
			c = (1.0f - std::abs(2.f * l - 1.0f)) * s;
			m = 1.0f * (l - 0.5f * c);
			x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.f) - 1.0f));
			if (h >= 0.0f && h < (hue_upper_limit / 6.0f)) {
				return KRE::Color(c+m, x+m, m, a);
			} else if (h >= (hue_upper_limit / 6.0f) && h < (hue_upper_limit / 3.0f)) {
				return KRE::Color(x+m, c+m, m, a);
			} else if (h < (hue_upper_limit / 3.0f) && h < (hue_upper_limit / 2.0f)) {
				return KRE::Color(m, c+m, x+m, a);
			} else if (h >= (hue_upper_limit / 2.0f) && h < (2.0f * hue_upper_limit / 3.0f)) {
				return KRE::Color(m, x+m, c+m, a);
			} else if (h >= (2.0 * hue_upper_limit / 3.0f) && h < (5.0f * hue_upper_limit / 6.0f)) {
				return KRE::Color(x+m, m, c+m, a);
			} else if (h >= (5.0 * hue_upper_limit / 6.0f) && h < hue_upper_limit) {
				return KRE::Color(c+m, m, x+m, a);
			}
			return KRE::Color(m, m, m, a);
		}

		// These are the properties that can be animated using the transition* properties.
		std::set<Property>& get_transitional_properties()
		{
			static std::set<Property> res;
			if(res.empty()) {
				res.emplace(Property::BACKGROUND_COLOR);
				res.emplace(Property::BACKGROUND_POSITION);
				res.emplace(Property::BORDER_TOP_COLOR);
				res.emplace(Property::BORDER_TOP_WIDTH);
				res.emplace(Property::BORDER_BOTTOM_COLOR);
				res.emplace(Property::BORDER_BOTTOM_WIDTH);
				res.emplace(Property::BORDER_LEFT_COLOR);
				res.emplace(Property::BORDER_LEFT_WIDTH);
				res.emplace(Property::BORDER_RIGHT_COLOR);
				res.emplace(Property::BORDER_RIGHT_WIDTH);
				res.emplace(Property::BORDER_SPACING);
				res.emplace(Property::BOTTOM);
				res.emplace(Property::CLIP);
				res.emplace(Property::COLOR);
				res.emplace(Property::FONT_SIZE);
				res.emplace(Property::FONT_WEIGHT);
				res.emplace(Property::HEIGHT);
				res.emplace(Property::LEFT);
				res.emplace(Property::LETTER_SPACING);
				res.emplace(Property::LINE_HEIGHT);
				res.emplace(Property::MARGIN_BOTTOM);
				res.emplace(Property::MARGIN_LEFT);
				res.emplace(Property::MARGIN_RIGHT);
				res.emplace(Property::MARGIN_TOP);
				res.emplace(Property::MAX_HEIGHT);
				res.emplace(Property::MAX_WIDTH);
				res.emplace(Property::MIN_HEIGHT);
				res.emplace(Property::MIN_WIDTH);
				res.emplace(Property::OPACITY);
				res.emplace(Property::OUTLINE_COLOR);
				res.emplace(Property::OUTLINE_WIDTH);
				res.emplace(Property::PADDING_BOTTOM);
				res.emplace(Property::PADDING_LEFT);
				res.emplace(Property::PADDING_RIGHT);
				res.emplace(Property::PADDING_TOP);
				res.emplace(Property::RIGHT);
				res.emplace(Property::TEXT_INDENT);
				res.emplace(Property::TEXT_SHADOW);
				res.emplace(Property::TOP);
				res.emplace(Property::VERTICAL_ALIGN);
				res.emplace(Property::VISIBILITY);
				res.emplace(Property::WIDTH);
				res.emplace(Property::WORD_SPACING);
				res.emplace(Property::Z_INDEX);
				res.emplace(Property::FILTER);
				res.emplace(Property::TRANSFORM);
			}
			return res;
		}

		struct PropertyRegistrar
		{
			PropertyRegistrar(const std::string& name, std::function<void(PropertyParser*)> fn) {
				get_property_table()[name] = PropertyNameInfo(Property::MAX_PROPERTIES, fn);
			}
			PropertyRegistrar(const std::string& name, Property p, bool inherited, StylePtr def, std::function<void(PropertyParser*)> fn) {
				get_property_table()[name] = PropertyNameInfo(p, fn);
				ASSERT_LOG(static_cast<int>(p) < static_cast<int>(get_property_info_table().size()),
					"Something went wrong. Tried to add a property outside of the maximum range of our property list table.");
				get_property_info_table()[static_cast<int>(p)].name = name;
				get_property_info_table()[static_cast<int>(p)].inherited = inherited;
				get_property_info_table()[static_cast<int>(p)].obj = def;
				get_property_info_table()[static_cast<int>(p)].is_default = true;
			}
		};

		using namespace std::placeholders;
		
		PropertyRegistrar property000("background-color", Property::BACKGROUND_COLOR, false, CssColor::create(CssColorParam::CSS_TRANSPARENT), std::bind(&PropertyParser::parseColor, _1, "background-color", ""));
		PropertyRegistrar property001("color", Property::COLOR, true, CssColor::create(CssColorParam::VALUE), std::bind(&PropertyParser::parseColor, _1, "color", ""));
		PropertyRegistrar property002("padding-left", Property::PADDING_LEFT, false, Length::create(0, false), std::bind(&PropertyParser::parseLength, _1, "padding-left", ""));
		PropertyRegistrar property003("padding-right", Property::PADDING_RIGHT, false, Length::create(0, false), std::bind(&PropertyParser::parseLength, _1, "padding-right", ""));
		PropertyRegistrar property004("padding-top", Property::PADDING_TOP, false, Length::create(0, false), std::bind(&PropertyParser::parseLength, _1, "padding-top", ""));
		PropertyRegistrar property005("padding-bottom", Property::PADDING_BOTTOM, false, Length::create(0, false), std::bind(&PropertyParser::parseLength, _1, "padding-bottom", ""));
		PropertyRegistrar property006("padding", std::bind(&PropertyParser::parseLengthList, _1, "padding", ""));
		PropertyRegistrar property007("margin-left", Property::MARGIN_LEFT, false, std::make_shared<Width>(Length(0, false)), std::bind(&PropertyParser::parseWidth, _1, "margin-left", ""));
		PropertyRegistrar property008("margin-right", Property::MARGIN_RIGHT, false, std::make_shared<Width>(Length(0, false)), std::bind(&PropertyParser::parseWidth, _1, "margin-right", ""));
		PropertyRegistrar property009("margin-top", Property::MARGIN_TOP, false, std::make_shared<Width>(Length(0, false)), std::bind(&PropertyParser::parseWidth, _1, "margin-top", ""));
		PropertyRegistrar property010("margin-bottom", Property::MARGIN_BOTTOM, false, std::make_shared<Width>(Length(0, false)), std::bind(&PropertyParser::parseWidth, _1, "margin-bottom", ""));
		PropertyRegistrar property011("margin", std::bind(&PropertyParser::parseWidthList, _1, "margin", ""));
		PropertyRegistrar property012("border-top-color", Property::BORDER_TOP_COLOR, false, CssColor::create(CssColorParam::CURRENT), std::bind(&PropertyParser::parseColor, _1, "border-top-color", ""));
		PropertyRegistrar property013("border-left-color", Property::BORDER_LEFT_COLOR, false, CssColor::create(CssColorParam::CURRENT), std::bind(&PropertyParser::parseColor, _1, "border-left-color", ""));
		PropertyRegistrar property014("border-bottom-color", Property::BORDER_BOTTOM_COLOR, false, CssColor::create(CssColorParam::CURRENT), std::bind(&PropertyParser::parseColor, _1, "border-bottom-color", ""));
		PropertyRegistrar property015("border-right-color", Property::BORDER_RIGHT_COLOR, false, CssColor::create(CssColorParam::CURRENT), std::bind(&PropertyParser::parseColor, _1, "border-right-color", ""));
		PropertyRegistrar property016("border-color", std::bind(&PropertyParser::parseColorList, _1, "border", "color"));
		PropertyRegistrar property017("border-top-width", Property::BORDER_TOP_WIDTH, false, Length::create(border_width_medium, LengthUnits::PX), std::bind(&PropertyParser::parseBorderWidth, _1, "border-top-width", ""));
		PropertyRegistrar property018("border-left-width", Property::BORDER_LEFT_WIDTH, false, Length::create(border_width_medium, LengthUnits::PX), std::bind(&PropertyParser::parseBorderWidth, _1, "border-left-width", ""));
		PropertyRegistrar property019("border-bottom-width", Property::BORDER_BOTTOM_WIDTH, false, Length::create(border_width_medium, LengthUnits::PX), std::bind(&PropertyParser::parseBorderWidth, _1, "border-bottom-width", ""));
		PropertyRegistrar property020("border-right-width", Property::BORDER_RIGHT_WIDTH, false, Length::create(border_width_medium, LengthUnits::PX), std::bind(&PropertyParser::parseBorderWidth, _1, "border-right-width", ""));
		PropertyRegistrar property021("border-width", std::bind(&PropertyParser::parseBorderWidthList, _1, "border", "width"));
		PropertyRegistrar property022("border-top-style", Property::BORDER_TOP_STYLE, false, Style::create<BorderStyle>(StyleId::BORDER_STYLE, BorderStyle::NONE), std::bind(&PropertyParser::parseBorderStyle, _1, "border-top-style", ""));
		PropertyRegistrar property023("border-left-style", Property::BORDER_LEFT_STYLE, false, Style::create<BorderStyle>(StyleId::BORDER_STYLE, BorderStyle::NONE), std::bind(&PropertyParser::parseBorderStyle, _1, "border-left-style", ""));
		PropertyRegistrar property024("border-bottom-style", Property::BORDER_BOTTOM_STYLE, false, Style::create<BorderStyle>(StyleId::BORDER_STYLE, BorderStyle::NONE), std::bind(&PropertyParser::parseBorderStyle, _1, "border-bottom-style", ""));
		PropertyRegistrar property025("border-right-style", Property::BORDER_RIGHT_STYLE, false, Style::create<BorderStyle>(StyleId::BORDER_STYLE, BorderStyle::NONE), std::bind(&PropertyParser::parseBorderStyle, _1, "border-right-style", ""));
		PropertyRegistrar property026("display", Property::DISPLAY, false, Style::create<Display>(StyleId::DISPLAY, Display::INLINE), std::bind(&PropertyParser::parseDisplay, _1, "display", ""));	
		PropertyRegistrar property027("width", Property::WIDTH, false, Width::create(true), std::bind(&PropertyParser::parseWidth, _1, "width", ""));	
		PropertyRegistrar property028("height", Property::HEIGHT, false, Width::create(true), std::bind(&PropertyParser::parseWidth, _1, "height", ""));
		PropertyRegistrar property029("white-space", Property::WHITE_SPACE, true, Style::create<Whitespace>(StyleId::WHITE_SPACE, Whitespace::NORMAL), std::bind(&PropertyParser::parseWhitespace, _1, "white-space", ""));	
		PropertyRegistrar property030("font-family", Property::FONT_FAMILY, true, FontFamily::create(get_default_fonts()), std::bind(&PropertyParser::parseFontFamily, _1, "font-family", ""));	
		PropertyRegistrar property031("font-size", Property::FONT_SIZE, true, FontSize::create(Length(default_font_size, LengthUnits::PT)), std::bind(&PropertyParser::parseFontSize, _1, "font-size", ""));
		PropertyRegistrar property032("font-style", Property::FONT_STYLE, true, Style::create<FontStyle>(StyleId::FONT_STYLE, FontStyle::NORMAL), std::bind(&PropertyParser::parseFontStyle, _1, "font-style", ""));
		PropertyRegistrar property033("font-variant", Property::FONT_VARIANT, true, Style::create<FontVariant>(StyleId::FONT_VARIANT, FontVariant::NORMAL), std::bind(&PropertyParser::parseFontVariant, _1, "font-variant", ""));
		PropertyRegistrar property034("font-weight", Property::FONT_WEIGHT, true, FontWeight::create(400), std::bind(&PropertyParser::parseFontWeight, _1, "font-weight", ""));
		//PropertyRegistrar property035("font", std::bind(&PropertyParser::parseFont, _1, "font", ""));
		PropertyRegistrar property036("letter-spacing", Property::LETTER_SPACING, true, Length::create(0, false), std::bind(&PropertyParser::parseSpacing, _1, "letter-spacing", ""));
		PropertyRegistrar property037("word-spacing", Property::WORD_SPACING, true, Length::create(0, false), std::bind(&PropertyParser::parseSpacing, _1, "word-spacing", ""));
		PropertyRegistrar property038("text-align", Property::TEXT_ALIGN, true, Style::create<TextAlign>(StyleId::TEXT_ALIGN, TextAlign::NORMAL), std::bind(&PropertyParser::parseTextAlign, _1, "text-align", ""));
		PropertyRegistrar property039("direction", Property::DIRECTION, true, Style::create<Direction>(StyleId::DIRECTION, Direction::LTR), std::bind(&PropertyParser::parseDirection, _1, "direction", ""));
		PropertyRegistrar property040("text-transform", Property::TEXT_TRANSFORM, true, Style::create<TextTransform>(StyleId::TEXT_TRANSFORM, TextTransform::NONE), std::bind(&PropertyParser::parseTextTransform, _1, "text-transform", ""));
		PropertyRegistrar property041("line-height", Property::LINE_HEIGHT, true, Length::create(line_height_scale, false), std::bind(&PropertyParser::parseLineHeight, _1, "line-height", ""));
		PropertyRegistrar property042("overflow", Property::CSS_OVERFLOW, false, Style::create<Overflow>(StyleId::CSS_OVERFLOW, Overflow::VISIBLE), std::bind(&PropertyParser::parseOverflow, _1, "overflow", ""));
		PropertyRegistrar property043("position", Property::POSITION, false, Style::create<Position>(StyleId::POSITION, Position::STATIC), std::bind(&PropertyParser::parsePosition, _1, "position", ""));
		PropertyRegistrar property044("float", Property::FLOAT, false, Style::create<Float>(StyleId::FLOAT, Float::NONE), std::bind(&PropertyParser::parseFloat, _1, "float", ""));
		PropertyRegistrar property045("left", Property::LEFT, false, Width::create(true), std::bind(&PropertyParser::parseWidth, _1, "left", ""));
		PropertyRegistrar property046("top", Property::TOP, false, Width::create(true), std::bind(&PropertyParser::parseWidth, _1, "top", ""));
		PropertyRegistrar property047("right", Property::RIGHT, false, Width::create(true), std::bind(&PropertyParser::parseWidth, _1, "right", ""));
		PropertyRegistrar property048("bottom", Property::BOTTOM, false, Width::create(true), std::bind(&PropertyParser::parseWidth, _1, "bottom", ""));
		PropertyRegistrar property049("background-image", Property::BACKGROUND_IMAGE, false, nullptr, std::bind(&PropertyParser::parseImageSource, _1, "background-image", ""));
		PropertyRegistrar property050("background-repeat", Property::BACKGROUND_REPEAT, false, Style::create<BackgroundRepeat>(StyleId::BACKGROUND_REPEAT, BackgroundRepeat::REPEAT), std::bind(&PropertyParser::parseBackgroundRepeat, _1, "background-repeat", ""));
		PropertyRegistrar property051("background-position", Property::BACKGROUND_POSITION, false, BackgroundPosition::create(), std::bind(&PropertyParser::parseBackgroundPosition, _1, "background-position", ""));
		PropertyRegistrar property052("list-style-type", Property::LIST_STYLE_TYPE, true, Style::create<ListStyleType>(StyleId::LIST_STYLE_TYPE, ListStyleType::DISC), std::bind(&PropertyParser::parseListStyleType, _1, "list-style-type", ""));
		PropertyRegistrar property053("border-style", std::bind(&PropertyParser::parseBorderStyleList, _1, "border", "style"));
		PropertyRegistrar property054("border", std::bind(&PropertyParser::parseBorder, _1, "border", ""));
		PropertyRegistrar property055("outline", std::bind(&PropertyParser::parseBorder, _1, "outline", ""));
		PropertyRegistrar property056("outline-width", Property::OUTLINE_WIDTH, false, Length::create(border_width_medium, LengthUnits::PX), std::bind(&PropertyParser::parseBorderWidth, _1, "outline-width", ""));
		PropertyRegistrar property057("outline-style", Property::OUTLINE_STYLE, false, Style::create<BorderStyle>(StyleId::BORDER_STYLE, BorderStyle::NONE), std::bind(&PropertyParser::parseBorderStyle, _1, "outline-style", ""));
		PropertyRegistrar property058("outline-color", Property::OUTLINE_COLOR, false, CssColor::create(CssColorParam::CURRENT), std::bind(&PropertyParser::parseColor, _1, "outline-color", ""));		
		PropertyRegistrar property059("background-attachment", Property::BACKGROUND_ATTACHMENT, false, Style::create<BackgroundAttachment>(StyleId::BACKGROUND_ATTACHMENT, BackgroundAttachment::FIXED), std::bind(&PropertyParser::parseBackgroundAttachment, _1, "background-attachment", ""));
		PropertyRegistrar property060("clear", Property::CLEAR, false, Style::create<Clear>(StyleId::ClEAR, Clear::NONE), std::bind(&PropertyParser::parseClear, _1, "clear", ""));
		PropertyRegistrar property061("clip", Property::CLIP, false, Clip::create(), std::bind(&PropertyParser::parseClip, _1, "clip", ""));
		PropertyRegistrar property062("content", Property::CONTENT, false, Content::create(), std::bind(&PropertyParser::parseContent, _1, "content", ""));
		PropertyRegistrar property063("counter-increment", Property::COUNTER_INCREMENT, false, Counter::create(), std::bind(&PropertyParser::parseCounter, _1, "counter-increment", ""));
		PropertyRegistrar property064("counter-reset", Property::COUNTER_RESET, false, Counter::create(), std::bind(&PropertyParser::parseCounter, _1, "counter-reset", ""));
		PropertyRegistrar property065("list-style-image", Property::LIST_STYLE_IMAGE, false, nullptr, std::bind(&PropertyParser::parseImageSource, _1, "list-style-image", ""));
		PropertyRegistrar property066("list-style-position", Property::LIST_STYLE_POSITION, false, Style::create<ListStylePosition>(StyleId::LIST_STYLE_POSITION, ListStylePosition::OUTSIDE), std::bind(&PropertyParser::parseListStylePosition, _1, "list-style-position", ""));
		PropertyRegistrar property067("max-height", Property::MAX_HEIGHT, false, Width::create(true), std::bind(&PropertyParser::parseWidth, _1, "max-height", ""));
		PropertyRegistrar property068("max-width", Property::MAX_WIDTH, false, Width::create(true), std::bind(&PropertyParser::parseWidth, _1, "max-width", ""));
		PropertyRegistrar property069("min-height", Property::MIN_HEIGHT, false, Width::create(true), std::bind(&PropertyParser::parseWidth, _1, "min-height", ""));
		PropertyRegistrar property070("min-width", Property::MIN_WIDTH, false, Width::create(true), std::bind(&PropertyParser::parseWidth, _1, "min-width", ""));
		PropertyRegistrar property071("quotes", Property::QUOTES, false, Quotes::create(), std::bind(&PropertyParser::parseQuotes, _1, "quotes", ""));
		PropertyRegistrar property072("text-decoration", Property::TEXT_DECORATION, false, Style::create<TextDecoration>(StyleId::TEXT_DECORATION, TextDecoration::NONE), std::bind(&PropertyParser::parseTextDecoration, _1, "text-decoration", ""));
		PropertyRegistrar property073("text-indent", Property::TEXT_INDENT, true, Width::create(true), std::bind(&PropertyParser::parseWidth, _1, "text-indent", ""));
		PropertyRegistrar property074("unicode-bidi", Property::UNICODE_BIDI, false, Style::create<UnicodeBidi>(StyleId::UNICODE_BIDI, UnicodeBidi::NORMAL), std::bind(&PropertyParser::parseUnicodeBidi, _1, "unicode-bidi", ""));
		PropertyRegistrar property075("vertical-align", Property::VERTICAL_ALIGN, false, VerticalAlign::create(), std::bind(&PropertyParser::parseVerticalAlign, _1, "vertical-align", ""));
		PropertyRegistrar property076("visibility", Property::VISIBILITY, true, Style::create<Visibility>(StyleId::VISIBILITY, Visibility::VISIBLE), std::bind(&PropertyParser::parseVisibility, _1, "visibility", ""));
		PropertyRegistrar property077("z-index", Property::Z_INDEX, false, Zindex::create(), std::bind(&PropertyParser::parseZindex, _1, "z-index", ""));
		PropertyRegistrar property078("cursor", Property::CURSOR, true, Cursor::create(), std::bind(&PropertyParser::parseCursor, _1, "cursor", ""));
		PropertyRegistrar property079("background", std::bind(&PropertyParser::parseBackground, _1, "background", ""));
		PropertyRegistrar property080("list-style", std::bind(&PropertyParser::parseListStyle, _1, "list-style", ""));

		// CSS3 provisional properties
		PropertyRegistrar property200("box-shadow", Property::BOX_SHADOW, false, BoxShadowStyle::create(), std::bind(&PropertyParser::parseBoxShadow, _1, "box-shadow", ""));

		PropertyRegistrar property210("border-image-source", Property::BORDER_IMAGE_SOURCE, false, nullptr, std::bind(&PropertyParser::parseImageSource, _1, "border-image-source", ""));
		PropertyRegistrar property211("border-image-repeat", Property::BORDER_IMAGE_REPEAT, false, BorderImageRepeat::create(), std::bind(&PropertyParser::parseBorderImageRepeat, _1, "border-image-repeat", ""));
		PropertyRegistrar property212("border-image-width", Property::BORDER_IMAGE_WIDTH, false, WidthList::create(1.0f), std::bind(&PropertyParser::parseWidthList2, _1, "border-image-width", ""));
		PropertyRegistrar property213("border-image-outset", Property::BORDER_IMAGE_OUTSET, false, WidthList::create(0.0f), std::bind(&PropertyParser::parseWidthList2, _1, "border-image-outset", ""));
		PropertyRegistrar property214("border-image-slice", Property::BORDER_IMAGE_SLICE, false, BorderImageSlice::create(), std::bind(&PropertyParser::parseBorderImageSlice, _1, "border-image-slice", ""));
		PropertyRegistrar property215("border-image", std::bind(&PropertyParser::parseBorderImage, _1, "border-image", ""));
		
		PropertyRegistrar property216("border-top-left-radius", Property::BORDER_TOP_LEFT_RADIUS, false, BorderRadius::create(), std::bind(&PropertyParser::parseSingleBorderRadius, _1, "border-top-left-radius", ""));
		PropertyRegistrar property217("border-top-right-radius", Property::BORDER_TOP_RIGHT_RADIUS, false, BorderRadius::create(), std::bind(&PropertyParser::parseSingleBorderRadius, _1, "border-top-right-radius", ""));
		PropertyRegistrar property218("border-bottom-left-radius", Property::BORDER_BOTTOM_LEFT_RADIUS, false, BorderRadius::create(), std::bind(&PropertyParser::parseSingleBorderRadius, _1, "border-bottom-left-radius", ""));
		PropertyRegistrar property219("border-bottom-right-radius", Property::BORDER_BOTTOM_RIGHT_RADIUS, false, BorderRadius::create(), std::bind(&PropertyParser::parseSingleBorderRadius, _1, "border-bottom-right-radius", ""));
		PropertyRegistrar property220("border-radius", std::bind(&PropertyParser::parseBorderRadius, _1, "border", "radius"));

		PropertyRegistrar property230("background-clip", Property::BACKGROUND_CLIP, false, Style::create<BackgroundClip>(StyleId::BACKGROUND_CLIP, BackgroundClip::BORDER_BOX), std::bind(&PropertyParser::parseBackgroundClip, _1, "background-clip", ""));
		PropertyRegistrar property231("opacity", Property::OPACITY, false, Length::create(fixed_point_scale, false), std::bind(&PropertyParser::parseLength, _1, "opacity", ""));
		PropertyRegistrar property232("text-shadow", Property::TEXT_SHADOW, false, nullptr, std::bind(&PropertyParser::parseTextShadow, _1, "text-shadow", ""));

		PropertyRegistrar property250("transition-property", Property::TRANSITION_PROPERTY, false, TransitionProperties::create(), std::bind(&PropertyParser::parseTransitionProperty, _1, "transition-property", ""));
		PropertyRegistrar property251("transition-duration", Property::TRANSITION_DURATION, false, TransitionTiming::create(), std::bind(&PropertyParser::parseTransitionTiming, _1, "transition-duration", ""));
		PropertyRegistrar property252("transition-delay", Property::TRANSITION_DELAY, false, TransitionTiming::create(), std::bind(&PropertyParser::parseTransitionTiming, _1, "transition-delay", ""));
		PropertyRegistrar property253("transition-timing-function", Property::TRANSITION_TIMING_FUNCTION, false, TransitionTimingFunctions::create(), std::bind(&PropertyParser::parseTransitionTimingFunction, _1, "transition-timing-function", ""));
		PropertyRegistrar property254("transition", std::bind(&PropertyParser::parseTransition, _1, "transition", ""));

		PropertyRegistrar property260("filter", Property::FILTER, false, FilterStyle::create(), std::bind(&PropertyParser::parseFilters, _1, "filter", ""));
		
		PropertyRegistrar property270("transform", Property::TRANSFORM, false, TransformStyle::create(), std::bind(&PropertyParser::parseTransform, _1, "transform", ""));
		PropertyRegistrar property271("transform-origin", Property::TRANSFORM_ORIGIN, false, TransformStyle::create(), std::bind(&PropertyParser::parseBackgroundPosition, _1, "transform-origin", ""));

		// Compound properties -- still to be implemented.
		// font

		// Table related properties -- still to be implemented.
		// border-collapse
		// border-spacing
		// caption-side
		// empty-cells
		// table-layout
		
		// Paged -- probably won't implement.
		// orphans
		// widows

		// transition -- transition-property, transition-duration, transition-timing-function, transition-delay
		// text-shadow [<x-offset> <y-offset> <blur> <color of shadow>]+
		// border-radius <dimension>
		// opacity
		// border-image
		// border-image-source
		// border-image-slice
		// border-image-width
		// border-image-outset
		// border-image-stretch
	}
	
	PropertyList::PropertyList()
		: properties_()
	{
	}

	void PropertyList::addProperty(Property p, StylePtr o, const Specificity& specificity)
	{
		PropertyInfo defaults = get_property_info_table()[static_cast<int>(p)];

		auto it = properties_.find(p);
		if(it == properties_.end()) {
			// unconditionally add new properties
			properties_[p] = PropertyStyle(o, specificity);
		} else {
			// check for important flag before merging.
			/*LOG_DEBUG("property: " << get_property_info_table()[static_cast<int>(p)].name << ", current spec: "
				<< it->second.specificity[0] << "," << it->second.specificity[1] << "," << it->second.specificity[2]
				<< ", new spec: " << specificity[0] << "," << specificity[1] << "," << specificity[2] 
				<< ", test: " << (it->second.specificity <= specificity ? "true" : "false"));*/
			if(((it->second.style->isImportant() && o->isImportant()) || !it->second.style->isImportant()) && it->second.specificity <= specificity) {
				it->second = PropertyStyle(o, specificity);
			}
		}
	}

	void PropertyList::addProperty(const std::string& name, StylePtr o)
	{
		//ASSERT_LOG(o != nullptr, "Adding invalid property is nullptr.");
		auto prop_it = get_property_table().find(name);
		if(prop_it == get_property_table().end()) {
			LOG_ERROR("Not adding property '" << name << "' since we have no mapping for it.");
			return;
		}
		addProperty(prop_it->second.value, o);
	}

	StylePtr PropertyList::getProperty(Property value) const
	{
		auto it = properties_.find(value);
		if(it == properties_.end()) {
			return nullptr;
		}
		return it->second.style;
	}

	void PropertyList::merge(const Specificity& specificity, const PropertyList& plist)
	{
		for(auto& p : plist.properties_) {
			addProperty(p.first, p.second.style, specificity);
		}
	}

	void PropertyList::markTransitions()
	{
		// annotate any styles that have transitions.
		auto it = properties_.find(Property::TRANSITION_PROPERTY);
		if(it == properties_.end() || it->second.style == nullptr) {
			// no transition properties listed, just exit.
			return;
		}
		// Find duration
		auto dura_it = properties_.find(Property::TRANSITION_DURATION);
		if(dura_it == properties_.end() || dura_it->second.style == nullptr) {
			// no duration and default is 0s
			return;
		}
		const std::vector<float>& duration = dura_it->second.style->asType<TransitionTiming>()->getTiming();
		
		// Find delays, if any
		auto delay_it = properties_.find(Property::TRANSITION_DELAY);
		std::vector<float> delay;
		if(delay_it == properties_.end() || delay_it->second.style == nullptr) {
			// no delay and default is 0s
			delay.emplace_back(0.0f);
		} else {
			delay = delay_it->second.style->asType<TransitionTiming>()->getTiming();
			if(delay.empty()) {
				delay.emplace_back(0.0f);
			}
		}

		// timing functions, if any.
		auto ttfn_it = properties_.find(Property::TRANSITION_TIMING_FUNCTION);
		std::vector<TimingFunction> ttfns;
		if(ttfn_it == properties_.end() || ttfn_it->second.style == nullptr) {
			ttfns.emplace_back(TimingFunction());
		} else {
			ttfns = ttfn_it->second.style->asType<TransitionTimingFunctions>()->getTimingFunctions();
			if(ttfns.empty())  {
				ttfns.emplace_back(TimingFunction());
			}
		}

		auto tprops = it->second.style->asType<TransitionProperties>();
		// we already know the properties here are transitional ones. 
		// since we checked when parsing. Except MAX_PROPERTY which is
		// a holder meaning all.
		int index = 0;
		for(auto& p : tprops->getProperties()) {
			if(p == Property::MAX_PROPERTIES) {
				// all
				for(auto& prop : properties_) {
					auto pit = get_transitional_properties().find(prop.first);
					if(pit != get_transitional_properties().end()) {
						// is transitional
						prop.second.style->addTransition(duration[index % duration.size()], 
							ttfns[index % ttfns.size()],
							delay[index % delay.size()]);
					}
				}
			} else {
				auto it = properties_.find(p);
				if(it == properties_.end()) {
					// didn't find property.
					++index;
					continue;
				}
				it->second.style->addTransition(duration[index % duration.size()], 
					ttfns[index % ttfns.size()],
					delay[index % delay.size()]);
			}
			++index;
		}
	}

	Property get_property_by_name(const std::string& name)
	{
		auto prop_it = get_property_table().find(name);
		if(prop_it == get_property_table().end()) {
			LOG_ERROR("Not adding property '" << name << "' since we have no mapping for it.");
			return Property::MAX_PROPERTIES;
		}
		return prop_it->second.value;
	}

	const std::string& get_property_name(Property p)
	{
		const int ndx = static_cast<int>(p);
		ASSERT_LOG(ndx < static_cast<int>(get_property_info_table().size()), 
			"Requested name of property, index not in table: " << ndx);
		return get_property_info_table()[ndx].name;
	}

	const PropertyInfo& get_default_property_info(Property p)
	{
		const int ndx = static_cast<int>(p);
		ASSERT_LOG(ndx < static_cast<int>(get_property_info_table().size()), 
			"Requested property info, index not in table: " << ndx);
		return get_property_info_table()[ndx];
	}

	PropertyParser::PropertyParser()
		: it_(),
		  end_(),
		  plist_()
	{
	}

	PropertyParser::const_iterator PropertyParser::parse(const std::string& name, const const_iterator& begin, const const_iterator& end)
	{
		it_ = begin;
		end_ = end;

		auto it = get_property_table().find(name);
		if(it == get_property_table().end()) {
			throw ParserError(formatter() << "Unable to find a parse function for property '" << name << "'");
		}
		it->second.fn(this);

		return it_;
	}

	void PropertyParser::inheritProperty(const std::string& name)
	{
		// called to make the property with the given name as inherited
		auto it = get_property_table().find(name);
		if(it == get_property_table().end()) {
			throw ParserError(formatter() << "Unable to find a parse function for property '" << name << "'");
		}
		plist_.addProperty(name, std::make_shared<Style>(true));
	}

	void PropertyParser::advance()
	{
		if(it_ != end_) {
			++it_;
		}
	}

	void PropertyParser::skipWhitespace()
	{
		while(isToken(TokenId::WHITESPACE)) {
			advance();
		}
	}

	bool PropertyParser::isToken(TokenId tok) const
	{
		if(it_ == end_) {
			return tok == TokenId::EOF_TOKEN ? true : false;
		}
		return (*it_)->id() == tok;
	}

	bool PropertyParser::isTokenDelimiter(const std::string& delim) const
	{
		return isToken(TokenId::DELIM) && (*it_)->getStringValue() == delim;
	}

	bool PropertyParser::isEndToken() const 
	{
		return isToken(TokenId::EOF_TOKEN) || isToken(TokenId::RBRACE) || isToken(TokenId::SEMICOLON) || isTokenDelimiter("!");
	}

	std::vector<TokenPtr> PropertyParser::parseCSVList(TokenId end_token)
	{
		std::vector<TokenPtr> res;
		while(!isToken(TokenId::EOF_TOKEN) && !isToken(end_token) && !isToken(TokenId::SEMICOLON)) {
			skipWhitespace();
			res.emplace_back(*it_);
			advance();
			skipWhitespace();
			if(isToken(TokenId::COMMA)) {
				advance();
			} else if(!isToken(end_token) && !isToken(TokenId::EOF_TOKEN) && !isToken(TokenId::SEMICOLON)) {
				throw ParserError("Expected ',' (COMMA) while parsing color value.");
			}
		}
		if(isToken(end_token)) {
			advance();
		}
		return res;
	}

	void PropertyParser::parseCSVNumberList(TokenId end_token, std::function<void(int, float, bool)> fn)
	{
		auto toks = parseCSVList(end_token);
		int n = 0;
		for(auto& t : toks) {
			if(t->id() == TokenId::PERCENT) {
				fn(n, static_cast<float>(t->getNumericValue()), true);
			} else if(t->id() == TokenId::NUMBER) {
				fn(n, static_cast<float>(t->getNumericValue()), false);
			} else {
				throw ParserError("Expected percent or numeric value while parsing numeric list.");
			}
			++n;
		}
	}

	void PropertyParser::parseCSVStringList(TokenId end_token, std::function<void(int, const std::string&)> fn)
	{
		auto toks = parseCSVList(end_token);
		int n = 0;
		for(auto& t : toks) {
			if(t->id() == TokenId::IDENT) {
				fn(n, t->getStringValue());
			} else if(t->id() == TokenId::STRING) {
				fn(n, t->getStringValue());
			} else {
				throw ParserError("Expected ident or string value while parsing string list.");
			}
			++n;
		}
	}

	void PropertyParser::parseCSVNumberListFromIt(std::vector<TokenPtr>::const_iterator start, 
		std::vector<TokenPtr>::const_iterator end, 
		std::function<void(int, float, bool)> fn)
	{
		int n = 0;
		for(auto it = start; it != end; ++it) {
			auto& t = *it;
			if(t->id() == TokenId::NUMBER) {
				fn(n, static_cast<float>(t->getNumericValue()), false);
			} else if(t->id() == TokenId::PERCENT) {
				fn(n, static_cast<float>(t->getNumericValue()), true);
			} else if(t->id() == TokenId::COMMA) {
				++n;
			}
		}
	}

	void PropertyParser::parseColor2(std::shared_ptr<CssColor> color)
	{
		if(isToken(TokenId::FUNCTION)) {
			const std::string& ref = (*it_)->getStringValue();
			if(ref == "rgb") {
				int values[3] = { 255, 255, 255 };
				parseCSVNumberListFromIt((*it_)->getParameters().cbegin(), (*it_)->getParameters().cend(), 
					[&values](int n, float value, bool is_percent) {
					if(n < 3) {
						if(is_percent) {
							value *= 255.0f / 100.0f;
						}
						values[n] = std::min(255, std::max(0, static_cast<int>(value)));
					}
				});
				advance();
				color->setColor(KRE::Color(values[0], values[1], values[2]));
			} else if(ref == "rgba") {
				int values[4] = { 255, 255, 255, 255 };
				parseCSVNumberListFromIt((*it_)->getParameters().cbegin(), (*it_)->getParameters().cend(), 
					[&values](int n, float value, bool is_percent) {
					if(n < 4) {
						if(is_percent) {
							value *= 255.0f / 100.0f;
						}
						values[n] = std::min(255, std::max(0, static_cast<int>(value)));
					}
				});
				advance();
				color->setColor(KRE::Color(values[0], values[1], values[2], values[3]));
			} else if(ref == "hsl") {
				float values[3];
				const float multipliers[3] = { 360.0f, 1.0f, 1.0f };
				parseCSVNumberListFromIt((*it_)->getParameters().cbegin(), (*it_)->getParameters().cend(), 
					[&values, &multipliers](int n, float value, bool is_percent) {
					if(n < 3) {
						if(is_percent) {
							value *= multipliers[n] / 100.0f;
						}
						values[n] = value;
					}
				});					
				advance();
				color->setColor(hsla_to_color(values[0], values[1], values[2], 1.0));
			} else if(ref == "hsla") {
				float values[4];
				const float multipliers[4] = { 360.0f, 1.0f, 1.0f, 1.0f };
				parseCSVNumberListFromIt((*it_)->getParameters().cbegin(), (*it_)->getParameters().cend(), 
					[&values, &multipliers](int n, float value, bool is_percent) {
					if(n < 4) {
						if(is_percent) {
							value *= multipliers[n] / 100.0f;
						}
						values[n] = value;
					}
				});					
				advance();
				color->setColor(hsla_to_color(values[0], values[1], values[2], values[3]));
			} else {
				throw ParserError(formatter() << "Unexpected token for color value, found " << ref);
			}
		} else if(isToken(TokenId::HASH)) {
			const std::string& ref = (*it_)->getStringValue();
			// is #fff or #ff00ff representation
			color->setColor(KRE::Color(ref));
			advance();
		} else {
			throw ParserError(formatter() << "Unexpected token for color value, found " << Token::tokenIdToString((*it_)->id()));
		}
	}

	std::shared_ptr<css::CssColor> PropertyParser::parseColorInternal()
	{
		auto color = CssColor::create();
		if(isToken(TokenId::IDENT)) {
			const std::string& ref = (*it_)->getStringValue();
			advance();
			if(ref == "transparent") {
				color->setParam(CssColorParam::CSS_TRANSPARENT);
			} else {
				// color value is in ref.
				color->setColor(KRE::Color(ref));
			}
		} else {
			parseColor2(color);
		}
		return color;
	}

	void PropertyParser::parseColorList(const std::string& prefix, const std::string& suffix)
	{
		StylePtr w1 = parseColorInternal();
		skipWhitespace();
		if(isEndToken()) {
			plist_.addProperty(prefix + "-top" + (!suffix.empty() ? "-" + suffix : ""), w1);
			plist_.addProperty(prefix + "-bottom" + (!suffix.empty() ? "-" + suffix : ""), w1);
			plist_.addProperty(prefix + "-right" + (!suffix.empty() ? "-" + suffix : ""), w1);
			plist_.addProperty(prefix + "-left" + (!suffix.empty() ? "-" + suffix : ""), w1);
			return;
		}
		StylePtr w2 = parseColorInternal();
		skipWhitespace();
		if(isEndToken()) {
			plist_.addProperty(prefix + "-top" + (!suffix.empty() ? "-" + suffix : ""), w1);
			plist_.addProperty(prefix + "-bottom" + (!suffix.empty() ? "-" + suffix : ""), w1);
			plist_.addProperty(prefix + "-right" + (!suffix.empty() ? "-" + suffix : ""), w2);
			plist_.addProperty(prefix + "-left" + (!suffix.empty() ? "-" + suffix : ""), w2);
			return;
		}
		StylePtr w3 = parseColorInternal();
		skipWhitespace();
		if(isEndToken()) {
			plist_.addProperty(prefix + "-top" + (!suffix.empty() ? "-" + suffix : ""), w1);
			plist_.addProperty(prefix + "-right" + (!suffix.empty() ? "-" + suffix : ""), w2);
			plist_.addProperty(prefix + "-left" + (!suffix.empty() ? "-" + suffix : ""), w2);
			plist_.addProperty(prefix + "-bottom" + (!suffix.empty() ? "-" + suffix : ""), w3);
			return;
		}
		StylePtr w4 = parseColorInternal();
		skipWhitespace();

		// four values, apply to individual elements.
		plist_.addProperty(prefix + "-top" + (!suffix.empty() ? "-" + suffix : ""), w1);
		plist_.addProperty(prefix + "-right" + (!suffix.empty() ? "-" + suffix : ""), w2);
		plist_.addProperty(prefix + "-bottom" + (!suffix.empty() ? "-" + suffix : ""), w3);
		plist_.addProperty(prefix + "-left" + (!suffix.empty() ? "-" + suffix : ""), w4);
	}

	StylePtr PropertyParser::parseWidthInternal()
	{
		skipWhitespace();
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			if(ref == "auto" || ref == "none") {
				advance();
				return Width::create(true);
			}
		}
		return std::make_shared<Width>(parseLengthInternal());
	}

	Width PropertyParser::parseWidthInternal2()
	{
		skipWhitespace();
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			if(ref == "auto" || ref == "none") {
				advance();
				return Width(true);
			}
		}
		return Width(parseLengthInternal());
	}

	Length PropertyParser::parseLengthInternal(NumericParseOptions opts)
	{
		skipWhitespace();
		if(isToken(TokenId::DIMENSION) && (opts & LENGTH)) {
			const std::string units = (*it_)->getStringValue();
			xhtml::FixedPoint value = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
			advance();
			return Length(value, units);
		} else if(isToken(TokenId::PERCENT) && (opts & PERCENTAGE)) {
			xhtml::FixedPoint d = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
			advance();
			return Length(d, true);
		} else if(isToken(TokenId::NUMBER) && (opts & NUMBER)) {
			xhtml::FixedPoint d = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
			advance();
			skipWhitespace();
			return Length(d, false);
		}
		throw ParserError(formatter() << "Unrecognised value for property: "  << (*it_)->toString());
	}

	StylePtr PropertyParser::parseBorderWidthInternal()
	{
		skipWhitespace();
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			if(ref == "thin") {
				advance();
				return std::make_shared<Length>(border_width_thin, LengthUnits::PX);
			} else if(ref == "medium") {
				advance();
				return std::make_shared<Length>(border_width_medium, LengthUnits::PX);
			} else if(ref == "thick") {
				advance();
				return std::make_shared<Length>(border_width_thick, LengthUnits::PX);
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for width value, property: " << ref);
			}
		}	
		return std::make_shared<Length>(parseLengthInternal());
	}

	void PropertyParser::parseColor(const std::string& prefix, const std::string& suffix)
	{
		plist_.addProperty(prefix, parseColorInternal());
	}

	void PropertyParser::parseWidth(const std::string& prefix, const std::string& suffix)
	{
		plist_.addProperty(prefix, parseWidthInternal());
	}

	void PropertyParser::parseLength(const std::string& prefix, const std::string& suffix)
	{
		plist_.addProperty(prefix, Length::create(parseLengthInternal()));
	}

	void PropertyParser::parseWidthList(const std::string& prefix, const std::string& suffix)
	{
		StylePtr w1 = parseWidthInternal();
		skipWhitespace();
		if(isEndToken()) {
			plist_.addProperty(prefix + "-top", w1);
			plist_.addProperty(prefix + "-bottom", w1);
			plist_.addProperty(prefix + "-right", w1);
			plist_.addProperty(prefix + "-left", w1);
			return;
		}
		StylePtr w2 = parseWidthInternal();
		skipWhitespace();
		if(isEndToken()) {
			plist_.addProperty(prefix + "-top", w1);
			plist_.addProperty(prefix + "-bottom", w1);
			plist_.addProperty(prefix + "-right", w2);
			plist_.addProperty(prefix + "-left", w2);
			return;
		}
		StylePtr w3 = parseWidthInternal();
		skipWhitespace();
		if(isEndToken()) {
			plist_.addProperty(prefix + "-top", w1);
			plist_.addProperty(prefix + "-right", w2);
			plist_.addProperty(prefix + "-left", w2);
			plist_.addProperty(prefix + "-bottom", w3);
			return;
		}
		StylePtr w4 = parseWidthInternal();
		skipWhitespace();

		// four values, apply to individual elements.
		plist_.addProperty(prefix + "-top", w1);
		plist_.addProperty(prefix + "-right", w2);
		plist_.addProperty(prefix + "-bottom", w3);
		plist_.addProperty(prefix + "-left", w4);
	}

	void PropertyParser::parseLengthList(const std::string& prefix, const std::string& suffix)
	{
		StylePtr w1 = Length::create(parseLengthInternal());
		skipWhitespace();
		if(isEndToken()) {
			plist_.addProperty(prefix + "-top", w1);
			plist_.addProperty(prefix + "-bottom", w1);
			plist_.addProperty(prefix + "-right", w1);
			plist_.addProperty(prefix + "-left", w1);
			return;
		}
		StylePtr w2 = Length::create(parseLengthInternal());
		skipWhitespace();
		if(isEndToken()) {
			plist_.addProperty(prefix + "-top", w1);
			plist_.addProperty(prefix + "-bottom", w1);
			plist_.addProperty(prefix + "-right", w2);
			plist_.addProperty(prefix + "-left", w2);
			return;
		}
		StylePtr w3 = Length::create(parseLengthInternal());
		skipWhitespace();
		if(isEndToken()) {
			plist_.addProperty(prefix + "-top", w1);
			plist_.addProperty(prefix + "-right", w2);
			plist_.addProperty(prefix + "-left", w2);
			plist_.addProperty(prefix + "-bottom", w3);
			return;
		}
		StylePtr w4 = Length::create(parseLengthInternal());
		skipWhitespace();

		// four values, apply to individual elements.
		plist_.addProperty(prefix + "-top", w1);
		plist_.addProperty(prefix + "-right", w2);
		plist_.addProperty(prefix + "-bottom", w3);
		plist_.addProperty(prefix + "-left", w4);
	}

	void PropertyParser::parseBorderWidth(const std::string& prefix, const std::string& suffix)
	{
		plist_.addProperty(prefix, parseBorderWidthInternal());
	}

	void PropertyParser::parseBorderWidthList(const std::string& prefix, const std::string& suffix)
	{
		StylePtr w1 = parseBorderWidthInternal();
		skipWhitespace();
		if(isEndToken()) {
			plist_.addProperty(prefix + "-top-" + suffix, w1);
			plist_.addProperty(prefix + "-bottom-" + suffix, w1);
			plist_.addProperty(prefix + "-right-" + suffix, w1);
			plist_.addProperty(prefix + "-left-" + suffix, w1);
			return;
		}
		StylePtr w2 = parseBorderWidthInternal();
		skipWhitespace();
		if(isEndToken()) {
			plist_.addProperty(prefix + "-top-" + suffix, w1);
			plist_.addProperty(prefix + "-bottom-" + suffix, w1);
			plist_.addProperty(prefix + "-right-" + suffix, w2);
			plist_.addProperty(prefix + "-left-" + suffix, w2);
			return;
		}
		StylePtr w3 = parseBorderWidthInternal();
		skipWhitespace();
		if(isEndToken()) {
			plist_.addProperty(prefix + "-top-" + suffix, w1);
			plist_.addProperty(prefix + "-right-" + suffix, w2);
			plist_.addProperty(prefix + "-left-" + suffix, w2);
			plist_.addProperty(prefix + "-bottom-" + suffix, w3);
			return;
		}
		StylePtr w4 = parseBorderWidthInternal();
		skipWhitespace();

		// four values, apply to individual elements.
		plist_.addProperty(prefix + "-top-" + suffix, w1);
		plist_.addProperty(prefix + "-right-" + suffix, w2);
		plist_.addProperty(prefix + "-bottom-" + suffix, w3);
		plist_.addProperty(prefix + "-left-" + suffix, w4);	
	}

	StylePtr PropertyParser::parseBorderStyleInternal()
	{
		BorderStyle bs = BorderStyle::NONE;
		skipWhitespace();
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			skipWhitespace();
			if(ref == "none") {
				bs = BorderStyle::NONE;
			} else if(ref == "hidden") {
				bs = BorderStyle::HIDDEN;
			} else if(ref == "dotted") {
				bs = BorderStyle::DOTTED;
			} else if(ref == "dashed") {
				bs = BorderStyle::DASHED;
			} else if(ref == "solid") {
				bs = BorderStyle::SOLID;
			} else if(ref == "double") {
				bs = BorderStyle::DOUBLE;
			} else if(ref == "groove") {
				bs = BorderStyle::GROOVE;
			} else if(ref == "ridge") {
				bs = BorderStyle::RIDGE;
			} else if(ref == "inset") {
				bs = BorderStyle::INSET;
			} else if(ref == "outset") {
				bs = BorderStyle::OUTSET;
			} else {
				throw ParserError(formatter() << "Unwxpected identifier '" << ref << "' while parsing border style");
			}
			return Style::create<BorderStyle>(StyleId::BORDER_STYLE, bs);
		}
		throw ParserError(formatter() << "Unexpected IDENTIFIER, found: " << (*it_)->toString());
	}

	void PropertyParser::parseBorderStyle(const std::string& prefix, const std::string& suffix)
	{
		plist_.addProperty(prefix, parseBorderStyleInternal());
	}

	void PropertyParser::parseBorderStyleList(const std::string& prefix, const std::string& suffix)
	{
		StylePtr w1 = parseBorderStyleInternal();
		skipWhitespace();
		if(isEndToken()) {
			plist_.addProperty(prefix + "-top-" + suffix, w1);
			plist_.addProperty(prefix + "-bottom-" + suffix, w1);
			plist_.addProperty(prefix + "-right-" + suffix, w1);
			plist_.addProperty(prefix + "-left-" + suffix, w1);
			return;
		}
		StylePtr w2 = parseBorderStyleInternal();
		skipWhitespace();
		if(isEndToken()) {
			plist_.addProperty(prefix + "-top-" + suffix, w1);
			plist_.addProperty(prefix + "-bottom-" + suffix, w1);
			plist_.addProperty(prefix + "-right-" + suffix, w2);
			plist_.addProperty(prefix + "-left-" + suffix, w2);
			return;
		}
		StylePtr w3 = parseBorderStyleInternal();
		skipWhitespace();
		if(isEndToken()) {
			plist_.addProperty(prefix + "-top-" + suffix, w1);
			plist_.addProperty(prefix + "-right-" + suffix, w2);
			plist_.addProperty(prefix + "-left-" + suffix, w2);
			plist_.addProperty(prefix + "-bottom-" + suffix, w3);
			return;
		}
		StylePtr w4 = parseBorderStyleInternal();
		skipWhitespace();

		// four values, apply to individual elements.
		plist_.addProperty(prefix + "-top-" + suffix, w1);
		plist_.addProperty(prefix + "-right-" + suffix, w2);
		plist_.addProperty(prefix + "-bottom-" + suffix, w3);
		plist_.addProperty(prefix + "-left-" + suffix, w4);	
	}

	void PropertyParser::parseDisplay(const std::string& prefix, const std::string& suffix)
	{
		Display display = Display::INLINE;
		skipWhitespace();
		if(isToken(TokenId::IDENT)) {
			std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "inline") {
				display = Display::INLINE;
			} else if(ref == "none") {
				display = Display::NONE;
			} else if(ref == "block") {
				display = Display::BLOCK;
			} else if(ref == "list-item") {
				display = Display::LIST_ITEM;
			} else if(ref == "inline-block") {
				display = Display::INLINE_BLOCK;
			} else if(ref == "table") {
				display = Display::TABLE;
			} else if(ref == "inline-table") {
				display = Display::INLINE_TABLE;
			} else if(ref == "table-row-group") {
				display = Display::TABLE_ROW_GROUP;
			} else if(ref == "table-header-group") {
				display = Display::TABLE_HEADER_GROUP;
			} else if(ref == "table-footer-group") {
				display = Display::TABLE_FOOTER_GROUP;
			} else if(ref == "table-row") {
				display = Display::TABLE_ROW;
			} else if(ref == "table-column-group") {
				display = Display::TABLE_COLUMN_GROUP;
			} else if(ref == "table-column") {
				display = Display::TABLE_COLUMN;
			} else if(ref == "table-cell") {
				display = Display::TABLE_CELL;
			} else if(ref == "table-caption") {
				display = Display::TABLE_CAPTION;
			} else {
				throw ParserError(formatter() << "Unrecognised token for display property: " << ref);
			}
		}
		plist_.addProperty(prefix, Style::create<Display>(StyleId::DISPLAY, display));
	}

	void PropertyParser::parseWhitespace(const std::string& prefix, const std::string& suffix)
	{
		Whitespace ws = Whitespace::NORMAL;
		if(isToken(TokenId::IDENT)) {
			std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "normal") {
				ws = Whitespace::NORMAL;
			} else if(ref == "pre") {
				ws = Whitespace::PRE;
			} else if(ref == "nowrap") {
				ws = Whitespace::NOWRAP;
			} else if(ref == "pre-wrap") {
				ws = Whitespace::PRE_WRAP;
			} else if(ref == "pre-line") {
				ws = Whitespace::PRE_LINE;
			} else {
				throw ParserError(formatter() << "Unrecognised token for display property: " << ref);
			}			
		} else {
			throw ParserError(formatter() << "Expected identifier for property: " << prefix << " found " << Token::tokenIdToString((*it_)->id()));
		}
		plist_.addProperty(prefix, Style::create<Whitespace>(StyleId::WHITE_SPACE, ws));
	}

	void PropertyParser::parseFontFamily(const std::string& prefix, const std::string& suffix)
	{
		std::vector<std::string> font_list;
		parseCSVStringList(TokenId::DELIM, [&font_list](int n, const std::string& str) {
			font_list.emplace_back(str);
		});
		plist_.addProperty(prefix, FontFamily::create(font_list));
	}
	
	void PropertyParser::parseFontSize(const std::string& prefix, const std::string& suffix)
	{
		FontSize fs;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "xx-small") {
				fs.setFontSize(FontSizeAbsolute::XX_SMALL);
			} else if(ref == "x-small") {
				fs.setFontSize(FontSizeAbsolute::X_SMALL);
			} else if(ref == "small") {
				fs.setFontSize(FontSizeAbsolute::SMALL);
			} else if(ref == "medium") {
				fs.setFontSize(FontSizeAbsolute::MEDIUM);
			} else if(ref == "large") {
				fs.setFontSize(FontSizeAbsolute::LARGE);
			} else if(ref == "x-large") {
				fs.setFontSize(FontSizeAbsolute::X_LARGE);
			} else if(ref == "xx-large") {
				fs.setFontSize(FontSizeAbsolute::XX_LARGE);
			} else if(ref == "larger") {
				fs.setFontSize(FontSizeRelative::LARGER);
			} else if(ref == "smaller") {
				fs.setFontSize(FontSizeRelative::SMALLER);
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else if(isToken(TokenId::DIMENSION)) {
			const std::string units = (*it_)->getStringValue();
			xhtml::FixedPoint value = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
			advance();
			fs.setFontSize(Length(value, units));
		} else if(isToken(TokenId::PERCENT)) {
			xhtml::FixedPoint d = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
			advance();
			fs.setFontSize(Length(d, true));
		} else if(isToken(TokenId::NUMBER)) {
			xhtml::FixedPoint d = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
			advance();
			fs.setFontSize(Length(d, false));
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}		
		plist_.addProperty(prefix, FontSize::create(fs));
	}

	void PropertyParser::parseFontWeight(const std::string& prefix, const std::string& suffix)
	{
		FontWeight fw;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "lighter") {
				fw.setRelative(FontWeightRelative::LIGHTER);
			} else if(ref == "bolder") {
				fw.setRelative(FontWeightRelative::BOLDER);
			} else if(ref == "normal") {
				fw.setWeight(400);
			} else if(ref == "bold") {
				fw.setWeight(700);
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else if(isToken(TokenId::NUMBER)) {
			fw.setWeight(static_cast<int>((*it_)->getNumericValue()));
			advance();
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, FontWeight::create(fw));
	}

	void PropertyParser::parseSpacing(const std::string& prefix, const std::string& suffix)
	{
		Length spacing;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "normal") {
				// spacing defaults to 0
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else if(isToken(TokenId::DIMENSION)) {
			const std::string units = (*it_)->getStringValue();
			xhtml::FixedPoint value = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
			advance();
			spacing = Length(value, units);
		} else if(isToken(TokenId::NUMBER)) {
			xhtml::FixedPoint d = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
			advance();
			spacing = Length(d, false);
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Length::create(spacing));
	}

	void PropertyParser::parseTextAlign(const std::string& prefix, const std::string& suffix)
	{
		TextAlign ta = TextAlign::NORMAL;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "left") {
				ta = TextAlign::LEFT;
			} else if(ref == "right") {
				ta = TextAlign::RIGHT;
			} else if(ref == "center" || ref == "centre") {
				ta = TextAlign::CENTER;
			} else if(ref == "justify") {
				ta = TextAlign::JUSTIFY;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<TextAlign>(StyleId::TEXT_ALIGN, ta));
	}

	void PropertyParser::parseDirection(const std::string& prefix, const std::string& suffix)
	{
		Direction dir = Direction::LTR;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "ltr") {
				dir = Direction::LTR;
			} else if(ref == "rtl") {
				dir = Direction::RTL;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<Direction>(StyleId::DIRECTION, dir));
	}

	void PropertyParser::parseTextTransform(const std::string& prefix, const std::string& suffix)
	{
		TextTransform tt = TextTransform::NONE;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "capitalize") {
				tt = TextTransform::CAPITALIZE;
			} else if(ref == "uppercase") {
				tt = TextTransform::UPPERCASE;
			} else if(ref == "lowercase") {
				tt = TextTransform::LOWERCASE;
			} else if(ref == "none") {
				tt = TextTransform::NONE;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<TextTransform>(StyleId::TEXT_TRANSFORM, tt));
	}

	void PropertyParser::parseLineHeight(const std::string& prefix, const std::string& suffix)
	{
		Length lh(static_cast<xhtml::FixedPoint>(1.1f * fixed_point_scale), false);
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "normal") {
				// do nothing as already set in default
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			lh = parseLengthInternal(NUMERIC);
		}
		plist_.addProperty(prefix, Length::create(lh));
	}

	void PropertyParser::parseFontStyle(const std::string& prefix, const std::string& suffix)
	{
		FontStyle fs = FontStyle::NORMAL;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "italic") {
				fs = FontStyle::ITALIC;
			} else if(ref == "normal") {
				fs = FontStyle::NORMAL;
			} else if(ref == "oblique") {
				fs = FontStyle::OBLIQUE;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<FontStyle>(StyleId::FONT_STYLE, fs));
	}

	void PropertyParser::parseFontVariant(const std::string& prefix, const std::string& suffix)
	{
		FontVariant fv = FontVariant::NORMAL;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "normal") {
				fv = FontVariant::NORMAL;
			} else if(ref == "small-caps") {
				fv = FontVariant::SMALL_CAPS;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<FontVariant>(StyleId::FONT_VARIANT, fv));
	}

	void PropertyParser::parseOverflow(const std::string& prefix, const std::string& suffix)
	{
		Overflow of = Overflow::VISIBLE;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "visible") {
				of = Overflow::VISIBLE;
			} else if(ref == "hidden") {
				of = Overflow::HIDDEN;
			} else if(ref == "scroll") {
				of = Overflow::SCROLL;
			} else if(ref == "clip") {
				of = Overflow::CLIP;
			} else if(ref == "auto") {
				of = Overflow::AUTO;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<Overflow>(StyleId::CSS_OVERFLOW, of));
	}

	void PropertyParser::parsePosition(const std::string& prefix, const std::string& suffix)
	{
		Position p = Position::STATIC;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "static") {
				p = Position::STATIC;
			} else if(ref == "absolute") {
				p = Position::ABSOLUTE_POS;
			} else if(ref == "relative") {
				p = Position::RELATIVE_POS;
			} else if(ref == "fixed") {
				p = Position::FIXED;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<Position>(StyleId::POSITION, p));
	}

	void PropertyParser::parseFloat(const std::string& prefix, const std::string& suffix)
	{
		Float p = Float::NONE;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "none") {
				p = Float::NONE;
			} else if(ref == "left") {
				p = Float::LEFT;
			} else if(ref == "right") {
				p = Float::RIGHT;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<Float>(StyleId::FLOAT, p));
	}

	void PropertyParser::parseImageSource(const std::string& prefix, const std::string& suffix)
	{
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "none") {
				plist_.addProperty(prefix, std::make_shared<UriStyle>(true));
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else if(isToken(TokenId::URL)) {
			const std::string uri = (*it_)->getStringValue();
			advance();
			plist_.addProperty(prefix, UriStyle::create(uri));
			// XXX here would be a good place to place to have a background thread working
			// to look up the uri and download it. or look-up in cache, etc.
		} else if(isToken(TokenId::FUNCTION)) {
			const std::string ref = (*it_)->getStringValue();
			auto params = (*it_)->getParameters();
			if(ref == "linear-gradient") {
				plist_.addProperty(prefix, parseLinearGradient(params));
				advance();
			} else if(ref == "radial-gradient") {
				ASSERT_LOG(false, "XXX: write radial-gradient parser");
			} else if(ref == "repeating-linear-gradient") {
				ASSERT_LOG(false, "XXX: write repeating-linear-gradient parser");
			} else if(ref == "repeating-radial-gradient") {
				ASSERT_LOG(false, "XXX: write repeating-radial-gradient parser");
			} else {
				throw ParserError(formatter() << "Unrecognised function for image '" << prefix << "': "  << (*it_)->toString());
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
	}

	void PropertyParser::parseBackgroundRepeat(const std::string& prefix, const std::string& suffix)
	{
		BackgroundRepeat repeat = BackgroundRepeat::REPEAT;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "repeat") {
				repeat = BackgroundRepeat::REPEAT;
			} else if(ref == "repeat-x") {
				repeat = BackgroundRepeat::REPEAT_X;
			} else if(ref == "repeat-y") {
				repeat = BackgroundRepeat::REPEAT_Y;
			} else if(ref == "no-repeat") {
				repeat = BackgroundRepeat::NO_REPEAT;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<BackgroundRepeat>(StyleId::BACKGROUND_REPEAT, repeat));
	}

	void PropertyParser::parseBackgroundPosition(const std::string& prefix, const std::string& suffix)
	{
		/// need to check this works.
		// this is slightly complicated by the fact that "top center" and "center top" are valid.
		// as is "0% top" and "top 0%" whereas "50% 25%" is fixed for left then top.
		auto pos = BackgroundPosition::create();
		bool was_horiz_set = false;
		bool was_vert_set = false;
		int cnt = 2;
		std::vector<Length> holder;
		while(cnt-- > 0) {
			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				if(ref == "left") {
					pos->setLeft(Length(0, true));
					was_horiz_set = true; 
				} else if(ref == "top") {
					pos->setTop(Length(0, true));
					was_vert_set = true;
				} else if(ref == "right") {
					pos->setLeft(Length(100 * fixed_point_scale, true));
					was_horiz_set = true; 
				} else if(ref == "bottom") {
					pos->setTop(Length(100 * fixed_point_scale, true));
					was_vert_set = true;
				} else if(ref == "center") {
					holder.emplace_back(50 * fixed_point_scale, true);
				} else {
					throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
				}
			} else if(isToken(TokenId::DIMENSION)) {
				const std::string units = (*it_)->getStringValue();
				xhtml::FixedPoint value = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
				advance();
				holder.emplace_back(value, units);
			} else if(isToken(TokenId::PERCENT)) {
				xhtml::FixedPoint d = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
				advance();
				holder.emplace_back(d, true);
			} else if(cnt > 0) {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}
			
			skipWhitespace();
		}
		if(was_horiz_set && !was_vert_set) {
			if(holder.size() > 0) {
				// we set something so apply it.
				pos->setTop(holder.front());
			} else {
				// apply a default, center
				pos->setTop(Length(50, true));
			}
		}
		if(was_vert_set && !was_horiz_set) {
			if(holder.size() > 0) {
				// we set something so apply it.
				pos->setLeft(holder.front());
			} else {
				// apply a default, center
				pos->setLeft(Length(50, true));
			}
		}
		if(!was_horiz_set && !was_vert_set) {
			// assume left top ordering.
			if(holder.size() > 1) {
				pos->setLeft(holder[0]);
				pos->setTop(holder[1]);
			} else if(holder.size() > 0) {
				pos->setLeft(holder.front());
				pos->setTop(holder.front());
			} else {
				pos->setLeft(Length(0, true));
				pos->setTop(Length(0, true));
			}
		}
		plist_.addProperty(prefix, pos);
	}

	ListStyleType PropertyParser::parseListStyleTypeInt(const std::string& ref)
	{
		if(ref == "none") {
			return ListStyleType::NONE;
		} else if(ref == "disc") {
			return ListStyleType::DISC;
		} else if(ref == "circle") {
			return ListStyleType::CIRCLE;
		} else if(ref == "square") {
			return ListStyleType::SQUARE;
		} else if(ref == "decimal") {
			return ListStyleType::DECIMAL;
		} else if(ref == "decimal-leading-zero") {
			return ListStyleType::DECIMAL_LEADING_ZERO;
		} else if(ref == "lower-roman") {
			return ListStyleType::LOWER_ROMAN;
		} else if(ref == "upper-roman") {
			return ListStyleType::UPPER_ROMAN;
		} else if(ref == "lower-greek") {
			return ListStyleType::LOWER_GREEK;
		} else if(ref == "lower-latin") {
			return ListStyleType::LOWER_LATIN;
		} else if(ref == "upper-latin") {
			return ListStyleType::UPPER_LATIN;
		} else if(ref == "armenian") {
			return ListStyleType::ARMENIAN;
		} else if(ref == "georgian") {
			return ListStyleType::GEORGIAN;
		} else if(ref == "lower-alpha") {
			return ListStyleType::LOWER_ALPHA;
		} else if(ref == "upper-alpha") {
			return ListStyleType::UPPER_ALPHA;
		} else {
			throw ParserError(formatter() << "Unrecognised value for list style: " << ref);
		}
		return ListStyleType::NONE;
	}

	void PropertyParser::parseListStyleType(const std::string& prefix, const std::string& suffix)
	{
		ListStyleType lst = ListStyleType::DISC;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			try {
				lst = parseListStyleTypeInt(ref);
			} catch(ParserError&) {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<ListStyleType>(StyleId::LIST_STYLE_TYPE, lst));
	}

	void PropertyParser::parseBorder(const std::string& prefix, const std::string& suffix)
	{
		auto len = Length::create(border_width_medium, LengthUnits::PX);
		BorderStyle bs = BorderStyle::NONE;
		auto color = CssColor::create();

		bool done = false;
		while(!done) {
			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				if(ref == "invert") {
					// not supporting invert color at the moment so we just 
					// set to current color.
					color->setParam(CssColorParam::CURRENT);
				} else if(ref == "thin") {
					len = Length::create(border_width_thin, LengthUnits::PX);
				} else if(ref == "medium") {
					len = Length::create(border_width_medium, LengthUnits::PX);
				} else if(ref == "thick") {
					len = Length::create(border_width_thick, LengthUnits::PX);
				} else if(ref == "none") {
					bs = BorderStyle::NONE;
				} else if(ref == "hidden") {
					bs = BorderStyle::HIDDEN;
				} else if(ref == "dotted") {
					bs = BorderStyle::DOTTED;
				} else if(ref == "dashed") {
					bs = BorderStyle::DASHED;
				} else if(ref == "solid") {
					bs = BorderStyle::SOLID;
				} else if(ref == "double") {
					bs = BorderStyle::DOUBLE;
				} else if(ref == "groove") {
					bs = BorderStyle::GROOVE;
				} else if(ref == "ridge") {
					bs = BorderStyle::RIDGE;
				} else if(ref == "inset") {
					bs = BorderStyle::INSET;
				} else if(ref == "outset") {
					bs = BorderStyle::OUTSET;
				} else {
					color->setColor(KRE::Color(ref));
				}
			} else if(isToken(TokenId::DIMENSION)) {
				const std::string units = (*it_)->getStringValue();
				xhtml::FixedPoint value = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
				advance();
				len = Length::create(value, units);
			} else {
				parseColor2(color);
			}

			skipWhitespace();
			if(isEndToken()) {
				done = true;
			}
		}
		StylePtr bs_style = Style::create<BorderStyle>(StyleId::BORDER_STYLE, bs);
		if(prefix == "border") {
			plist_.addProperty("border-top-width", len);
			plist_.addProperty("border-left-width", len);
			plist_.addProperty("border-bottom-width", len);
			plist_.addProperty("border-right-width", len);

			plist_.addProperty("border-top-style", bs_style);
			plist_.addProperty("border-left-style", bs_style);
			plist_.addProperty("border-bottom-style", bs_style);
			plist_.addProperty("border-right-style", bs_style);

			plist_.addProperty("border-top-color", color);
			plist_.addProperty("border-left-color", color);
			plist_.addProperty("border-bottom-color", color);
			plist_.addProperty("border-right-color", color);

			// Reset all the border-image-* properties here.
			plist_.addProperty("border-image-source", UriStyle::create(true));
			plist_.addProperty("border-image-repeat", BorderImageRepeat::create());
			plist_.addProperty("border-image-width", WidthList::create());
			plist_.addProperty("border-image-outset", WidthList::create());
			plist_.addProperty("border-image-slice", BorderImageSlice::create());
		} else if(prefix == "outline") {
			plist_.addProperty("outline-width", len);
			plist_.addProperty("outline-style", bs_style);
			plist_.addProperty("outline-color", color);
		}
	}

	void PropertyParser::parseBackgroundAttachment(const std::string& prefix, const std::string& suffix)
	{
		BackgroundAttachment p = BackgroundAttachment::SCROLL;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "scroll") {
				p = BackgroundAttachment::SCROLL;
			} else if(ref == "fixed") {
				p = BackgroundAttachment::FIXED;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<BackgroundAttachment>(StyleId::BACKGROUND_ATTACHMENT, p));
	}

	void PropertyParser::parseClear(const std::string& prefix, const std::string& suffix)
	{
		Clear p = Clear::NONE;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "none") {
				p = Clear::NONE;
			} else if(ref == "left") {
				p = Clear::LEFT;
			} else if(ref == "right") {
				p = Clear::RIGHT;
			} else if(ref == "both") {
				p = Clear::BOTH;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<Clear>(StyleId::ClEAR, p));
	}

	void PropertyParser::parseClip(const std::string& prefix, const std::string& suffix)
	{
		auto clip = Clip::create();
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "auto") {
				// do nothing is the default
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else if(isToken(TokenId::FUNCTION)) {
			const std::string& ref = (*it_)->getStringValue();
			if(ref == "rect") {
				std::vector<xhtml::FixedPoint> values;
				parseCSVNumberListFromIt((*it_)->getParameters().cbegin(), (*it_)->getParameters().end(), 
					[&values](int n, float value, bool is_percent) {
						values.emplace_back(static_cast<int>(value * fixed_point_scale));
					}
				);
				if(values.size() < 4) {
					throw ParserError(formatter() << "Not enough values for 'rect' in property '" << prefix << "': "  << (*it_)->toString());
				}
				advance();
				clip->setRect(values[0], values[1], values[2], values[3]);
			} else {
				throw ParserError(formatter() << "Unrecognised function for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, clip);
	}

	void PropertyParser::parseCounter(const std::string& prefix, const std::string& suffix)
	{
		std::vector<std::pair<std::string,int>> counters;
		bool done = false;
		while(!done) {
			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				if(ref == "none") {
					plist_.addProperty(prefix, Counter::create());
					return;
				} else {
					counters.emplace_back(std::make_pair(ref, 1));
				}
			} else if(isToken(TokenId::NUMBER)) {
				int number = static_cast<int>((*it_)->getNumericValue());
				advance();
				if(counters.empty()) {
					throw ParserError(formatter() << "Found a number and no associated identifier value for property '" << prefix << "': "  << (*it_)->toString());
				}
				counters.back().second = number;
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}
			skipWhitespace();
			if(isEndToken()) {
				done = true;
			}
		}
		plist_.addProperty(prefix, Counter::create(counters));
	}

	void PropertyParser::parseListStylePosition(const std::string& prefix, const std::string& suffix)
	{
		ListStylePosition lsp = ListStylePosition::OUTSIDE;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "inside") {
				lsp = ListStylePosition::INSIDE;
			} else if(ref == "outside") {
				lsp = ListStylePosition::OUTSIDE;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<ListStylePosition>(StyleId::LIST_STYLE_POSITION, lsp));
	}

	void PropertyParser::parseUnicodeBidi(const std::string& prefix, const std::string& suffix)
	{
		UnicodeBidi bidi = UnicodeBidi::NORMAL;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "normal") {
				bidi = UnicodeBidi::NORMAL;
			} else if(ref == "embed") {
				bidi = UnicodeBidi::EMBED;
			} else if(ref == "bidi-override") {
				bidi = UnicodeBidi::BIDI_OVERRIDE;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<UnicodeBidi>(StyleId::UNICODE_BIDI, bidi));
	}

	void PropertyParser::parseVerticalAlign(const std::string& prefix, const std::string& suffix)
	{
		auto va = VerticalAlign::create();
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "baseline") {
				va->setAlign(CssVerticalAlign::BASELINE);
			} else if(ref == "sub") {
				va->setAlign(CssVerticalAlign::SUB);
			} else if(ref == "super") {
				va->setAlign(CssVerticalAlign::SUPER);
			} else if(ref == "top") {
				va->setAlign(CssVerticalAlign::TOP);
			} else if(ref == "text-top") {
				va->setAlign(CssVerticalAlign::TEXT_TOP);
			} else if(ref == "bottom") {
				va->setAlign(CssVerticalAlign::BOTTOM);
			} else if(ref == "text-bottom") {
				va->setAlign(CssVerticalAlign::TEXT_BOTTOM);
			} else if(ref == "middle") {
				va->setAlign(CssVerticalAlign::MIDDLE);
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else if(isToken(TokenId::PERCENT)) {
			xhtml::FixedPoint d = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
			advance();
			va->setLength(Length(d, true));
		} else if(isToken(TokenId::DIMENSION)) {
			const std::string units = (*it_)->getStringValue();
			xhtml::FixedPoint value = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
			advance();
			va->setLength(Length(value, units));
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, va);
	}

	void PropertyParser::parseVisibility(const std::string& prefix, const std::string& suffix)
	{
		Visibility vis = Visibility::VISIBLE;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "visible") {
				vis = Visibility::VISIBLE;
			} else if(ref == "hidden") {
				vis = Visibility::HIDDEN;
			} else if(ref == "collapse") {
				vis = Visibility::COLLAPSE;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<Visibility>(StyleId::VISIBILITY, vis));
	}

	void PropertyParser::parseZindex(const std::string& prefix, const std::string& suffix)
	{
		auto zindex = Zindex::create();
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "auto") {
				// do nothing is default
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else if(isToken(TokenId::NUMBER)) {
			zindex->setIndex(static_cast<int>((*it_)->getNumericValue()));
			advance();
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, zindex);
	}

	void PropertyParser::parseQuotes(const std::string& prefix, const std::string& suffix)
	{
		std::vector<quote_pair> quotes;
		bool done = false;
		while(!done) {
			std::string first_quote;
			std::string second_quote;
			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				if(ref == "none") {
					plist_.addProperty(prefix, Quotes::create());
					return;
				} else {
					first_quote = ref;
				}
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}
			skipWhitespace();
			if(isToken(TokenId::IDENT)) {
				second_quote = (*it_)->getStringValue();
				advance();
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}
			quotes.emplace_back(first_quote, second_quote);
			
			skipWhitespace();
			if(isEndToken()) {
				done = true;
			}
		}
		plist_.addProperty(prefix, Quotes::create(quotes));
	}

	void PropertyParser::parseTextDecoration(const std::string& prefix, const std::string& suffix)
	{
		TextDecoration td = TextDecoration::NONE;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "none") {
				td = TextDecoration::NONE;
			} else if(ref == "underline") {
				td = TextDecoration::UNDERLINE;
			} else if(ref == "overline") {
				td = TextDecoration::OVERLINE;
			} else if(ref == "line-through") {
				td = TextDecoration::LINE_THROUGH;
			} else if(ref == "blink") {
				// blink is dead to us
				td = TextDecoration::NONE;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<TextDecoration>(StyleId::TEXT_DECORATION, td));
	}

	void PropertyParser::parseCursor(const std::string& prefix, const std::string& suffix)
	{
		auto cursor = Cursor::create();
		std::vector<std::shared_ptr<ImageSource>> uris;
		bool done = false;
		while(!done) {
			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				if(ref == "auto") {
					cursor->setCursor(CssCursor::AUTO);
				} else if(ref == "crosshair") {
					cursor->setCursor(CssCursor::CROSSHAIR);
				} else if(ref == "default") {
					cursor->setCursor(CssCursor::DEFAULT);
				} else if(ref == "pointer") {
					cursor->setCursor(CssCursor::POINTER);
				} else if(ref == "move") {
					cursor->setCursor(CssCursor::MOVE);
				} else if(ref == "e-resize") {
					cursor->setCursor(CssCursor::E_RESIZE);
				} else if(ref == "ne-resize") {
					cursor->setCursor(CssCursor::NE_RESIZE);
				} else if(ref == "nw-resize") {
					cursor->setCursor(CssCursor::NW_RESIZE);
				} else if(ref == "n-resize") {
					cursor->setCursor(CssCursor::N_RESIZE);
				} else if(ref == "se-resize") {
					cursor->setCursor(CssCursor::SE_RESIZE);
				} else if(ref == "sw-resize") {
					cursor->setCursor(CssCursor::SW_RESIZE);
				} else if(ref == "s-resize") {
					cursor->setCursor(CssCursor::S_RESIZE);
				} else if(ref == "w-resize") {
					cursor->setCursor(CssCursor::W_RESIZE);
				} else if(ref == "text") {
					cursor->setCursor(CssCursor::TEXT);
				} else if(ref == "wait") {
					cursor->setCursor(CssCursor::WAIT);
				} else if(ref == "help") {
					cursor->setCursor(CssCursor::HELP);
				} else if(ref == "progress") {
					cursor->setCursor(CssCursor::PROGRESS);
				} else {
					throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
				}
			} else if(isToken(TokenId::URL)) {
				const std::string uri = (*it_)->getStringValue();
				advance();
				uris.emplace_back(UriStyle::create(uri));
				// XXX here would be a good place to place to have a background thread working
				// to look up the uri and download it. or look-up in cache, etc.
			// XXX add gradient parser here
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}
			skipWhitespace();
			if(isEndToken()) {
				done = true;
			}
		}
		cursor->setURI(uris);
		plist_.addProperty(prefix, cursor);
	}

	void PropertyParser::parseContent(const std::string& prefix, const std::string& suffix)
	{
		std::vector<ContentType> ct;
		bool done = false;
		while(!done) {
			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				if(ref == "none") {
					plist_.addProperty(prefix, Content::create());
					return;
				} else if(ref == "normal") {
					plist_.addProperty(prefix, Content::create());
					return;
				} else if(ref == "open-quote") {
					ct.emplace_back(CssContentType::OPEN_QUOTE);
				} else if(ref == "close-quote") {
					ct.emplace_back(CssContentType::CLOSE_QUOTE);
				} else if(ref == "no-open-quote") {
					ct.emplace_back(CssContentType::NO_OPEN_QUOTE);
				} else if(ref == "no-close-quote") {
					ct.emplace_back(CssContentType::NO_CLOSE_QUOTE);
				} else {
					ct.emplace_back(CssContentType::STRING, ref);
				}
			} else if(isToken(TokenId::FUNCTION)) {
				const std::string& ref = (*it_)->getStringValue();
				auto& params = (*it_)->getParameters();
				if(ref == "attr") {
					if(params.empty()) {
						throw ParserError(formatter() << "No attr parameter for property '" << prefix << "': "  << (*it_)->toString());
					}
					ct.emplace_back(CssContentType::ATTRIBUTE, params.front()->getStringValue());
				} else if(ref == "counter") {
					if(params.empty()) {
						throw ParserError(formatter() << "No counter parameter for property '" << prefix << "': "  << (*it_)->toString());
					}
					if(params.size() == 1) {
						ct.emplace_back(ListStyleType::DECIMAL, params.front()->getStringValue());
					} else {						
						try {
							ListStyleType lst = parseListStyleTypeInt(params[1]->getStringValue());
							ct.emplace_back(lst, params.front()->getStringValue());
						} catch(ParserError& e) {
							throw ParserError(formatter() << e.what() << " while parsing counter function in content property.");
						}
					}
				} else if(ref == "counters") {
					if(params.size() < 2) {
						throw ParserError(formatter() << "Not enough parameters for property '" << prefix << "': "  << (*it_)->toString());
					}
					if(params.size() == 2) {
						ct.emplace_back(ListStyleType::DECIMAL, params[0]->getStringValue(), params[1]->getStringValue());
					} else {						
						try {
							ListStyleType lst = parseListStyleTypeInt(params[2]->getStringValue());
							ct.emplace_back(lst, params[0]->getStringValue(), params[1]->getStringValue());
						} catch(ParserError& e) {
							throw ParserError(formatter() << e.what() << " while parsing counter function in content property.");
						}
					}
				}
			} else if(isToken(TokenId::URL)) {
				const std::string uri = (*it_)->getStringValue();
				advance();
				ct.emplace_back(CssContentType::URI, uri);
				// XXX here would be a good place to place to have a background thread working
				// to look up the uri and download it. or look-up in cache, etc.
			} else if(isToken(TokenId::STRING)) {
				const std::string str = (*it_)->getStringValue();
				advance();
				ct.emplace_back(CssContentType::STRING, str);
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}

			skipWhitespace();
			if(isEndToken()) {
				done = true;
			}
		}
		plist_.addProperty(prefix, Content::create(ct));
	}

	void PropertyParser::parseBackground(const std::string& prefix, const std::string& suffix)
	{
		BackgroundAttachment ba = BackgroundAttachment::SCROLL;
		auto bc = CssColor::create(CssColorParam::CSS_TRANSPARENT);
		auto br = BackgroundRepeat::REPEAT;
		auto bp = BackgroundPosition::create();
		ImageSourcePtr bi = nullptr;

		bool was_horiz_set = false;
		bool was_vert_set = false;
		int cnt = 2;
		std::vector<Length> holder;

		bool done = false;
		while(!done) {
			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				if(ref == "transparent") {
					bc->setParam(CssColorParam::CSS_TRANSPARENT);
				} else if(ref == "scroll") {
					ba = BackgroundAttachment::SCROLL;
				} else if(ref == "fixed") {
					ba = BackgroundAttachment::FIXED;
				} else if(ref == "left") {
					bp->setLeft(Length(0, true));
					was_horiz_set = true; 
				} else if(ref == "top") {
					bp->setTop(Length(0, true));
					was_vert_set = true;
				} else if(ref == "right") {
					bp->setLeft(Length(100 * fixed_point_scale, true));
					was_horiz_set = true; 
				} else if(ref == "bottom") {
					bp->setTop(Length(100 * fixed_point_scale, true));
					was_vert_set = true;
				} else if(ref == "center") {
					holder.emplace_back(50 * fixed_point_scale, true);
				} else if(ref == "repeat") {
					br = BackgroundRepeat::REPEAT;
				} else if(ref == "repeat-x") {
					br = BackgroundRepeat::REPEAT_X;
				} else if(ref == "repeat-y") {
					br = BackgroundRepeat::REPEAT_Y;
				} else if(ref == "no-repeat") {
					br = BackgroundRepeat::NO_REPEAT;
				} else {
					bc->setColor(KRE::Color(ref));
				}
			} else if(isToken(TokenId::DIMENSION)) {
				const std::string units = (*it_)->getStringValue();
				xhtml::FixedPoint value = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
				advance();
				holder.emplace_back(value, units);
			} else if(isToken(TokenId::PERCENT)) {
				xhtml::FixedPoint d = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
				advance();
				holder.emplace_back(d, true);
			} else if(isToken(TokenId::URL)) {
				const std::string uri = (*it_)->getStringValue();
				advance();
				bi = UriStyle::create(uri);
			} else if(isToken(TokenId::FUNCTION)) {
				const std::string& ref = (*it_)->getStringValue();
				auto& params = (*it_)->getParameters();
				if(ref == "linear-gradient") {
					bi = parseLinearGradient(params);
					advance();
				} else if(ref == "url") {
					if(params.empty()) {
						throw ParserError(formatter() << "expected at least one parameter to url '" << prefix << "': "  << (*it_)->toString());
					}
					bi = UriStyle::create(params.front()->getStringValue());
					advance();
				} else {
					parseColor2(bc);
				}
			} else {
				parseColor2(bc);
			}

			if(holder.size() > 2) {
				throw ParserError(formatter() << "Too many values where added for background position '");
			}

			skipWhitespace();
			if(isEndToken()) {
				done = true;
			}
		}

		if(was_horiz_set && !was_vert_set) {
			if(holder.size() > 0) {
				// we set something so apply it.
				bp->setTop(holder.front());
			} else {
				// apply a default, center
				bp->setTop(Length(50, true));
			}
		}
		if(was_vert_set && !was_horiz_set) {
			if(holder.size() > 0) {
				// we set something so apply it.
				bp->setLeft(holder.front());
			} else {
				// apply a default, center
				bp->setLeft(Length(50, true));
			}
		}
		if(!was_horiz_set && !was_vert_set) {
			// assume left top ordering.
			if(holder.size() > 1) {
				bp->setLeft(holder[0]);
				bp->setTop(holder[1]);
			} else if(holder.size() > 0) {
				bp->setLeft(holder.front());
				bp->setTop(holder.front());
			} else {
				bp->setLeft(Length(0, true));
				bp->setTop(Length(0, true));
			}
		}

		plist_.addProperty(prefix + "-attachment", Style::create<BackgroundAttachment>(StyleId::BACKGROUND_ATTACHMENT, ba));
		plist_.addProperty(prefix + "-color", bc);
		plist_.addProperty(prefix + "-position", bp);
		plist_.addProperty(prefix + "-repeat", Style::create(StyleId::BACKGROUND_REPEAT, br));
		plist_.addProperty(prefix + "-image", bi);
	}

	void PropertyParser::parseListStyle(const std::string& prefix, const std::string& suffix)
	{
		ListStyleType lst = ListStyleType::DISC;
		ListStylePosition pos = ListStylePosition::OUTSIDE;
		ImageSourcePtr img = nullptr;

		int none_counter = 0;
		bool set_lst = false;

		bool done = false;
		while(!done) {
			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				if(ref == "none") {
					++none_counter;
				} else if(ref == "inside") {
					pos = ListStylePosition::INSIDE;
				} else if(ref == "outside") {
					pos = ListStylePosition::OUTSIDE;
				} else {
					try {
						lst = parseListStyleTypeInt(ref);
						set_lst = true;
					} catch(ParserError&) {
						throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
					}
				}
			} else if(isToken(TokenId::URL)) {
				const std::string uri = (*it_)->getStringValue();
				advance();
				img = UriStyle::create(uri);
				// XXX here would be a good place to place to have a background thread working
				// to look up the uri and download it. or look-up in cache, etc.
			} else if(isToken(TokenId::FUNCTION)) {
				const std::string ref = (*it_)->getStringValue();
				auto params = (*it_)->getParameters();
				if(ref == "linear-gradient") {
					img = parseLinearGradient(params);
					advance();
				} else if(ref == "radial-gradient") {
					ASSERT_LOG(false, "XXX: write radial-gradient parser");
				} else if(ref == "repeating-linear-gradient") {
					ASSERT_LOG(false, "XXX: write repeating-linear-gradient parser");
				} else if(ref == "repeating-radial-gradient") {
					ASSERT_LOG(false, "XXX: write repeating-radial-gradient parser");
				}				
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}

			skipWhitespace();
			if(isEndToken()) {
				done = true;
			}
		}

		if(none_counter > 0) {
			if(!set_lst) {
				lst = ListStyleType::NONE;
			}
			// we don't need to check uri because it defaults to none.
		}

		plist_.addProperty(prefix + "-type", Style::create<ListStyleType>(StyleId::LIST_STYLE_TYPE, lst));
		plist_.addProperty(prefix + "-position", Style::create<ListStylePosition>(StyleId::LIST_STYLE_POSITION, pos));
		plist_.addProperty(prefix + "-image", img);
	}

	void PropertyParser::parseBoxShadow(const std::string& prefix, const std::string& suffix)
	{
		std::vector<BoxShadow> bs;

		bool done = false;
		while(!done) {
			bool inset = false;
			Length xo;
			Length yo;
			Length br;
			Length sr;
			auto color = CssColor::create(CssColorParam::CURRENT);

			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				if(ref == "none") {
					plist_.addProperty(prefix, BoxShadowStyle::create());
				} else if(ref == "inset") {
					inset = true;
				}
			}

			skipWhitespace();
			if(isToken(TokenId::DIMENSION)) {
				const std::string units = (*it_)->getStringValue();
				xhtml::FixedPoint value = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
				advance();
				xo = Length(value, units);
			} else {
				throw ParserError(formatter() << "Expected dimension of x-offset while parsing: " << prefix << ": "  << (*it_)->toString());
			}

			skipWhitespace();
			if(isToken(TokenId::DIMENSION)) {
				const std::string units = (*it_)->getStringValue();
				xhtml::FixedPoint value = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
				advance();
				yo = Length(value, units);
			} else {
				throw ParserError(formatter() << "Expected dimension of y-offset while parsing: " << prefix << ": "  << (*it_)->toString());
			}

			skipWhitespace();
			if(isToken(TokenId::DIMENSION)) {
				const std::string units = (*it_)->getStringValue();
				xhtml::FixedPoint value = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
				advance();
				br = Length(value, units);
			}

			skipWhitespace();
			if(isToken(TokenId::DIMENSION)) {
				const std::string units = (*it_)->getStringValue();
				xhtml::FixedPoint value = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
				advance();
				sr = Length(value, units);
			}

			skipWhitespace();
			try {
				color = parseColorInternal();
			} catch(ParserError&) {
			}

			skipWhitespace();
			if(isEndToken()) {
				done = true;
			} else {
				if(!isToken(TokenId::COMMA)) {
					throw ParserError(formatter() << "Expected comma or end of list" << ": "  << (*it_)->toString());
				}
				advance();
				skipWhitespace();
			}
			bs.emplace_back(inset, xo, yo, br, sr, *color);
		}
		plist_.addProperty(prefix, BoxShadowStyle::create(bs));
	}

	CssBorderImageRepeat PropertyParser::parseBorderImageRepeatInteral(const std::string& ref)
	{
		if(ref == "repeat") {
			return CssBorderImageRepeat::REPEAT;
		} else if(ref == "stretch") {
			return CssBorderImageRepeat::STRETCH;
		} else if(ref == "round") {
			return CssBorderImageRepeat::ROUND;
		} else if(ref == "space") {
			return CssBorderImageRepeat::SPACE;
		} else {
			throw ParserError(formatter() << "Unrecognised identifier for 'border-image-repeat' property: " << ref);
		}
	}

	void PropertyParser::parseBorderImageRepeat(const std::string& prefix, const std::string& suffix)
	{
		bool done = false;
		std::vector<CssBorderImageRepeat> repeat;
		while(!done) {
			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				repeat.emplace_back(parseBorderImageRepeatInteral(ref));
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}
			skipWhitespace();
			if(isEndToken()) {
				done = true;
			}
		}
		if(repeat.size() == 0) {
			repeat.emplace_back(CssBorderImageRepeat::STRETCH);
			repeat.emplace_back(CssBorderImageRepeat::STRETCH);
		}
		if(repeat.size() == 1) {
			repeat.push_back(repeat[0]);
		}
		plist_.addProperty(prefix, BorderImageRepeat::create(repeat[0], repeat[1]));
	}

	void PropertyParser::parseWidthList2(const std::string& prefix, const std::string& suffix)
	{
		std::vector<Width> widths;
		bool done = false;
		while(!done) {
			widths.emplace_back(parseWidthInternal2());
			skipWhitespace();
			if(isEndToken()) {
				done = true;
			}
		}
		plist_.addProperty(prefix, WidthList::create(widths));
	}

	void PropertyParser::parseBorderImageSlice(const std::string& prefix, const std::string& suffix)
	{
		std::vector<Width> widths;
		bool fill = false;
		bool done = false;
		while(!done) {
			if(isToken(TokenId::IDENT) && (*it_)->getStringValue() == "fill") {
				fill = true;
			} else {
				widths.emplace_back(parseWidthInternal2());
			}
			skipWhitespace();
			if(isEndToken()) {
				done = true;
			}
		}
		plist_.addProperty(prefix, BorderImageSlice::create(widths, fill));
	}

	void PropertyParser::parseBorderImage(const std::string& prefix, const std::string& suffix)
	{
		bool fill = false;;
		std::vector<Width> slices;
		std::vector<CssBorderImageRepeat> repeat;
		std::vector<Width> outset;
		std::vector<Width> widths;
		ImageSourcePtr img = nullptr;

		// parse source first
		if(isToken(TokenId::URL)) {
			std::string uri = (*it_)->getStringValue();
			img = UriStyle::create(uri);
			advance();
			skipWhitespace();
		} else if(isToken(TokenId::FUNCTION)) {
			const std::string ref = (*it_)->getStringValue();
			auto params = (*it_)->getParameters();
			if(ref == "linear-gradient") {
				img = parseLinearGradient(params);
			} else if(ref == "radial-gradient") {
				ASSERT_LOG(false, "XXX: write radial-gradient parser");
			} else if(ref == "repeating-linear-gradient") {
				ASSERT_LOG(false, "XXX: write repeating-linear-gradient parser");
			} else if(ref == "repeating-radial-gradient") {
				ASSERT_LOG(false, "XXX: write repeating-radial-gradient parser");
			}			
		} else {
			throw ParserError(formatter() << "expected uri, found: " << (*it_)->toString());
		}

		// next parse slice.
		bool done = false;
		while(!done) {
			slices.emplace_back(parseWidthInternal2());
			skipWhitespace();
			if(isEndToken() || isTokenDelimiter("/") || isToken(TokenId::IDENT)) {
				done = true;
			}
		}
		if(isToken(TokenId::IDENT) && (*it_)->getStringValue() == "fill") {
			advance();
			fill = true;
		}

		skipWhitespace();
		if(isTokenDelimiter("/")) {
			advance();
			skipWhitespace();
			// optional width
			done = false;
			while(!done) {
				if(isTokenDelimiter("/")) {
					done = true;
					break;
				}
				widths.emplace_back(parseWidthInternal2());
				skipWhitespace();
				if(isEndToken() || isTokenDelimiter("/") || isToken(TokenId::IDENT)) {
					done = true;
				}
			}

			if(isTokenDelimiter("/")) {
				advance();
				// non-optional at this point outset
				done = false;
				while(!done) {
					outset.emplace_back(parseWidthInternal2());
					skipWhitespace();
					if(isEndToken() || isToken(TokenId::IDENT)) {
						done = true;
					}
				}
			}
		}

		skipWhitespace();
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			repeat.emplace_back(parseBorderImageRepeatInteral(ref));

			skipWhitespace();
			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				repeat.emplace_back(parseBorderImageRepeatInteral(ref));
			}
		}
		skipWhitespace();

		if(repeat.size() == 0) {
			repeat.emplace_back(CssBorderImageRepeat::STRETCH);
			repeat.emplace_back(CssBorderImageRepeat::STRETCH);
		}
		if(repeat.size() == 1) {
			repeat.push_back(repeat[0]);
		}

		plist_.addProperty("border-image-source", img);
		plist_.addProperty("border-image-repeat", BorderImageRepeat::create(repeat[0], repeat[1]));
		if(widths.empty()) {
			plist_.addProperty("border-image-width", WidthList::create(1.0f));
		} else {
			plist_.addProperty("border-image-width", WidthList::create(widths));
		}			
		if(outset.empty()) {
			plist_.addProperty("border-image-outset", WidthList::create(0.0f));
		} else {
			plist_.addProperty("border-image-outset", WidthList::create(outset));
		}
		plist_.addProperty("border-image-slice", BorderImageSlice::create(slices, fill));
	}

	void PropertyParser::parseSingleBorderRadius(const std::string& prefix, const std::string& suffix)
	{
		// Parse one or two border widths, which can be length or percentages.
		std::vector<Length> lengths;
		bool done = false;
		while(!done) {
			lengths.emplace_back(parseLengthInternal(static_cast<NumericParseOptions>(NumericParseOptions::LENGTH | NumericParseOptions::PERCENTAGE)));
			if(isEndToken()) {
				done = true;
			}
		}
		if(lengths.empty()) {
			throw ParserError(formatter() << "No lengths/percentages supplied for " << prefix);
		}
		if(lengths.size() == 1) {
			lengths.push_back(lengths[0]);
		}
		plist_.addProperty(prefix, BorderRadius::create(lengths[0], lengths[1]));
	}

	void PropertyParser::parseBorderRadius(const std::string& prefix, const std::string& suffix)
	{
		std::vector<Length> lengths1;
		std::vector<Length> lengths2;
		bool done = false;
		bool extended_syntax = false;
		while(!done) {
			skipWhitespace();
			if(isEndToken()) {
				done = true;
			} else if(isTokenDelimiter("/")) {
				advance();
				skipWhitespace();
				done = true;
				extended_syntax = true;
			} else {
				lengths1.emplace_back(parseLengthInternal(static_cast<NumericParseOptions>(NumericParseOptions::LENGTH | NumericParseOptions::PERCENTAGE)));
			}
		}
		done = false;
		while(!done) {
			if(isEndToken()) {
				done = true;
			} else {
				lengths2.emplace_back(parseLengthInternal(static_cast<NumericParseOptions>(NumericParseOptions::LENGTH | NumericParseOptions::PERCENTAGE)));
			}
			skipWhitespace();
		}
		if(lengths1.empty() || (extended_syntax && lengths2.empty())) {
			throw ParserError(formatter() << "No lengths/percentages supplied for " << prefix);
		}
		std::vector<Length> horiz_lengths;
		std::vector<Length> vert_lengths;
		switch(lengths1.size()) {
			case 0: /* Invalid case. Will have already thrown ParseError by this point. */ break;
			case 1:
				// Radius for all for sides.
				horiz_lengths.emplace_back(lengths1[0]);
				horiz_lengths.emplace_back(lengths1[0]);
				horiz_lengths.emplace_back(lengths1[0]);
				horiz_lengths.emplace_back(lengths1[0]);
				if(!extended_syntax) {
					vert_lengths.emplace_back(lengths1[0]);
					vert_lengths.emplace_back(lengths1[0]);
					vert_lengths.emplace_back(lengths1[0]);
					vert_lengths.emplace_back(lengths1[0]);					
				}
				break;
			case 2:
				horiz_lengths.push_back(lengths1[0]);
				horiz_lengths.push_back(lengths1[1]);
				horiz_lengths.push_back(lengths1[0]);
				horiz_lengths.push_back(lengths1[1]);
				if(!extended_syntax) {
					vert_lengths.emplace_back(lengths1[0]);
					vert_lengths.emplace_back(lengths1[1]);
					vert_lengths.emplace_back(lengths1[0]);
					vert_lengths.emplace_back(lengths1[1]);					
				}
				break;
			case 3:
				horiz_lengths.push_back(lengths1[0]);
				horiz_lengths.push_back(lengths1[1]);
				horiz_lengths.push_back(lengths1[2]);
				horiz_lengths.push_back(lengths1[1]);
				if(!extended_syntax) {
					vert_lengths.push_back(lengths1[0]);
					vert_lengths.push_back(lengths1[1]);
					vert_lengths.push_back(lengths1[2]);
					vert_lengths.push_back(lengths1[1]);
				}
				break;
			default: 
				// is 4 or more already.
				horiz_lengths = lengths1;
				if(!extended_syntax) {
					vert_lengths = lengths1;
				}
				break;
		}
		if(extended_syntax) {
			switch(lengths2.size()) {
				case 0: /* Invalid case. Will have already thrown ParseError by this point. */ break;
				case 1:
					vert_lengths.emplace_back(lengths2[0]);
					vert_lengths.emplace_back(lengths2[0]);
					vert_lengths.emplace_back(lengths2[0]);
					vert_lengths.emplace_back(lengths2[0]);					
					break;
				case 2:
					vert_lengths.emplace_back(lengths2[0]);
					vert_lengths.emplace_back(lengths2[1]);
					vert_lengths.emplace_back(lengths2[0]);
					vert_lengths.emplace_back(lengths2[1]);					
					break;
				case 3:
					vert_lengths.emplace_back(lengths2[0]);
					vert_lengths.emplace_back(lengths2[1]);
					vert_lengths.emplace_back(lengths2[2]);
					vert_lengths.emplace_back(lengths2[1]);					
					break;
				default: 
					// is 4 or more already.
					vert_lengths = lengths2;
					break;
			}
		}
		plist_.addProperty(prefix + "-top-left-" + suffix, BorderRadius::create(horiz_lengths[0], vert_lengths[0]));
		plist_.addProperty(prefix + "-top-right-" + suffix, BorderRadius::create(horiz_lengths[1], vert_lengths[1]));
		plist_.addProperty(prefix + "-bottom-left-" + suffix, BorderRadius::create(horiz_lengths[2], vert_lengths[2]));
		plist_.addProperty(prefix + "-bottom-right-" + suffix, BorderRadius::create(horiz_lengths[3], vert_lengths[3]));
	}

	void PropertyParser::parseBackgroundClip(const std::string& prefix, const std::string& suffix)
	{
		BackgroundClip bc = BackgroundClip::BORDER_BOX;
		if(isToken(TokenId::IDENT)) {
			const std::string ref = (*it_)->getStringValue();
			advance();
			if(ref == "border-box") {
				bc = BackgroundClip::BORDER_BOX;
			} else if(ref == "padding-box") {
				bc = BackgroundClip::PADDING_BOX;
			} else if(ref == "content-box") {
				bc = BackgroundClip::CONTENT_BOX;
			} else {
				throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
			}
		} else {
			throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
		}
		plist_.addProperty(prefix, Style::create<BackgroundClip>(StyleId::BACKGROUND_CLIP, bc));
	}

	ImageSourcePtr PropertyParser::parseLinearGradient(const std::vector<TokenPtr>& tokens)
	{
		IteratorContext ic(*this, tokens);
		auto lingrad = LinearGradient::create();

		float angle = 180.0f;
		bool expect_comma = false;

		skipWhitespace();
		// Check for angle value first.
		if(isToken(TokenId::IDENT) && (*it_)->getStringValue() == "to") {
			advance();
			skipWhitespace();
			if(isToken(TokenId::IDENT)) {
				std::string direction1 = (*it_)->getStringValue();
				advance();
				expect_comma = true;
				if(direction1 == "left") {
					angle = 270.0f;
				} else if(direction1 == "right") {
					angle = 90.0f;
				} else if(direction1 == "top") {
					angle = 0;
				} else if(direction1 == "bottom") {
					angle = 180.0f;
				}
				skipWhitespace();
				if(isToken(TokenId::IDENT)) {
					std::string direction2 = (*it_)->getStringValue();
					advance();
					if(direction2 == "left") {
						if(angle == 0.0f) {
							angle = 315.0f;
						} else {
							angle = 225.0f;
						}
					} else if(direction2 == "right") {
						if(angle == 0.0f) {
							angle = 45.0f;
						} else {
							angle = 135.0f;
						}
					} else if(direction2 == "top") {
						if(angle == 90.0f) {
							angle = 45.0f;
						} else {
							angle = 315.0f;
						}
					} else if(direction2 == "bottom") {
						if(angle == 90.0f) {
							angle = 135.0f;
						} else {
							angle = 225.0f;
						}
					}
				}
			} else {
				throw ParserError(formatter() << "Expected identifier for 'linear-gradient' after 'to', found: " << (*it_)->toString());
			}
		} else if(isToken(TokenId::DIMENSION)) {
			Angle new_angle(static_cast<float>((*it_)->getNumericValue()), (*it_)->getStringValue());
			angle = new_angle.getAngle();
			while(angle < 0) {
				angle += 360.0f;
			}
			angle = std::fmod(angle, 360.0f);
			expect_comma = true;
			advance();
		}
		skipWhitespace();
		lingrad->setAngle(angle);

		if(expect_comma) {
			if(!isToken(TokenId::COMMA)) {
				throw ParserError(formatter() << "Expected comma while parsing linear gradient found: " << (*it_)->toString());
			}
			advance();
			skipWhitespace();
		}

		// <color-stop> [, <color-stop>]+
		// where <color-stop> = <color> [ <percentage> | <length> ]?

		std::shared_ptr<CssColor> cs_color = parseColorInternal();
		Length len;
		skipWhitespace();
		if(isToken(TokenId::DIMENSION)) {
			const std::string units = (*it_)->getStringValue();
			xhtml::FixedPoint value = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
			advance();
			len = Length(value, units);
		} else if(isToken(TokenId::PERCENT)) {
			xhtml::FixedPoint d = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
			advance();
			len = Length(d, true);
		}
		skipWhitespace();
		lingrad->addColorStop(LinearGradient::ColorStop(cs_color, len));

		while(isToken(TokenId::COMMA)) {
			advance();
			skipWhitespace();
			std::shared_ptr<CssColor> cs_color = parseColorInternal();
			Length len;

			skipWhitespace();
			if(isToken(TokenId::DIMENSION)) {
				const std::string units = (*it_)->getStringValue();
				xhtml::FixedPoint value = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
				advance();
				len = Length(value, units);
			} else if(isToken(TokenId::PERCENT)) {
				xhtml::FixedPoint d = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
				advance();
				len = Length(d, true);
			}
			skipWhitespace();

			lingrad->addColorStop(LinearGradient::ColorStop(cs_color, len));
		}
		
		if(lingrad->getColorStops().empty()) {
			throw ParserError(formatter() << "No color stops where found while processing linear-gradient");
		}

		return lingrad;
	}

	void PropertyParser::parseTransitionProperty(const std::string& prefix, const std::string& suffix)
	{
		std::vector<Property> props;
		while(!isEndToken()) {
			skipWhitespace();
			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				if(ref == "all") {
					// we use MAX_PROPERTIES to mean *ALL*
					props.emplace_back(Property::MAX_PROPERTIES);
				} else if(ref == "none") {
					plist_.addProperty(prefix, nullptr);
					return;
				} else {
					auto pit = get_property_table().find(ref);
					if(pit == get_property_table().end()) {
						// just giving up -- goes against spec
						throw ParserError(formatter() << "Couldn't find property with name " << ref << " in the list of all properties");
					}

					auto it = get_transitional_properties().find(pit->second.value);
					if(it == get_transitional_properties().end()) {
						throw ParserError(formatter() << "Couldn't find property with name " << ref << " in the list of transitional properties");
					}
					props.emplace_back(*it);
				}
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}
			skipWhitespace();
			if(isToken(TokenId::COMMA)) {
				advance();
				skipWhitespace();
			}
		}
		plist_.addProperty(prefix, TransitionProperties::create(props));
	}

	void PropertyParser::parseTransitionTimingFunction(const std::string& prefix, const std::string& suffix)
	{
		std::vector<TimingFunction> fns;
		while(!isEndToken()) {
			skipWhitespace();
			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				if(ref == "ease") {
					fns.emplace_back(0.25f, 0.1f, 0.25f, 1.0f);
				} else if(ref == "linear") {
					fns.emplace_back(0.0f, 0.0f, 1.0f, 1.0f);
				} else if(ref == "ease-in") {
					fns.emplace_back(0.42f, 0.0f, 1.0f, 1.0f);
				} else if(ref == "ease-out") {
					fns.emplace_back(0.0f, 0.0f, 0.58f, 1.0f);
				} else if(ref == "ease-in-out") {
					fns.emplace_back(0.42f, 0.0f, 0.58f, 1.0f);
				} else if(ref == "step-start") {
					fns.emplace_back(1, StepChangePoint::START);
				} else if(ref == "step-end") {
					fns.emplace_back(1, StepChangePoint::END);
				} else {
					throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
				}
			} else if(isToken(TokenId::FUNCTION)) {
				const std::string ref = (*it_)->getStringValue();
				auto tokens = (*it_)->getParameters();
				advance();
				if(ref == "cubic-bezier") {
					IteratorContext ic(*this, tokens);
					std::vector<float> pt;
					for(int n = 0; n != 4; ++n) {
						skipWhitespace();
						if(!isToken(TokenId::NUMBER)) {
							throw ParserError(formatter() << "Expected integer parsing '" << ref << "' function , property:'" << prefix << "'");
						}
						pt.emplace_back(static_cast<float>((*it_)->getNumericValue()));
						advance();
						skipWhitespace();
						if(n < 4-1) {
							if(!isToken(TokenId::COMMA)) {
								throw ParserError(formatter() << "Expected comma while parsing '" << ref << "' function, property: '" << prefix << "'");
							}
							advance();
						}
					}
					fns.emplace_back(pt[0], pt[1], pt[2], pt[3]);
				} else if(ref == "steps") {
					IteratorContext ic(*this, tokens);
					skipWhitespace();
					if(!isToken(TokenId::NUMBER)) {
						throw ParserError(formatter() << "Expected integer parsing '" << ref << "' function, property: '" << prefix << "'");
					}
					int nintervals = static_cast<int>((*it_)->getNumericValue());
					advance();
					skipWhitespace();
					if(isToken(TokenId::COMMA)) {
						advance();
						skipWhitespace();
						if(!isToken(TokenId::IDENT)) {
							throw ParserError(formatter() << "Expected 'start' or 'end' parsing '" << ref << "' function, property: '" << prefix << "'");
						}
						const std::string ref = (*it_)->getStringValue();
						if(ref == "start") {
							fns.emplace_back(nintervals, StepChangePoint::START);
						} else if( ref == "end") {
							fns.emplace_back(nintervals, StepChangePoint::END);
						} else {
							throw ParserError(formatter() << "Expected 'start' or 'end' parsing 'steps' function, found" << ref << " property: '" << prefix << "'");
						}
					} else {
						fns.emplace_back(nintervals, StepChangePoint::START);
					}
				} else {
					throw ParserError(formatter() << "Unrecognised function for '" << prefix << "' property: " << ref);
				}
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}
		}
		plist_.addProperty(prefix, TransitionTimingFunctions::create(fns));
	}

	void PropertyParser::parseTransitionTiming(const std::string& prefix, const std::string& suffix)
	{
		std::vector<float> times;

		while(!isEndToken()) {
			skipWhitespace();
			if(isToken(TokenId::DIMENSION)) {
				Time new_time(static_cast<float>((*it_)->getNumericValue()), (*it_)->getStringValue());
				advance();
				times.emplace_back(new_time.getTime(TimeUnits::SECONDS));
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}
			skipWhitespace();
			if(isToken(TokenId::COMMA)) {
				advance();
				skipWhitespace();
			}
		}

		plist_.addProperty(prefix, TransitionTiming::create(times));
	}

	void PropertyParser::parseTransition(const std::string& prefix, const std::string& suffix)
	{
		std::vector<float> durations;
		std::vector<float> delays;
		std::vector<TimingFunction> fns;
		std::vector<Property> props;

		// [ none | single-transition-property ] || time(duration) || timing fn || time(delay)
		// order of parsing is important.

		while(!isEndToken()) {
			skipWhitespace();
			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				if(ref == "none") {
					throw ParserError(formatter() << "none found in transition properties list.");
				} else if(ref == "all") {
					// we use the MAX_PROPERTIES as a placeholder for *ALL*.
					props.emplace_back(Property::MAX_PROPERTIES);
				} else {
					auto pit = get_property_table().find(ref);
					if(pit == get_property_table().end()) {
						// just giving up -- goes against spec
						throw ParserError(formatter() << "Couldn't find property with name " << ref << " in the list of all properties");
					}

					auto it = get_transitional_properties().find(pit->second.value);
					if(it == get_transitional_properties().end()) {
						throw ParserError(formatter() << "Couldn't find property with name " << ref << " in the list of transitional properties");
					}
					props.emplace_back(*it);
				}
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}
			skipWhitespace();

			if(isToken(TokenId::DIMENSION)) {
				Time new_time(static_cast<float>((*it_)->getNumericValue()), (*it_)->getStringValue());
				advance();
				durations.emplace_back(new_time.getTime(TimeUnits::SECONDS));
			} else if(isToken(TokenId::COMMA)) {
				advance();
				continue;
			} else if(isEndToken()) {
				continue;
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}
			skipWhitespace();

			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				if(ref == "ease") {
					fns.emplace_back(0.25f, 0.1f, 0.25f, 1.0f);
				} else if(ref == "linear") {
					fns.emplace_back(0.0f, 0.0f, 1.0f, 1.0f);
				} else if(ref == "ease-in") {
					fns.emplace_back(0.42f, 0.0f, 1.0f, 1.0f);
				} else if(ref == "ease-out") {
					fns.emplace_back(0.0f, 0.0f, 0.58f, 1.0f);
				} else if(ref == "ease-in-out") {
					fns.emplace_back(0.42f, 0.0f, 0.58f, 1.0f);
				} else if(ref == "step-start") {
					fns.emplace_back(1, StepChangePoint::START);
				} else if(ref == "step-end") {
					fns.emplace_back(1, StepChangePoint::END);
				} else {
					throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
				}
			} else if(isToken(TokenId::FUNCTION)) {
				const std::string ref = (*it_)->getStringValue();
				auto tokens = (*it_)->getParameters();
				advance();
				if(ref == "cubic-bezier") {
					IteratorContext ic(*this, tokens);
					std::vector<float> pt;
					for(int n = 0; n != 4; ++n) {
						skipWhitespace();
						if(!isToken(TokenId::NUMBER)) {
							throw ParserError(formatter() << "Expected integer parsing '" << ref << "' function , property:'" << prefix << "'");
						}
						pt.emplace_back(static_cast<float>((*it_)->getNumericValue()));
						advance();
						skipWhitespace();
						if(n < 4-1) {
							if(!isToken(TokenId::COMMA)) {
								throw ParserError(formatter() << "Expected comma while parsing '" << ref << "' function, property: '" << prefix << "'");
							}
							advance();
						}
					}
					if(pt[0] < 0.0f || pt[0] > 1.0f) {
						throw ParserError(formatter() << "cubic-bezier function X values must be in range [0,1], X1 was: " << pt[0] << " property: " << prefix);
					}
					if(pt[2] < 0.0f || pt[2] > 1.0f) {
						throw ParserError(formatter() << "cubic-bezier function X values must be in range [0,1], X2 was: " << pt[2] << " property: " << prefix);
					}
					fns.emplace_back(pt[0], pt[1], pt[2], pt[3]);
				} else if(ref == "steps") {
					IteratorContext ic(*this, tokens);
					skipWhitespace();
					if(!isToken(TokenId::NUMBER)) {
						throw ParserError(formatter() << "Expected integer parsing '" << ref << "' function, property: '" << prefix << "'");
					}
					int nintervals = static_cast<int>((*it_)->getNumericValue());
					if(nintervals < 1) {
						throw ParserError(formatter() << "step function interval expected to be greater than 1, was: " << nintervals << " property: " << prefix);
					}
					advance();
					skipWhitespace();
					if(isToken(TokenId::COMMA)) {
						advance();
						skipWhitespace();
						if(!isToken(TokenId::IDENT)) {
							throw ParserError(formatter() << "Expected 'start' or 'end' parsing '" << ref << "' function, property: '" << prefix << "'");
						}
						const std::string ref = (*it_)->getStringValue();
						if(ref == "start") {
							fns.emplace_back(nintervals, StepChangePoint::START);
						} else if( ref == "end") {
							fns.emplace_back(nintervals, StepChangePoint::END);
						} else {
							throw ParserError(formatter() << "Expected 'start' or 'end' parsing 'steps' function, found" << ref << " property: '" << prefix << "'");
						}
					}
				} else {
					throw ParserError(formatter() << "Unrecognised function for '" << prefix << "' property: " << ref);
				}
			} else if(isToken(TokenId::COMMA)) {
				advance();
				continue;
			} else if(isEndToken()) {
				continue;
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}
			skipWhitespace();

			if(isToken(TokenId::DIMENSION)) {
				Time new_time(static_cast<float>((*it_)->getNumericValue()), (*it_)->getStringValue());
				advance();
				delays.emplace_back(new_time.getTime(TimeUnits::SECONDS));
			} else if(isToken(TokenId::COMMA)) {
				advance();
				continue;
			} else if(isEndToken()) {
				continue;
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}
		}
		plist_.addProperty(prefix + "-property", TransitionProperties::create(props));
		plist_.addProperty(prefix + "-duration", TransitionTiming::create(durations));
		plist_.addProperty(prefix + "-timing-function", TransitionTimingFunctions::create(fns));
		plist_.addProperty(prefix + "-delay", TransitionTiming::create(delays));
	}

	void PropertyParser::parseTextShadow(const std::string& prefix, const std::string& suffix)
	{
		std::vector<TextShadow> shadows;
		std::vector<Length> lengths;
		std::shared_ptr<CssColor> color = std::make_shared<CssColor>();
		while(!isEndToken()) {
			skipWhitespace();
			if(isToken(TokenId::IDENT)) {
				const std::string ref = (*it_)->getStringValue();
				advance();
				if(ref == "none") {
					// no style specified
					return;
				} else {
					color->setColor(KRE::Color(ref));
				}
			} else if(isToken(TokenId::NUMBER)) {
				xhtml::FixedPoint d = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
				advance();
				skipWhitespace();
				lengths.emplace_back(Length(d, false));
			} else if(isToken(TokenId::DIMENSION)) {
				const std::string units = (*it_)->getStringValue();
				xhtml::FixedPoint value = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
				advance();
				lengths.emplace_back(Length(value, units));
			} else if(isToken(TokenId::PERCENT)) {
				xhtml::FixedPoint d = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
				advance();
				lengths.emplace_back(Length(d, true));
			} else {
				parseColor2(color);
			}
			skipWhitespace();

			if(isToken(TokenId::COMMA)) {
				advance();
				skipWhitespace();
				if(lengths.size() < 2) {
					throw ParserError(formatter() << "A text shadow definition requires at least 2 length values. found: " << lengths.size());
				}
				shadows.emplace_back(lengths, color != nullptr ? *color : CssColor());
				lengths.clear();
			}
		}

		if(lengths.size() >= 2) {
			shadows.emplace_back(lengths, color != nullptr ? *color : CssColor());
		}
		plist_.addProperty(prefix, TextShadowStyle::create(shadows));
	}

	void PropertyParser::parseFilters(const std::string& prefix, const std::string& suffix)
	{
		std::vector<FilterPtr> filter_list;
		while(!isEndToken()) {
			skipWhitespace();
			if(isToken(TokenId::IDENT)) {
				auto ref = (*it_)->getStringValue();
				advance();
				if(ref == "none") {
					if(!filter_list.empty()) {
						throw ParserError(formatter() << "It is an error to have 'none' appearing in a '" << prefix << "' list.");
					}
					plist_.addProperty(prefix, FilterStyle::create());
					return;
				} else {
					throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
				}
			} else if(isToken(TokenId::URL)) {
				// XXX Not supporting URL based filter specifications at this time -- maybe in the far future?
				LOG_ERROR("Dropping declaration for '" << prefix << "' no support uri filter");
			} else if(isToken(TokenId::FUNCTION)) {
				const std::string ref = (*it_)->getStringValue();
				auto params = (*it_)->getParameters();
				advance();
				IteratorContext ic(*this, params);
				if(ref == "blur") {
					filter_list.emplace_back(std::make_shared<Filter>(CssFilterId::BLUR, parseLengthInternal(NumericParseOptions::LENGTH)));
				} else if(ref == "brightness") {
					filter_list.emplace_back(std::make_shared<Filter>(CssFilterId::BRIGHTNESS, parseLengthInternal(NumericParseOptions::NUMBER_OR_PERCENT)));
				} else if(ref == "contrast") {
					filter_list.emplace_back(std::make_shared<Filter>(CssFilterId::CONTRAST, parseLengthInternal(NumericParseOptions::NUMBER_OR_PERCENT)));
				} else if(ref == "drop-shadow") {
					std::vector<Length> lengths;
					std::shared_ptr<CssColor> color = std::make_shared<CssColor>(CssColorParam::CURRENT);
					bool inset = false;
					while(!isEndToken()) {
						skipWhitespace();
						if(isToken(TokenId::DIMENSION)) {
							xhtml::FixedPoint value = static_cast<xhtml::FixedPoint>((*it_)->getNumericValue() * fixed_point_scale);
							lengths.emplace_back(Length(value, (*it_)->getStringValue()));
							advance();
						} else if(isToken(TokenId::IDENT)) {
							std::string colval = (*it_)->getStringValue();
							color->setColor(KRE::Color(colval));
							advance();
						} else {
							parseColor2(color);
						}
					}
					if(lengths.size() >= 2 && lengths.size() <= 4) {
						filter_list.emplace_back(std::make_shared<Filter>(CssFilterId::DROP_SHADOW, BoxShadow(inset, 
							lengths[0], 
							lengths[1], 
							lengths.size() < 3 ? Length() : lengths[2], 
							lengths.size() < 4 ? Length() : lengths[3],
							*color)));
					} else {
						throw ParserError(formatter() << "Unrecognised parmeters to drop-shadow function in property '" << prefix << "')");
					}
				} else if(ref == "grayscale") {
					filter_list.emplace_back(std::make_shared<Filter>(CssFilterId::GRAYSCALE, parseLengthInternal(NumericParseOptions::NUMBER_OR_PERCENT)));
				} else if(ref == "hue-rotate") {
					if(isToken(TokenId::DIMENSION)) {
						Angle new_angle(static_cast<float>((*it_)->getNumericValue()), (*it_)->getStringValue());
						filter_list.emplace_back(std::make_shared<Filter>(CssFilterId::HUE_ROTATE, new_angle));
					} else {
						throw ParserError(formatter() << "Expected angle in degrees for rotate function, in property '" << prefix << "', found: " << (*it_)->toString());
					}
				} else if(ref == "invert") {
					filter_list.emplace_back(std::make_shared<Filter>(CssFilterId::INVERT, parseLengthInternal(NumericParseOptions::NUMBER_OR_PERCENT)));
				} else if(ref == "opacity") {
					filter_list.emplace_back(std::make_shared<Filter>(CssFilterId::OPACITY, parseLengthInternal(NumericParseOptions::NUMBER_OR_PERCENT)));
				} else if(ref == "sepia") {
					filter_list.emplace_back(std::make_shared<Filter>(CssFilterId::SEPIA, parseLengthInternal(NumericParseOptions::NUMBER_OR_PERCENT)));
				} else if(ref == "saturate") {
					filter_list.emplace_back(std::make_shared<Filter>(CssFilterId::SATURATE, parseLengthInternal(NumericParseOptions::NUMBER_OR_PERCENT)));
				} else {
					throw ParserError(formatter() << "Unrecognised function for '" << prefix << "' property: " << ref);
				}
			} else if(isEndToken()) {
				// do nothing
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}
		}
		plist_.addProperty(prefix, FilterStyle::create(filter_list));
	}
	
	void PropertyParser::parseTransform(const std::string& prefix, const std::string& suffix)
	{
		std::vector<Transform> transforms;
		while(!isEndToken()) {
			skipWhitespace();
			if(isToken(TokenId::IDENT)) {
				auto ref = (*it_)->getStringValue();
				advance();
				if(ref == "none") {
					if(!transforms.empty()) {
						throw ParserError(formatter() << "It is an error to have 'none' appearing in a '" << prefix << "' list.");
					}
					plist_.addProperty(prefix, FilterStyle::create());
					return;
				} else {
					throw ParserError(formatter() << "Unrecognised identifier for '" << prefix << "' property: " << ref);
				}
			} else if(isToken(TokenId::FUNCTION)) {
				const std::string ref = (*it_)->getStringValue();
				auto params = (*it_)->getParameters();
				advance();
				IteratorContext ic(*this, params);
				skipWhitespace();
				if(ref == "matrix") {
				} else if(ref == "translate") {
					Length tx = parseLengthInternal(NumericParseOptions::LENGTH_OR_PERCENT);
					Length ty;
					skipWhitespace();
					if(isToken(TokenId::COMMA)) {
						advance();
						skipWhitespace();
						ty = parseLengthInternal(NumericParseOptions::LENGTH_OR_PERCENT);
					}
					transforms.emplace_back(TransformId::TRANSLATE_2D, tx, ty);
				} else if(ref == "translateX") {
					Length tx = parseLengthInternal(NumericParseOptions::LENGTH_OR_PERCENT);
					transforms.emplace_back(TransformId::TRANSLATE_2D, tx, Length());
				} else if(ref == "translateY") {
					Length ty = parseLengthInternal(NumericParseOptions::LENGTH_OR_PERCENT);
					transforms.emplace_back(TransformId::TRANSLATE_2D, Length(), ty);
				} else if(ref == "scale") {
					Length sx = parseLengthInternal(NumericParseOptions::NUMBER);
					Length sy = sx;
					skipWhitespace();
					if(isToken(TokenId::COMMA)) {
						advance();
						skipWhitespace();
						sy = parseLengthInternal(NumericParseOptions::NUMBER);
					}
					transforms.emplace_back(TransformId::SCALE_2D, sx, sy);
				} else if(ref == "scaleX") {
					Length sx = parseLengthInternal(NumericParseOptions::NUMBER);
					Length sy(fixed_point_scale, false);
					transforms.emplace_back(TransformId::SCALE_2D, sx, sy);
				} else if(ref == "scaleY") {
					Length sx(fixed_point_scale, false);
					Length sy = parseLengthInternal(NumericParseOptions::NUMBER);
					transforms.emplace_back(TransformId::SCALE_2D, sx, sy);
				} else if(ref == "rotate") {
					if(isToken(TokenId::DIMENSION)) {
						std::array<Angle, 2> angles;
						angles[0] = Angle(static_cast<float>((*it_)->getNumericValue()), (*it_)->getStringValue());
						angles[1] = Angle();
						transforms.emplace_back(TransformId::ROTATE_2D, angles);
					} else if(isToken(TokenId::NUMBER)) {
						// just going to soak this, as we only accept 0 which would be equivalent to no rotatation
					} else {
						throw ParserError(formatter() << "Expected angle in degrees for rotate function, in property '" << prefix << "', found: " << (*it_)->toString());
					}
				} else if(ref == "skew") {
					std::array<Angle, 2> angles;
					angles[0] = Angle();
					angles[1] = Angle();
					if(isToken(TokenId::DIMENSION)) {
						angles[0] = Angle(static_cast<float>((*it_)->getNumericValue()), (*it_)->getStringValue());
					} else if(isToken(TokenId::NUMBER)) {
						// just going to soak this, as we only accept 0 which would be equivalent to no skew
					} else {
						throw ParserError(formatter() << "Expected angle in degrees for rotate function, in property '" << prefix << "', found: " << (*it_)->toString());
					}
					skipWhitespace();
					if(isToken(TokenId::COMMA)) {
						advance();
						skipWhitespace();
						if(isToken(TokenId::DIMENSION)) {
							angles[1] = Angle(static_cast<float>((*it_)->getNumericValue()), (*it_)->getStringValue());
						} else if(isToken(TokenId::NUMBER)) {
							// just going to soak this, as we only accept 0 which would be equivalent to no skew
						} else {
							throw ParserError(formatter() << "Expected angle in degrees for rotate function, in property '" << prefix << "', found: " << (*it_)->toString());
						}
					}
					transforms.emplace_back(TransformId::ROTATE_2D, angles);
				} else if(ref == "skewX") {
					if(isToken(TokenId::DIMENSION)) {
						std::array<Angle, 2> angles;
						angles[0] = Angle(static_cast<float>((*it_)->getNumericValue()), (*it_)->getStringValue());
						angles[1] = Angle();
						transforms.emplace_back(TransformId::SKEWX_2D, angles);
					} else if(isToken(TokenId::NUMBER)) {
						// just going to soak this, as we only accept 0 which would be equivalent to no skew
					} else {
						throw ParserError(formatter() << "Expected angle in degrees for rotate function, in property '" << prefix << "', found: " << (*it_)->toString());
					}
				} else if(ref == "skewY") {
					if(isToken(TokenId::DIMENSION)) {
						std::array<Angle, 2> angles;
						angles[0] = Angle();
						angles[1] = Angle(static_cast<float>((*it_)->getNumericValue()), (*it_)->getStringValue());
						transforms.emplace_back(TransformId::SKEWY_2D, angles);
					} else if(isToken(TokenId::NUMBER)) {
						// just going to soak this, as we only accept 0 which would be equivalent to no skew
					} else {
						throw ParserError(formatter() << "Expected angle in degrees for rotate function, in property '" << prefix << "', found: " << (*it_)->toString());
					}
				} else {
					throw ParserError(formatter() << "Unrecognised function for '" << prefix << "' property: " << ref);
				}
			} else if(isEndToken()) {
				// do nothing
			} else {
				throw ParserError(formatter() << "Unrecognised value for property '" << prefix << "': "  << (*it_)->toString());
			}
		}
		plist_.addProperty(prefix, TransformStyle::create(transforms));
	}
}
