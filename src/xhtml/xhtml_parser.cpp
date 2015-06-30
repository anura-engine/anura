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

#include <map>

#include <boost/property_tree/xml_parser.hpp>

#include "asserts.hpp"
#include "xhtml_element.hpp"
#include "xhtml_text_node.hpp"
#include "xhtml_parser.hpp"

#include "unit_test.hpp"

namespace xhtml
{
	namespace 
	{
		const std::string XmlAttr = "<xmlattr>";
		const std::string XmlText = "<xmltext>";
	}

	using namespace boost::property_tree;

	class ParserNode;
	typedef std::shared_ptr<ParserNode> ParserNodePtr;

	class ParserAttribute;
	typedef std::shared_ptr<ParserAttribute> ParserAttributePtr;

	class ParserAttribute
	{
	public:
		ParserAttribute(const std::string& name, const std::string& value) 
			: name_(name),
			  value_(value)
		{
		}
		AttributePtr createAttribute() {
			return Attribute::create(name_, value_);
		}
	private:
		std::string name_;
		std::string value_;
	};

	class ParserNode
	{
	public:
		ParserNode(const std::string& name, ptree& pt)
			: name_(name)
		{
			if(name_ == XmlText) {
				value_ = pt.data();
			}
			for(auto& element : pt) {
				if(element.first == XmlAttr) {
					for(auto& attr : element.second) {
						attributes_[attr.first] = std::make_shared<ParserAttribute>(attr.first, attr.second.data());
					}
				} else {
					children_.emplace_back(std::make_shared<ParserNode>(element.first, element.second));
				}
			}
		}
		NodePtr createNode(const DocumentPtr& owner_doc) {
			NodePtr node;
			if(name_ == XmlText) {
				node = Text::create(value_, owner_doc);
			} else {
				node = Element::create(name_, owner_doc);
			}
			for(auto& a : attributes_) {
				node->addAttribute(a.second->createAttribute());
			}
			for(auto& c : children_) {
				node->addChild(c->createNode(owner_doc));
			}
			node->init();
			return node;
		}
	private:
		std::string name_;
		std::string value_;
		std::vector<ParserNodePtr> children_;
		std::map<std::string, ParserAttributePtr> attributes_;
	};

	DocumentFragmentPtr parse_from_file(const std::string& filename, const DocumentPtr& owner_doc)
	{
		std::vector<ParserNodePtr> nodes;
		try {
			ptree pt;
			read_xml(filename, pt, xml_parser::no_concat_text);
			for(auto& element : pt) {
				nodes.emplace_back(std::make_shared<ParserNode>(element.first, element.second));
			}
		} catch(ptree_error& e) {
			ASSERT_LOG(false, "Error parsing XHTML: " << e.what() << " : " << filename);
		}
		auto frag = DocumentFragment::create();
		for(auto& pn : nodes) {
			frag->addChild(pn->createNode(owner_doc), owner_doc);
		}
		return frag;
	}

	DocumentFragmentPtr parse_from_string(const std::string& str, const DocumentPtr& owner_doc)
	{
		if(str.empty()) {
			LOG_ERROR("parse_from_string No string data to parse.");
			return nullptr;
		}
		std::vector<ParserNodePtr> nodes;
		std::istringstream markup(str);
		try {
			ptree pt;
			read_xml(markup, pt, xml_parser::no_concat_text);
			for(auto& element : pt) {
				nodes.emplace_back(std::make_shared<ParserNode>(element.first, element.second));
			}
		} catch(ptree_error& e) {
			ASSERT_LOG(false, "Error parsing XHTML: " << e.what() << " : " << str);
		}
		auto frag = DocumentFragment::create();
		for(auto& pn : nodes) {
			frag->addChild(pn->createNode(owner_doc), owner_doc);
		}		
		return frag;
	}
}

UNIT_TEST(xhtml_node_tests)
{
	auto frag = xhtml::parse_from_string("<em> \n \n \n <![CDATA[this is some text!!!!]]> \n \n \n</em>", nullptr);
	LOG_DEBUG("children in fragment: " << frag->getChildren().size());
	frag->normalize();
	LOG_DEBUG("children in fragment (after normalize): " << frag->getChildren().size());
}
