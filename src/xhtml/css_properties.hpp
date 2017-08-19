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

#include <functional>

#include "css_styles.hpp"
#include "css_lexer.hpp"

namespace css
{
	struct PropertyInfo
	{
		PropertyInfo() : name(), inherited(false), obj(), is_default(false) {}
		PropertyInfo(const std::string& n, bool inh, StylePtr def) : name(n), inherited(inh), obj(def), is_default(false) {}
		std::string name;
		bool inherited;
		StylePtr obj;
		bool is_default;
	};

	class PropertyList
	{
	public:
		struct PropertyStyle {
			PropertyStyle() : style(nullptr), specificity() {}
			explicit PropertyStyle(StylePtr s, const Specificity& sp) : style(s), specificity(sp) {}
			StylePtr style;
			Specificity specificity;
		};

		typedef std::map<Property, PropertyStyle>::iterator iterator;
		typedef std::map<Property, PropertyStyle>::const_iterator const_iterator;
		PropertyList();
		void addProperty(Property p, StylePtr o, const Specificity& specificity=Specificity());
		void addProperty(const std::string& name, StylePtr o);
		StylePtr getProperty(Property p) const;
		bool hasProperty(Property p) const { return properties_.find(p) != properties_.end(); }
		void merge(const Specificity& specificity, const PropertyList& plist);
		void clear() { properties_.clear(); }
		iterator begin() { return properties_.begin(); }
		iterator end() { return properties_.end(); }
		const_iterator begin() const { return properties_.cbegin(); }
		const_iterator end() const { return properties_.cend(); }
		bool empty() const { return properties_.empty(); }
		void markTransitions();
	private:
		std::map<Property, PropertyStyle> properties_;
	};

	class PropertyParser
	{
	public:
		typedef std::vector<TokenPtr>::const_iterator const_iterator;
		PropertyParser();
		const_iterator parse(const std::string& name, const const_iterator& begin, const const_iterator& end);
		void inheritProperty(const std::string& name);
		const PropertyList& getPropertyList() const { return plist_; }
		PropertyList& getPropertyList() { return plist_; }
		void parseColor(const std::string& prefix, const std::string& suffix);
		void parseColorList(const std::string& prefix, const std::string& suffix);
		void parseWidth(const std::string& prefix, const std::string& suffix);
		void parseLength(const std::string& prefix, const std::string& suffix);
		void parseWidthList(const std::string& prefix, const std::string& suffix);
		void parseLengthList(const std::string& prefix, const std::string& suffix);
		void parseBorderWidth(const std::string& prefix, const std::string& suffix);
		void parseBorderWidthList(const std::string& prefix, const std::string& suffix);
		void parseBorderStyle(const std::string& prefix, const std::string& suffix);
		void parseBorderStyleList(const std::string& prefix, const std::string& suffix);
		void parseDisplay(const std::string& prefix, const std::string& suffix);
		void parseWhitespace(const std::string& prefix, const std::string& suffix);
		void parseFontFamily(const std::string& prefix, const std::string& suffix);
		void parseFontSize(const std::string& prefix, const std::string& suffix);
		void parseFontWeight(const std::string& prefix, const std::string& suffix);
		void parseSpacing(const std::string& prefix, const std::string& suffix);
		void parseTextAlign(const std::string& prefix, const std::string& suffix);
		void parseDirection(const std::string& prefix, const std::string& suffix);
		void parseTextTransform(const std::string& prefix, const std::string& suffix);
		void parseLineHeight(const std::string& prefix, const std::string& suffix);
		void parseFontStyle(const std::string& prefix, const std::string& suffix);
		void parseFontVariant(const std::string& prefix, const std::string& suffix);
		void parseOverflow(const std::string& prefix, const std::string& suffix);
		void parsePosition(const std::string& prefix, const std::string& suffix);
		void parseFloat(const std::string& prefix, const std::string& suffix);
		void parseImageSource(const std::string& prefix, const std::string& suffix);
		void parseBackgroundRepeat(const std::string& prefix, const std::string& suffix);
		void parseBackgroundPosition(const std::string& prefix, const std::string& suffix);
		void parseListStyleType(const std::string& prefix, const std::string& suffix);
		void parseBorder(const std::string& prefix, const std::string& suffix);
		void parseBackgroundAttachment(const std::string& prefix, const std::string& suffix);
		void parseClear(const std::string& prefix, const std::string& suffix);
		void parseClip(const std::string& prefix, const std::string& suffix);
		void parseContent(const std::string& prefix, const std::string& suffix);
		void parseCounter(const std::string& prefix, const std::string& suffix);
		void parseCursor(const std::string& prefix, const std::string& suffix);
		void parseListStylePosition(const std::string& prefix, const std::string& suffix);
		void parseQuotes(const std::string& prefix, const std::string& suffix);
		void parseTextDecoration(const std::string& prefix, const std::string& suffix);
		void parseUnicodeBidi(const std::string& prefix, const std::string& suffix);
		void parseVerticalAlign(const std::string& prefix, const std::string& suffix);
		void parseVisibility(const std::string& prefix, const std::string& suffix);
		void parseZindex(const std::string& prefix, const std::string& suffix);
		void parseBackground(const std::string& prefix, const std::string& suffix);
		void parseListStyle(const std::string& prefix, const std::string& suffix);

		void parseBoxShadow(const std::string& prefix, const std::string& suffix);
		void parseBorderImageRepeat(const std::string& prefix, const std::string& suffix);
		void parseWidthList2(const std::string& prefix, const std::string& suffix);
		void parseBorderImageSlice(const std::string& prefix, const std::string& suffix);
		void parseBorderImage(const std::string& prefix, const std::string& suffix);
		void parseSingleBorderRadius(const std::string& prefix, const std::string& suffix);
		void parseBorderRadius(const std::string& prefix, const std::string& suffix);
		void parseBackgroundClip(const std::string& prefix, const std::string& suffix);
		void parseTextShadow(const std::string& prefix, const std::string& suffix);

		void parseTransition(const std::string& prefix, const std::string& suffix);
		void parseTransitionProperty(const std::string& prefix, const std::string& suffix);
		void parseTransitionTimingFunction(const std::string& prefix, const std::string& suffix);
		void parseTransitionTiming(const std::string& prefix, const std::string& suffix);

		void parseFilters(const std::string& prefix, const std::string& suffix);
		void parseTransform(const std::string& prefix, const std::string& suffix);
	private:
		enum NumericParseOptions {
			NUMBER = 1,
			PERCENTAGE = 2,
			LENGTH = 4,
			AUTO = 8,
			NUMERIC = NUMBER | PERCENTAGE | LENGTH,
			NUMBER_OR_PERCENT = NUMBER | PERCENTAGE,
			LENGTH_OR_PERCENT = LENGTH | PERCENTAGE,
			ALL = NUMBER | PERCENTAGE | LENGTH | AUTO,
		};
		void advance();
		void skipWhitespace();
		bool isEndToken() const;
		bool isToken(TokenId tok) const;
		bool isTokenDelimiter(const std::string& delim) const;
		std::vector<TokenPtr> parseCSVList(TokenId end_token);
		void parseCSVNumberListFromIt(std::vector<TokenPtr>::const_iterator start, std::vector<TokenPtr>::const_iterator end, std::function<void(int, float, bool)> fn);
		void parseCSVNumberList(TokenId end_token, std::function<void(int, float, bool)> fn);
		void parseCSVStringList(TokenId end_token, std::function<void(int, const std::string&)> fn);
		std::shared_ptr<css::CssColor> parseColorInternal();
		Length parseLengthInternal(NumericParseOptions opts=ALL);
		StylePtr parseWidthInternal();
		Width parseWidthInternal2();
		StylePtr parseBorderWidthInternal();
		StylePtr parseBorderStyleInternal();
		ImageSourcePtr parseLinearGradient(const std::vector<TokenPtr>& tokens);
		void parseColor2(std::shared_ptr<CssColor> color);
		ListStyleType parseListStyleTypeInt(const std::string& lst);
		CssBorderImageRepeat parseBorderImageRepeatInteral(const std::string& ref);

		struct IteratorContext
		{
			IteratorContext(PropertyParser& pp, const std::vector<TokenPtr>& toks)
				: pp_(pp),
				  it_it(pp.it_),
				  end_it(pp.end_)
			{
				pp.it_ = toks.cbegin();
				pp.end_ = toks.cend();
			}
			~IteratorContext() 
			{
				pp_.it_ = it_it;
				pp_.end_ = end_it;
			}
			PropertyParser& pp_; 
			const_iterator it_it;
			const_iterator end_it;
		};

		const_iterator it_;
		const_iterator end_;
		PropertyList plist_;
	};

	const std::string& get_property_name(Property p);
	const PropertyInfo& get_default_property_info(Property p);
	Property get_property_by_name(const std::string& name);
}
