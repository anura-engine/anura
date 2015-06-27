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

#include "asserts.hpp"
#include "Color.hpp"
#include "Texture.hpp"
#include "xhtml_fwd.hpp"

#define MAKE_FACTORY(classname)																\
	template<typename... T>																	\
	static std::shared_ptr<classname> create(T&& ... all) {									\
		return std::make_shared<classname>(std::forward<T>(all)...);						\
	}


namespace xhtml
{
	class RenderContext;
	typedef int FixedPoint;
}

namespace css
{
	typedef std::array<int,3> Specificity;

	inline bool operator==(const Specificity& lhs, const Specificity& rhs) {
		return lhs[0] == rhs[0] && lhs[1] == rhs[1] && lhs[2] == rhs[2];
	}
	inline bool operator<(const Specificity& lhs, const Specificity& rhs) {
		return lhs[0] == rhs[0] ? lhs[1] == rhs[1] ? lhs[2] < rhs[2] : lhs[1] < rhs[1] : lhs[0] < rhs[0];
	}
	inline bool operator<=(const Specificity& lhs, const Specificity& rhs) {
		return lhs == rhs || lhs < rhs;
	}

	enum class Property {
		BACKGROUND_ATTACHMENT,
		BACKGROUND_COLOR,
		BACKGROUND_IMAGE,
		BACKGROUND_POSITION,
		BACKGROUND_REPEAT,
		BORDER_COLLAPSE,
		BORDER_TOP_COLOR,
		BORDER_LEFT_COLOR,
		BORDER_BOTTOM_COLOR,
		BORDER_RIGHT_COLOR,
		BORDER_TOP_STYLE,
		BORDER_LEFT_STYLE,
		BORDER_BOTTOM_STYLE,
		BORDER_RIGHT_STYLE,
		BORDER_TOP_WIDTH,
		BORDER_LEFT_WIDTH,
		BORDER_BOTTOM_WIDTH,
		BORDER_RIGHT_WIDTH,
		BOTTOM,
		CAPTION_SIDE,
		CLEAR,
		CLIP,
		COLOR,
		CONTENT,
		COUNTER_INCREMENT,
		COUNTER_RESET,
		CURSOR,
		DIRECTION,
		DISPLAY,
		EMPTY_CELLS,
		FLOAT,
		FONT_FAMILY,
		FONT_SIZE,
		FONT_STYLE,
		FONT_VARIANT,
		FONT_WEIGHT,
		HEIGHT,
		LEFT,
		LETTER_SPACING,
		LINE_HEIGHT,
		LIST_STYLE_IMAGE,
		LIST_STYLE_POSITION,
		LIST_STYLE_TYPE,
		MARGIN_TOP,
		MARGIN_LEFT,
		MARGIN_BOTTOM,
		MARGIN_RIGHT,
		MAX_HEIGHT,
		MAX_WIDTH,
		MIN_HEIGHT,
		MIN_WIDTH,
		ORPHANS,
		OUTLINE_COLOR,
		OUTLINE_STYLE,
		OUTLINE_WIDTH,
		CSS_OVERFLOW,		// had to decorate the name because it clashes with a #define
		PADDING_TOP,
		PADDING_LEFT,
		PADDING_RIGHT,
		PADDING_BOTTOM,
		POSITION,
		QUOTES,
		RIGHT,
		TABLE_LAYOUT,
		TEXT_ALIGN,
		TEXT_DECORATION,
		TEXT_INDENT,
		TEXT_TRANSFORM,
		TOP,
		UNICODE_BIDI,
		VERTICAL_ALIGN,
		VISIBILITY,
		WHITE_SPACE,
		WIDOWS,
		WIDTH,
		WORD_SPACING,
		Z_INDEX,

		// CSS3 provision properties
		BOX_SHADOW,
		TEXT_SHADOW,
		TRANSITION_PROPERTY,
		TRANSITION_DURATION,
		TRANSITION_TIMING_FUNCTION,
		TRANSITION_DELAY,
		BORDER_TOP_LEFT_RADIUS,
		BORDER_TOP_RIGHT_RADIUS,
		BORDER_BOTTOM_LEFT_RADIUS,
		BORDER_BOTTOM_RIGHT_RADIUS,
		BORDER_SPACING,
		OPACITY,
		BORDER_IMAGE_SOURCE,
		BORDER_IMAGE_SLICE,
		BORDER_IMAGE_WIDTH,
		BORDER_IMAGE_OUTSET,
		BORDER_IMAGE_REPEAT,
		BACKGROUND_CLIP,

		MAX_PROPERTIES,
	};

	enum class Side {
		TOP,
		LEFT,
		BOTTOM, 
		RIGHT,
	};

	enum class CssColorParam {
		NONE,
		TRANSPARENT,
		VALUE,
		CURRENT,		// use current foreground color
	};

	class Style;
	typedef std::shared_ptr<Style> StylePtr;

	enum class StyleId {
		INHERIT, // pseudo style
		COLOR,
		WIDTH,
		LENGTH,
		IMAGE_SOURCE,
		FONT_FAMILY,
		FONT_WEIGHT,
		FONT_SIZE,
		DISPLAY,
		POSITION,
		FLOAT,
		BORDER_STYLE,
		CLIP,
		CONTENT,
		COUNTERS,
		CURSOR,
		LIST_STYLE_IMAGE,
		QUOTES,
		VERTICAL_ALIGN,
		ZINDEX,
		BOX_SHADOW,
		BORDER_IMAGE_REPEAT,
		WIDTH_LIST,
		BORDER_IMAGE_SLICE,
		BORDER_RADIUS,
		TRANSITION_PROPERTIES,
		TRANSITION_TIMING,
		TRANSITION_TIMING_FUNCTION,
		BACKGROUND_POSITION,
		FONT_VARIANT,
		WHITE_SPACE,
		TEXT_ALIGN,
		DIRECTION,
		TEXT_TRANSFORM,
		CSS_OVERFLOW,
		BACKGROUND_REPEAT,
		LIST_STYLE_TYPE,
		BACKGROUND_ATTACHMENT,
		LIST_STYLE_POSITION,
		TEXT_DECORATION,
		UNICODE_BIDI,
		VISIBILITY,
		BACKGROUND_CLIP,
		FONT_STYLE,
		ClEAR,
		TEXT_SHADOW,
	};

	enum class CssTransitionTimingFunction {
		STEPS,
		CUBIC_BEZIER,
	};

	enum class StepChangePoint {
		START,
		END,
	};

	// This is needed before the definition of Style
	class TimingFunction
	{
	public:
		TimingFunction() : ttfn_(CssTransitionTimingFunction::CUBIC_BEZIER), nintervals_(0), poc_(StepChangePoint::END), p1_(0.25f, 0.1f), p2_(0.25f, 1.0f) {}
		// for cubic-bezier
		explicit TimingFunction(float x1, float y1, float x2, float y2) 
			: ttfn_(CssTransitionTimingFunction::CUBIC_BEZIER), nintervals_(0), poc_(StepChangePoint::END), p1_(x1, y1), p2_(x2, y2) {}
		// for step function
		explicit TimingFunction(int nintervals, StepChangePoint poc)
			: ttfn_(CssTransitionTimingFunction::STEPS), nintervals_(nintervals), poc_(poc), p1_(), p2_() {}
		CssTransitionTimingFunction getFunction() const { return ttfn_; }
		int getIntervals() const { return nintervals_; }
		StepChangePoint getStepChangePoint() const { return poc_; }
		const glm::vec2& getP1() const { return p1_; }
		const glm::vec2& getP2() const { return p2_; }
	private:
		CssTransitionTimingFunction ttfn_;
		int nintervals_;
		StepChangePoint poc_;
		glm::vec2 p1_;
		glm::vec2 p2_;
	};

	struct StyleTransition 
	{
		StyleTransition() : duration(0), ttfn(), delay() {}
		StyleTransition(float dura, TimingFunction fn, float del) : duration(dura), ttfn(fn), delay(del) {}
		float duration;
		TimingFunction ttfn;
		float delay;
	};

	// This is the basee class for all other styles.
	class Style : public std::enable_shared_from_this<Style>
	{
	public:
		template<typename T> explicit Style(StyleId id, T value) 
			: id_(id), 
			  is_important_(false), 
			  is_inherited_(false), 
			  stored_enum_(true), 
			  enumeration_(static_cast<int>(value)),
			  transitions_()
		{}
		explicit Style(StyleId id) : id_(id), is_important_(false), is_inherited_(false), stored_enum_(false), enumeration_(0), transitions_() {}
		explicit Style(bool inh) : id_(StyleId::INHERIT), is_important_(false), is_inherited_(inh), stored_enum_(false), enumeration_(0), transitions_() {}
		virtual ~Style() {}
		StyleId id() const { return id_; }
		void setImportant(bool imp=true) { is_important_ = imp; }
		void setInherited(bool inh=true) { is_inherited_ = inh; }
		bool isImportant() const { return is_important_; }
		bool isInherited() const { return is_inherited_; }
		bool operator==(const StylePtr& style) const;
		bool operator!=(const StylePtr& style) const { return !operator==(style); }
		template<typename T> T getEnum() const { 
			ASSERT_LOG(stored_enum_ == true, "Requested an enumeration for this style, which isn't an enumerated type.");
			return static_cast<T>(enumeration_); 
		}
		template<typename T> void setEnum(T value) {
			enumeration_ = static_cast<int>(value);
			stored_enum_ = true;
		}
		template<typename T> std::shared_ptr<T> asType() { 
			auto ptr = std::dynamic_pointer_cast<T>(shared_from_this()); 
			ASSERT_LOG(ptr != nullptr, "Could not convert from " << static_cast<int>(id_));
			return ptr;
		}
		template<typename T> static StylePtr create(StyleId id, T value) { return std::make_shared<Style>(id, value); }
		void addTransition(float duration, const TimingFunction& ttfn, float delay) { transitions_.emplace_back(duration, ttfn, delay); }
		bool hasTransition() const { return !transitions_.empty(); }
		const std::vector<StyleTransition>& getTransitions() const { return transitions_; }
	private:
		virtual bool isEqual(const StylePtr& style) const;
		StyleId id_;
		bool is_important_;
		bool is_inherited_;
		bool stored_enum_;
		int enumeration_;

		// transitions
		std::vector<StyleTransition> transitions_;
	};

	class CssColor : public Style
	{
	public:
		MAKE_FACTORY(CssColor);
		CssColor();
		explicit CssColor(CssColorParam param, const KRE::Color& color=KRE::Color::colorWhite());
		void setParam(CssColorParam param);
		void setColor(const KRE::Color& color);
		CssColorParam getParam() const { return param_; }
		const KRE::ColorPtr& getColor() const { return color_; }	
		bool isTransparent() const { return param_ == CssColorParam::TRANSPARENT; }
		bool isNone() const { return param_ == CssColorParam::NONE; }
		bool isValue() const { return param_ == CssColorParam::VALUE; }
		KRE::ColorPtr compute() const;
	private:
		bool isEqual(const StylePtr& style) const override;
		CssColorParam param_;
		KRE::ColorPtr color_;
	};

	enum class LengthUnits {
		NUMBER,		// Plain old number
		EM,			// Computed value of the font-size property
		EX,			// Computed height of lowercase 'x'
		IN,			// Inches
		CM,			// Centimeters
		MM,			// Millimeters
		PT,			// Point size, equal to 1/72 of an inch
		PC,			// Picas. 1 pica = 12pt
		PX,			// Pixels. 1px = 0.75pt
		PERCENT		// percent value
	};
	
	class Length : public Style
	{
	public:
		MAKE_FACTORY(Length);
		Length() : Style(StyleId::LENGTH), value_(0), units_(LengthUnits::NUMBER) {}
		explicit Length(xhtml::FixedPoint value, bool is_percent) : Style(StyleId::LENGTH), value_(value), units_(is_percent ? LengthUnits::PERCENT : LengthUnits::NUMBER) {}
		explicit Length(xhtml::FixedPoint value, LengthUnits units) : Style(StyleId::LENGTH), value_(value), units_(units) {}
		explicit Length(xhtml::FixedPoint value, const std::string& units);
		bool isNumber() const { return units_ == LengthUnits::NUMBER; }
		bool isPercent() const { return units_ == LengthUnits::PERCENT; }
		bool isLength() const {  return units_ != LengthUnits::NUMBER && units_ != LengthUnits::PERCENT; }
		xhtml::FixedPoint compute(xhtml::FixedPoint scale=65536) const;
		bool operator==(const Length& a) const;
		xhtml::FixedPoint getValue() const { return value_; }
		LengthUnits getUnits() const { return units_; }
	private:
		bool isEqual(const StylePtr& style) const override;
		xhtml::FixedPoint value_;
		LengthUnits units_;
	};

	enum class AngleUnits {
		DEGREES,
		RADIANS,
		GRADIANS,
		TURNS,
	};

	class Angle
	{
	public:
		Angle() : value_(0), units_(AngleUnits::DEGREES) {}
		explicit Angle(float angle, AngleUnits units) : value_(angle), units_(units) {}
		explicit Angle(float angle, const std::string& units);		
		float getAngle(AngleUnits units=AngleUnits::DEGREES);
	private:
		float value_;
		AngleUnits units_;
	};

	enum class TimeUnits {
		SECONDS,
		MILLISECONDS,
	};

	class Time
	{
	public:
		Time() : value_(0), units_(TimeUnits::SECONDS) {}
		explicit Time(float t, TimeUnits units) : value_(t), units_(units) {}
		explicit Time(float t, const std::string& units);		
		float getTime(TimeUnits units=TimeUnits::SECONDS);
	private:
		float value_;
		TimeUnits units_;
	};

	class Width : public Style
	{
	public:
		MAKE_FACTORY(Width);
		Width() : Style(StyleId::WIDTH), is_auto_(false), width_() {}
		explicit Width(bool a) : Style(StyleId::WIDTH), is_auto_(a), width_() {}
		explicit Width(const Length& len) : Style(StyleId::WIDTH), is_auto_(false), width_(len) {}
		bool isAuto() const { return is_auto_; }
		const Length& getLength() const { return width_; }
		bool isEqual(const StylePtr& style) const override;
		bool operator==(const Width& a) const {
			return is_auto_ != a.is_auto_ ? false : is_auto_ ? true : width_ == a.width_;
		}
	private:
		bool is_auto_;
		Length width_;
	};

	enum class BorderStyle {
		NONE,
		HIDDEN,
		DOTTED,
		DASHED,
		SOLID,
		DOUBLE,
		GROOVE,
		RIDGE,
		INSET,
		OUTSET,
	};

	class ImageSource : public Style
	{
	public:
		ImageSource() : Style(StyleId::IMAGE_SOURCE) {}
		virtual ~ImageSource() {}
		// Returns a texture to use, width/height are only suggestions because
		// textures may not have intrinsic values (i.e. linear gradients)
		virtual KRE::TexturePtr getTexture(xhtml::FixedPoint width, xhtml::FixedPoint height) = 0;
	private:
	};
	typedef std::shared_ptr<ImageSource> ImageSourcePtr;

	class UriStyle : public ImageSource
	{
	public:
		MAKE_FACTORY(UriStyle);
		UriStyle() : is_none_(true), uri_() {}
		explicit UriStyle(bool none) : is_none_(none), uri_() {}
		explicit UriStyle(const std::string uri) : is_none_(false), uri_(uri) {}
		bool isNone() const { return is_none_; }
		const std::string& getUri() const { return uri_; }
		void setURI(const std::string& uri) { uri_ = uri; is_none_ = false; }
		bool isEqual(const StylePtr& style) const override;
		KRE::TexturePtr getTexture(xhtml::FixedPoint width, xhtml::FixedPoint height) override;
	private:
		bool is_none_;
		std::string uri_;
	};

	class LinearGradient : public ImageSource
	{
	public:
		MAKE_FACTORY(LinearGradient);
		LinearGradient() : angle_(0), color_stops_() {}
		struct ColorStop {
			ColorStop() : color(), length() {}
			explicit ColorStop(const std::shared_ptr<CssColor>& c, const Length& l) : color(c), length(l) {}
			std::shared_ptr<CssColor> color;
			Length length;
		};
		void setAngle(float angle) { angle_ = angle; }
		void clearColorStops() { color_stops_.clear(); }
		void addColorStop(const ColorStop& cs) { color_stops_.emplace_back(cs); }
		const std::vector<ColorStop>& getColorStops() const { return color_stops_; }
		bool isEqual(const StylePtr& style) const override;
		KRE::TexturePtr getTexture(xhtml::FixedPoint width, xhtml::FixedPoint height) override;
	private:
		float angle_;	// in degrees
		std::vector<ColorStop> color_stops_;
	};

	class FontFamily : public Style
	{
	public:
		MAKE_FACTORY(FontFamily);
		FontFamily();
		explicit FontFamily(const std::vector<std::string>& fonts) : Style(StyleId::FONT_FAMILY), fonts_(fonts) {}
		bool isEqual(const StylePtr& style) const override;
		const std::vector<std::string>& getFontList() const { return fonts_; }
	private:
		std::vector<std::string> fonts_;
	};

	enum class FontSizeAbsolute {
		NONE,
		XX_SMALL,
		X_SMALL,
		SMALL,
		MEDIUM,
		LARGE,
		X_LARGE,
		XX_LARGE,
		XXX_LARGE,
	};

	enum class FontSizeRelative {
		NONE,
		LARGER,
		SMALLER,
	};

	class FontSize : public Style
	{
	public:
		MAKE_FACTORY(FontSize);
		FontSize() 
			: Style(StyleId::FONT_SIZE),
			  is_absolute_(false), 
			  absolute_(FontSizeAbsolute::NONE), 
			  is_relative_(false), 
			  relative_(FontSizeRelative::NONE), 
			  is_length_(false), 
			  length_()
		{
		}
		explicit FontSize(FontSizeAbsolute absvalue) 
			: Style(StyleId::FONT_SIZE),
			  is_absolute_(true), 
			  absolute_(absvalue), 
			  is_relative_(false), 
			  relative_(FontSizeRelative::NONE), 
			  is_length_(false), 
			  length_()
		{
		}
		explicit FontSize(const Length& len) 
			: Style(StyleId::FONT_SIZE),
			  is_absolute_(false), 
			  absolute_(FontSizeAbsolute::NONE), 
			  is_relative_(false), 
			  relative_(FontSizeRelative::NONE), 
			  is_length_(true), 
			  length_(len)
		{
		}
		void setFontSize(FontSizeAbsolute absvalue) { 
			disableAll();
			absolute_ = absvalue;
			is_absolute_ = true;
		}
		void setFontSize(FontSizeRelative rel) { 
			disableAll();
			relative_ = rel;
			is_relative_ = true;
		}
		void setFontSize(const Length& len) { 
			disableAll();
			length_ = len;
			is_length_ = true;
		}
		xhtml::FixedPoint compute(xhtml::FixedPoint parent_fs, int dpi) const;
		bool isEqual(const StylePtr& style) const override;
	private:
		bool is_absolute_;
		FontSizeAbsolute absolute_;
		bool is_relative_;
		FontSizeRelative relative_;
		bool is_length_;
		Length length_;
		void disableAll() { is_absolute_= false; is_relative_ = false; is_length_ = false; }
	};

	enum class Float {
		NONE,
		LEFT,
		RIGHT,
	};
	
	enum class Display {
		NONE,
		INLINE,
		BLOCK,
		LIST_ITEM,
		INLINE_BLOCK,
		TABLE,
		INLINE_TABLE,
		TABLE_ROW_GROUP,
		TABLE_HEADER_GROUP,
		TABLE_FOOTER_GROUP,
		TABLE_ROW,
		TABLE_COLUMN_GROUP,
		TABLE_COLUMN,
		TABLE_CELL,
		TABLE_CAPTION,
	};

	enum class Whitespace {
		NORMAL,
		PRE,
		NOWRAP,
		PRE_WRAP,
		PRE_LINE,
	};

	enum class FontStyle {
		NORMAL,
		ITALIC,
		OBLIQUE,
	};

	enum class FontVariant {
		NORMAL,
		SMALL_CAPS,
	};

	enum class FontWeightRelative {
		LIGHTER,
		BOLDER,
	};

	class FontWeight : public Style
	{
	public:
		MAKE_FACTORY(FontWeight);
		FontWeight() : Style(StyleId::FONT_WEIGHT), is_relative_(false), weight_(400), relative_(FontWeightRelative::LIGHTER) {}
		explicit FontWeight(FontWeightRelative r) : Style(StyleId::FONT_WEIGHT),  is_relative_(true), weight_(400), relative_(r) {}
		explicit FontWeight(int fw) : Style(StyleId::FONT_WEIGHT), is_relative_(false), weight_(fw), relative_(FontWeightRelative::LIGHTER) {}
		void setRelative(FontWeightRelative r) { is_relative_ = true; relative_ = r; }
		void setWeight(int fw) { is_relative_ = false; weight_ = fw; }
		int compute(int fw) const;
		bool isEqual(const StylePtr& style) const override;
	private:
		bool is_relative_;
		int weight_;
		FontWeightRelative relative_;
	};

	enum class TextAlign {
		// normal is the default value that acts as 'left' if direction=ltr and
		// 'right' if direction='rtl'.
		NORMAL,
		LEFT,
		RIGHT,
		CENTER,
		JUSTIFY,
	};

	enum class Direction {
		LTR,
		RTL,
	};

	enum class TextTransform {
		NONE,
		CAPITALIZE,
		UPPERCASE,
		LOWERCASE,
	};

	enum class Overflow {
		VISIBLE,
		HIDDEN,
		SCROLL,
		CLIP,
		AUTO,
	};

	enum class Position {
		STATIC,
		RELATIVE,
		ABSOLUTE,
		FIXED,
	};

	enum class BackgroundRepeat {
		REPEAT,
		REPEAT_X,
		REPEAT_Y,
		NO_REPEAT,
	};

	class BackgroundPosition : public Style
	{
	public:
		MAKE_FACTORY(BackgroundPosition);
		BackgroundPosition();
		void setLeft(const Length& left);
		void setTop(const Length& top);
		const Length& getLeft() const { return left_; }
		const Length& getTop() const { return top_; }
		bool isEqual(const StylePtr& style) const override;
	private:
		Length left_;
		Length top_;
	};

	enum class ListStyleType {
		DISC,
		CIRCLE,
		SQUARE,
		DECIMAL,
		DECIMAL_LEADING_ZERO,
		LOWER_ROMAN,
		UPPER_ROMAN,
		LOWER_GREEK,
		LOWER_LATIN,
		UPPER_LATIN,
		ARMENIAN,
		GEORGIAN,
		LOWER_ALPHA,
		UPPER_ALPHA,
		NONE,
	};

	enum class BackgroundAttachment {
		SCROLL,
		FIXED,
	};

	enum class Clear {
		NONE,
		LEFT,
		RIGHT,
		BOTH,
	};

	class Clip : public Style
	{
	public:
		MAKE_FACTORY(Clip);
		Clip() : Style(StyleId::CLIP), auto_(true), rect_() {}
		explicit Clip(xhtml::FixedPoint left, xhtml::FixedPoint top, xhtml::FixedPoint right, xhtml::FixedPoint bottom) 
			: Style(StyleId::CLIP),
			  auto_(false), 
			  rect_(left, top, right, bottom) 
		{
		}
		bool isAuto() const { return auto_; }
		const xhtml::Rect& getRect() const { return rect_; }
		void setRect(const xhtml::Rect& r) { rect_ = r; auto_ = false; }
		void setRect(xhtml::FixedPoint left, xhtml::FixedPoint top, xhtml::FixedPoint right, xhtml::FixedPoint bottom) { 
			rect_.x = left;
			rect_.y = top;
			rect_.width = right;
			rect_.height = bottom;
			auto_ = false; 
		}
		bool isEqual(const StylePtr& style) const override;
	private:
		bool auto_;
		xhtml::Rect rect_;
	};

	enum class CssContentType {
		STRING,
		URI,
		COUNTER,
		COUNTERS,
		OPEN_QUOTE,
		CLOSE_QUOTE,
		NO_OPEN_QUOTE,
		NO_CLOSE_QUOTE,
		ATTRIBUTE,
	};

	// encapsulates one kind of content.
	class ContentType
	{
	public:
		explicit ContentType(CssContentType type);
		explicit ContentType(CssContentType type, const std::string& name);
		explicit ContentType(ListStyleType lst, const std::string& name);
		explicit ContentType(ListStyleType lst, const std::string& name, const std::string& sep);
	private:
		CssContentType type_;
		std::string str_;
		std::string uri_;
		std::string counter_name_;
		std::string counter_seperator_;
		ListStyleType counter_style_;
		std::string attr_;
	};

	class Content : public Style
	{
	public:
		MAKE_FACTORY(Content);
		Content() : Style(StyleId::CONTENT), content_() {}	// means none/normal
		explicit Content(const std::vector<ContentType>& content) : Style(StyleId::CONTENT), content_(content) {}
		void setContent(const std::vector<ContentType>& content) { content_ = content; }
		bool isEqual(const StylePtr& style) const override;
	private:
		std::vector<ContentType> content_;
	};

	// used for incrementing and resetting.
	class Counter : public Style
	{
	public:
		MAKE_FACTORY(Counter);
		Counter() : Style(StyleId::COUNTERS), counters_() {}
		explicit Counter(const std::vector<std::pair<std::string,int>>& counters) : Style(StyleId::COUNTERS), counters_(counters) {}
		const std::vector<std::pair<std::string,int>>& getCounters() const { return counters_; }
		bool isEqual(const StylePtr& style) const override;
	private:
		std::vector<std::pair<std::string,int>> counters_;
	};

	enum class CssCursor {
		AUTO,
		CROSSHAIR,
		DEFAULT,
		POINTER,
		MOVE,
		E_RESIZE,
		NE_RESIZE,
		NW_RESIZE,
		N_RESIZE,
		SE_RESIZE,
		SW_RESIZE,
		S_RESIZE,
		W_RESIZE,
		TEXT,
		WAIT,
		PROGRESS,
		HELP,
	};

	class Cursor : public Style
	{
	public:
		MAKE_FACTORY(Cursor);
		Cursor() : Style(StyleId::CURSOR), uris_(), cursor_(CssCursor::AUTO) {}
		explicit Cursor(CssCursor c) : Style(StyleId::CURSOR), uris_(), cursor_(c) {}
		explicit Cursor(const std::vector<ImageSourcePtr>& uris, CssCursor c) : Style(StyleId::CURSOR), uris_(uris), cursor_(c) {}
		void setURI(const std::vector<ImageSourcePtr>& uris) { uris_ = uris; }
		void setCursor(CssCursor c) { cursor_ = c; }
		bool isEqual(const StylePtr& style) const override;
	private:
		std::vector<std::shared_ptr<ImageSource>> uris_;
		CssCursor cursor_;
	};

	enum class ListStylePosition {
		INSIDE,
		OUTSIDE,
	};

	typedef std::pair<std::string, std::string> quote_pair;
	class Quotes : public Style
	{
	public:
		MAKE_FACTORY(Quotes);
		Quotes() : Style(StyleId::QUOTES), quotes_() {}
		explicit Quotes(const std::vector<quote_pair> quotes) : Style(StyleId::QUOTES), quotes_(quotes) {}
		bool isNone() const { return quotes_.empty(); }
		const std::vector<quote_pair>& getQuotes() const { return quotes_; }
		const quote_pair& getQuotesAtLevel(int n) { 
			if(n < 0) {
				static quote_pair no_quotes;
				return no_quotes;
			}
			if(n < static_cast<int>(quotes_.size())) {
				return quotes_[n];
			}
			return quotes_.back();
		};
		bool isEqual(const StylePtr& style) const override;
	private:
		std::vector<quote_pair> quotes_;
	};

	enum class TextDecoration {
		NONE,
		UNDERLINE,
		OVERLINE,
		LINE_THROUGH,
		BLINK, // N.B. We will not support blinking text.
	};

	enum class UnicodeBidi {
		NORMAL,
		EMBED,
		BIDI_OVERRIDE,
	};

	enum class CssVerticalAlign {
		BASELINE,
		SUB,
		SUPER,
		TOP,
		TEXT_TOP,
		MIDDLE,
		BOTTOM,
		TEXT_BOTTOM,
		
		LENGTH,
	};

	class VerticalAlign : public Style
	{
	public:
		MAKE_FACTORY(VerticalAlign);
		VerticalAlign() : Style(StyleId::VERTICAL_ALIGN), va_(CssVerticalAlign::BASELINE), len_() {}
		explicit VerticalAlign(CssVerticalAlign va) : Style(StyleId::VERTICAL_ALIGN), va_(va), len_() {}
		explicit VerticalAlign(Length len) : Style(StyleId::VERTICAL_ALIGN), va_(CssVerticalAlign::LENGTH), len_(len) {}
		void setAlign(CssVerticalAlign va) { va_ = va; }
		void setLength(const Length& len) { len_ = len; va_ = CssVerticalAlign::LENGTH; }
		const Length& getLength() const { return len_; }
		CssVerticalAlign getAlign() const { return va_; }
		bool isEqual(const StylePtr& style) const override;
	private:
		CssVerticalAlign va_;
		Length len_;
	};

	enum class Visibility {
		VISIBLE,
		HIDDEN,
		COLLAPSE,
	};

	class Zindex : public Style
	{
	public:
		MAKE_FACTORY(Zindex);
		Zindex() : Style(StyleId::ZINDEX), auto_(true), index_(0) {}
		explicit Zindex(int n) : Style(StyleId::ZINDEX), auto_(false), index_(n) {}
		void setIndex(int index) { index_ = index; auto_ = false; }
		bool isAuto() const { return auto_; }
		int getIndex() const { return index_; }
		bool isEqual(const StylePtr& style) const override;
	private:
		bool auto_;
		int index_;
	};

	class BoxShadow
	{
	public:
		BoxShadow();
		explicit BoxShadow(bool inset, const Length& x, const Length& y, const Length& blur, const Length& spread, const CssColor& color);
		bool inset() const { return inset_; }
		const Length& getX() const { return x_offset_; }
		const Length& getY() const { return y_offset_; }
		const Length& getBlur() const { return blur_radius_; }
		const Length& getSpread() const { return spread_radius_; }
		const CssColor& getColor() const { return color_; }
	private:
		bool inset_;
		Length x_offset_;
		Length y_offset_;
		Length blur_radius_;
		Length spread_radius_;
		CssColor color_;
	};

	class BoxShadowStyle : public Style
	{
	public:
		MAKE_FACTORY(BoxShadowStyle);
		BoxShadowStyle() : Style(StyleId::BOX_SHADOW), shadows_() {}
		explicit BoxShadowStyle(const std::vector<BoxShadow>& shadows) : Style(StyleId::BOX_SHADOW), shadows_(shadows) {}
		void setShadows(const std::vector<BoxShadow>& shadows) { shadows_ = shadows; }
		const std::vector<BoxShadow>& getShadows() const { return shadows_; }
		bool isEqual(const StylePtr& style) const override;
	private:
		std::vector<BoxShadow> shadows_;
	};

	enum class CssBorderImageRepeat {
		STRETCH,
		REPEAT,
		ROUND,
		SPACE,
	};
	
	struct BorderImageRepeat : public Style
	{
		MAKE_FACTORY(BorderImageRepeat);
		BorderImageRepeat() : Style(StyleId::BORDER_IMAGE_REPEAT), image_repeat_horiz_(CssBorderImageRepeat::STRETCH), image_repeat_vert_(CssBorderImageRepeat::STRETCH) {}
		explicit BorderImageRepeat(CssBorderImageRepeat image_repeat_h, CssBorderImageRepeat image_repeat_v) : Style(StyleId::BORDER_IMAGE_REPEAT), image_repeat_horiz_(image_repeat_h), image_repeat_vert_(image_repeat_v) {}
		CssBorderImageRepeat image_repeat_horiz_;
		CssBorderImageRepeat image_repeat_vert_;
		bool isEqual(const StylePtr& style) const override;
	};

	class WidthList : public Style
	{
	public:
		MAKE_FACTORY(WidthList);
		WidthList() : Style(StyleId::WIDTH_LIST), widths_{} {}
		explicit WidthList(float value);
		explicit WidthList(const std::vector<Width>& widths);
		void setWidths(const std::vector<Width>& widths);
		const std::array<Width,4>& getWidths() const { return widths_; }
		const Width& getTop() const { return widths_[0]; }
		const Width& getLeft() const { return widths_[1]; }
		const Width& getBottom() const { return widths_[2]; }
		const Width& getRight() const { return widths_[3]; }
		bool isEqual(const StylePtr& style) const override;
	private:
		std::array<Width,4> widths_;
	};

	class BorderImageSlice : public Style
	{
	public:
		MAKE_FACTORY(BorderImageSlice);
		BorderImageSlice() : Style(StyleId::BORDER_IMAGE_SLICE), slices_{}, fill_(false) {}
		explicit BorderImageSlice(const std::vector<Width>& widths, bool fill);
		bool isFilled() const { return fill_; }
		void setWidths(const std::vector<Width>& widths);
		const std::array<Width,4>& getWidths() const { return slices_; }
		bool isEqual(const StylePtr& style) const override;
	private:
		std::array<Width,4> slices_;
		bool fill_;
	};

	class BorderRadius : public Style
	{
	public:
		MAKE_FACTORY(BorderRadius);
		BorderRadius() : Style(StyleId::BORDER_RADIUS), horiz_(0, false), vert_(0, false) {}
		explicit BorderRadius(const Length& horiz, const Length& vert) : Style(StyleId::BORDER_RADIUS), horiz_(horiz), vert_(vert) {}
		const Length& getHoriz() const { return horiz_; }
		const Length& getVert() const { return vert_; }
		bool isEqual(const StylePtr& style) const override;
	private:
		Length horiz_;
		Length vert_;
	};

	enum class BackgroundClip {
		BORDER_BOX,
		PADDING_BOX,
		CONTENT_BOX,
	};

	class TransitionProperties : public Style
	{
	public:
		MAKE_FACTORY(TransitionProperties);
		TransitionProperties() : Style(StyleId::TRANSITION_PROPERTIES), properties_() {}
		explicit TransitionProperties(const std::vector<Property>& p) : Style(StyleId::TRANSITION_PROPERTIES), properties_(p) {}
		const std::vector<Property>& getProperties() { return properties_; }
		bool isEqual(const StylePtr& style) const override;
	private:
		std::vector<Property> properties_;
	};

	// for delays and duration
	class TransitionTiming : public Style
	{
	public:
		MAKE_FACTORY(TransitionTiming);
		TransitionTiming() : Style(StyleId::TRANSITION_TIMING), timings_() {}
		explicit TransitionTiming(const std::vector<float>& timings) : Style(StyleId::TRANSITION_TIMING), timings_(timings) {}
		const std::vector<float>& getTiming() const { return timings_; }
		bool isEqual(const StylePtr& style) const override;
	private:
		std::vector<float> timings_;
	};

	class TransitionTimingFunctions : public Style
	{
	public:
		MAKE_FACTORY(TransitionTimingFunctions);
		TransitionTimingFunctions() : Style(StyleId::TRANSITION_TIMING_FUNCTION), ttfns_() {}
		explicit TransitionTimingFunctions(const std::vector<TimingFunction>& ttfns) : Style(StyleId::TRANSITION_TIMING_FUNCTION), ttfns_(ttfns) {}
		const std::vector<TimingFunction>& getTimingFunctions() { return ttfns_; }
		bool isEqual(const StylePtr& style) const override;
	private:
		std::vector<TimingFunction> ttfns_;
	};

	class TextShadow
	{
	public:
		TextShadow() : color_(), offset_{}, blur_radius_(0, LengthUnits::PX) {}
		explicit TextShadow(const Length& offset_x, const Length& offset_y);
		explicit TextShadow(const Length& offset_x, const Length& offset_y, const CssColor& color, const Length& blur);
		explicit TextShadow(const std::vector<Length>& len, const CssColor& color);
		void setColor(const CssColor& color) { color_ = color; }
		void setBlur(const Length& radius) { blur_radius_ = radius; }
		const std::array<Length, 2>& getOffset() const { return offset_; }
		const CssColor& getColor() const { return color_; }
		const Length& getBlur() const { return blur_radius_; }
	private:
		CssColor color_;
		std::array<Length, 2> offset_;
		Length blur_radius_;
	};

	class TextShadowStyle : public Style
	{
	public:
		MAKE_FACTORY(TextShadowStyle);
		TextShadowStyle() : Style(StyleId::TEXT_SHADOW), shadows_() {}
		explicit TextShadowStyle(const std::vector<TextShadow>& shadows) : Style(StyleId::TEXT_SHADOW), shadows_(shadows) {}
		const std::vector<TextShadow>& getShadows() const { return shadows_; }
	private:
		std::vector<TextShadow> shadows_;
	};
}
