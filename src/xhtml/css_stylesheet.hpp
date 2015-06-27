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

#include "xhtml_fwd.hpp"
#include "css_selector.hpp"
#include "css_properties.hpp"

namespace css
{
	struct CssRule
	{
		std::vector<SelectorPtr> selectors;
		PropertyList declaractions;
	};
	typedef std::shared_ptr<CssRule> CssRulePtr;

	class StyleSheet
	{
	public:
		StyleSheet();
		void addRule(const CssRulePtr& rule);
		std::string toString() const;

		const std::vector<CssRulePtr>& getRules() const { return rules_; }
		void applyRulesToElement(xhtml::NodePtr n);
	private:
		std::vector<CssRulePtr> rules_;
	};
	typedef std::shared_ptr<StyleSheet> StyleSheetPtr;
}
