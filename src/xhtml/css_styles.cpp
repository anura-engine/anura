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
		return is_relative_ == p->is_relative_ ? is_relative_ ? relative_ == p->relative_ : weight_ == weight_ : false;
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
