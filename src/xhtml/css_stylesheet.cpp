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

#include "css_stylesheet.hpp"
#include "xhtml_node.hpp"

namespace css
{
	// StyleSheet functions
	StyleSheet::StyleSheet()
		: rules_()
	{
	}

	void StyleSheet::addRule(const CssRulePtr& rule)
	{
		rules_.emplace_back(rule);
		//std::stable_sort(rules_.begin(), rules_.end(), sort_fn);
	}

	std::string StyleSheet::toString() const
	{
		std::ostringstream ss;
		for(auto& r : rules_) {
			for(auto& s : r->selectors) {
				ss << s->toString() << ", ";
			}
			ss << "\n";
			for(auto& d : r->declaractions) {				
				ss << "    " << get_property_name(d.first) << " : " /*<< d.second*/ << "\n";
			}
		}
		return ss.str();
	}

	void StyleSheet::applyRulesToElement(xhtml::NodePtr n)
	{
		if(n->id() == xhtml::NodeId::ELEMENT) {
			n->clearProperties();
			for(auto& r : rules_) {
				for(auto& s : r->selectors) {
					if(s->match(n)) {
						//LOG_DEBUG("merge for node: " << n->toString() << ", selector: " << s->toString() << ", spec: " << s->getSpecificity()[0] << "," << s->getSpecificity()[1] << "," << s->getSpecificity()[2]);
						n->mergeProperties(s->getSpecificity(), r->declaractions);
						break;
					}
				}				
			}
		}
	}
}
