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
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "css_styles.hpp"
#include "xhtml.hpp"
#include "xhtml_element_id.hpp"

namespace css
{
	class Selector;
	typedef std::shared_ptr<Selector> SelectorPtr;

	class Token;
	typedef std::shared_ptr<Token> TokenPtr;

	class FilterSelector;
	typedef std::shared_ptr<FilterSelector> FilterSelectorPtr;

	struct SelectorParseError  : public std::runtime_error
	{
		SelectorParseError(const char* msg) : std::runtime_error(msg) {}
	};

	enum class PseudoClass {
		NONE	= 0,		// Standard
		HOVER	= 1,		// Active when the mouse is over the element.
		ACTIVE	= 2,		// Active when between mouse press and mouse release.
		FOCUS	= 4,		// Active when the element has focus and can accept keyboard events.
		CHECKED = 8,		// Active when input element is checked.
		BEFORE	= 16,		// 
	};

	inline PseudoClass operator|(PseudoClass a, PseudoClass b) {
		return static_cast<PseudoClass>(static_cast<int>(a) | static_cast<int>(b));
	};

	inline PseudoClass operator&(PseudoClass a, PseudoClass b) {
		return static_cast<PseudoClass>(static_cast<int>(a) & static_cast<int>(b));
	};

	inline PseudoClass operator~(PseudoClass a) {
		return static_cast<PseudoClass>(~static_cast<int>(a));
	}

	enum class Combinator {
		NONE,
		CHILD,					// '>'
		DESCENDENT,				// ' '
		SIBLING,				// '+'
	};

	enum class FilterId {
		ID,
		CLASS,
		PSEUDO,
		ATTRIBUTE,
	};

	class FilterSelector
	{
	public:
		explicit FilterSelector(FilterId id);
		virtual ~FilterSelector() {}
		FilterId id() const { return id_; }
		virtual bool match(xhtml::NodePtr element) const = 0;
		virtual std::string toString() const = 0;
		virtual Specificity calculateSpecificity() = 0;
	private:
		FilterId id_;
	};

	class SimpleSelector
	{
	public:
		SimpleSelector();
		bool hasCombinator() const { return combinator_ != Combinator::NONE; }
		void setCombinator(Combinator c) { combinator_ = c; }
		Combinator getCombinator() const { return combinator_; }
		bool match(xhtml::NodePtr element) const;
		void addFilter(FilterSelectorPtr f);
		void setElementId(xhtml::ElementId id);
		xhtml::ElementId getElementId() const { return element_; }
		std::string toString() const;
		const Specificity& getSpecificity() const { return specificity_; }
	private:
		xhtml::ElementId element_;
		std::vector<FilterSelectorPtr> filters_;
		Combinator combinator_;
		Specificity specificity_;
	};
	typedef std::shared_ptr<SimpleSelector> SimpleSelectorPtr;

	class Selector
	{
	public:
		Selector();
		static std::vector<SelectorPtr> parseTokens(const std::vector<TokenPtr>& tokens);
		bool match(xhtml::NodePtr element) const;
		void addSimpleSelector(SimpleSelectorPtr s) { selector_chain_.emplace_back(s); }
		std::string toString() const;
		void calculateSpecificity();
		const Specificity& getSpecificity() const { return specificity_; }
	private:
		std::vector<SimpleSelectorPtr> selector_chain_;
		Specificity specificity_;
	};

	struct SpecificityOrdering
	{
		bool operator()(const Specificity& lhs, const Specificity& rhs) const;
	};
}
