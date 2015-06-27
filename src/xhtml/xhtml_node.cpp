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

#include <sstream>

#include "asserts.hpp"
#include "css_parser.hpp"
#include "xhtml_text_node.hpp"
#include "xhtml_render_ctx.hpp"

#include "filesystem.hpp"

namespace xhtml
{
	namespace {
		struct DocumentImpl : public Document 
		{
			DocumentImpl(css::StyleSheetPtr ss) : Document(ss) {}
		};

		struct DocumentFragmentImpl : public DocumentFragment
		{
			DocumentFragmentImpl(WeakDocumentPtr owner) : DocumentFragment(owner) {}
		};

		struct AttributeImpl : public Attribute
		{
			AttributeImpl(const std::string& name, const std::string& value, WeakDocumentPtr owner) : Attribute(name, value, owner) {}
		};
	}

	Node::Node(NodeId id, WeakDocumentPtr owner)
		: id_(id),
		  children_(),
		  attributes_(),
		  left_(),
		  right_(),
		  parent_(),
		  owner_document_(owner),
		  properties_(),
		  pclass_(css::PseudoClass::NONE),
		  active_pclass_(css::PseudoClass::NONE),
		  active_rect_(),
		  dimensions_()
	{
	}

	Node::~Node()
	{
	}
	
	void Node::addChild(NodePtr child)
	{		
		if(child->id() == NodeId::DOCUMENT_FRAGMENT) {
			// we add the children of a document fragment rather than the node itself.
			if(children_.empty()) {
				children_ = child->children_;
				for(auto& c : children_) {
					c->setParent(shared_from_this());
				}
			} else {
				if(!child->children_.empty()) {
					children_.back()->right_ = child->children_.front();
					child->children_.front()->left_ = children_.back();
					for(auto& c : child->children_) {
						c->setParent(shared_from_this());
					}
					children_.insert(children_.end(), child->children_.begin(), child->children_.end());
				}
			}
		} else {
			child->left_ = child->right_ = std::weak_ptr<Node>();
			if(!children_.empty()) {
				children_.back()->right_ = child;
				child->left_ = children_.back();
			}
			children_.emplace_back(child);
			child->setParent(shared_from_this());
		}
	}

	void Node::removeChild(NodePtr child)
	{
		if(child->getParent() == shared_from_this()) {
			if(children_.size() == 1) {
				children_.clear();
			} else {
				children_.erase(std::remove_if(children_.begin(), children_.end(), [child](NodePtr p){ return p == child; }), children_.end());
				auto left = child->left_.lock();
				if(left != nullptr) {
					left->right_ = child->right_;
				}
				auto right = child->right_.lock();
				if(right != nullptr) {
					right->left_ = child->left_;
				}
			}			
			child->left_ = child->right_ = std::weak_ptr<Node>();
		} else {
			ASSERT_LOG(false, "Tried to remove child node which doesn't belong to us.");
		}
	}

	void Node::addAttribute(AttributePtr a)
	{
		a->setParent(shared_from_this());
		attributes_[a->getName()] = a;
	}

	bool Node::preOrderTraversal(std::function<bool(NodePtr)> fn)
	{
		// Visit node, visit children.
		if(!fn(shared_from_this())) {
			return false;
		}
		for(auto& c : children_) {
			if(!c->preOrderTraversal(fn)) {
				return false;
			}
		}
		return true;
	}

	bool Node::postOrderTraversal(std::function<bool(NodePtr)> fn)
	{
		// Visit children, then this process node.
		for(auto& c : children_) {
			if(!c->preOrderTraversal(fn)) {
				return false;
			}
		}
		if(!fn(shared_from_this())) {
			return false;
		}
		return true;
	}

	bool Node::ancestralTraverse(std::function<bool(NodePtr)> fn)
	{
		if(fn(shared_from_this())) {
			return true;
		}
		auto parent = getParent();
		if(parent != nullptr) {
			return parent->ancestralTraverse(fn);
		}
		return false;
	}

	AttributePtr Node::getAttribute(const std::string& name)
	{
		auto it = attributes_.find(name);
		return it != attributes_.end() ? it->second : nullptr;
	}

	std::string Node::nodeToString() const
	{
		std::ostringstream ss;
		for(auto& a : getAttributes()) {
			ss << "{" << a.second->toString() << "}";
		}
		return ss.str();
	}

	const std::string& Node::getValue() const
	{
		static std::string null_str;
		return null_str;
	}

	void Node::processWhitespace()
	{
		using namespace css;
		StylePtr style = properties_.getProperty(Property::WHITE_SPACE);
		Whitespace ws = style != nullptr ? style->getEnum<Whitespace>() : Whitespace::NORMAL;
		bool collapse_whitespace = ws == Whitespace::NORMAL || ws == Whitespace::NOWRAP || ws == Whitespace::PRE_LINE;
		if(collapse_whitespace) {
			std::vector<NodePtr> removal_list;
			for(auto& child : children_) {
				if(child->id() == NodeId::TEXT) {
					auto& txt = child->getValue();
					bool non_empty = false;
					for(auto& ch : txt) {
						if(ch != '\t' && ch != '\r' && ch != '\n' && ch != ' ') {
							non_empty = true;
						}
					}
					if(!non_empty) {
						removal_list.emplace_back(child);
					}
				}
			}
			for(auto& child : removal_list) {
				removeChild(child);
			}
		}

		for(auto& child : children_) {
			child->processWhitespace();
		}
	}


	void Node::inheritProperties()
	{
		NodePtr parent = getParent();
		ASSERT_LOG(parent != nullptr, "Node::inheritProperties: parent was null.");
		properties_ = parent->getProperties();
	}

	NodePtr Node::getElementById(const std::string& ident)
	{
		if(id() == NodeId::ELEMENT) {
			auto attr = getAttribute("id");
			if(attr != nullptr && attr->getValue() == ident) {
				return shared_from_this();
			}
		}
		for(auto& child : children_) {
			auto node = child->getElementById(ident);
			if(node != nullptr) {
				return node;
			}
		}
		return nullptr;
	}

	void Node::normalize()
	{
		std::vector<NodePtr> new_child_list;
		TextPtr new_text_node;
		for(auto& c : children_) {
			if(c->id() == NodeId::TEXT) {
				if(!c->getValue().empty()) {
					if(new_text_node) {
						new_text_node->addText(c->getValue());
					} else {
						new_text_node = Text::create(c->getValue(), owner_document_);
					}
				}
			} else {
				if(new_text_node) {
					new_child_list.emplace_back(new_text_node);
					new_text_node.reset();
				}
				new_child_list.emplace_back(c);
			}
		}
		if(new_text_node != nullptr) {
			new_child_list.emplace_back(new_text_node);
		}
		children_ = new_child_list;
		for(auto& c : children_) {
			c->normalize();
		}
	}

	bool Node::handleMouseButtonUp(bool* trigger, const point& p)
	{
		if(!handleMouseButtonUpInt(trigger, p)) {
			return false;
		}
		// XXX
		return true;
	}

	bool Node::handleMouseButtonDown(bool* trigger, const point& p)
	{
		if(!handleMouseButtonDownInt(trigger, p)) {
			return false;
		}
		// XXX
		return true;
	}

	bool Node::handleMouseMotion(bool* trigger, const point& p)
	{
		if(!handleMouseMotionInt(trigger, p)) {
			return false;
		}
		bool hover = hasPseudoClass(css::PseudoClass::HOVER);
		if(!hover || active_rect_.empty()) {
			return true;
		}
		if(geometry::pointInRect(p, active_rect_)) {
			if((active_pclass_ & css::PseudoClass::HOVER) != css::PseudoClass::HOVER) {
				active_pclass_ = active_pclass_ | css::PseudoClass::HOVER;
				*trigger = true;
			}
			return true;
		} else if((active_pclass_ & css::PseudoClass::HOVER) == css::PseudoClass::HOVER) {
			active_pclass_ = active_pclass_ & ~css::PseudoClass::HOVER;
			*trigger = true;
		}
		return true;
	}

	bool Document::handleMouseMotion(bool claimed, int x, int y)
	{		
		bool trigger = false;
		point p(x, y);
		claimed = !preOrderTraversal([&trigger, &p](NodePtr node) {
			node->handleMouseMotion(&trigger, p);
			return true;
		});
		trigger_layout_ |= trigger;
		return claimed;
	}

	bool Document::handleMouseButtonDown(bool claimed, int x, int y, unsigned button)
	{
		bool trigger = false;
		point p(x, y);
		claimed = !preOrderTraversal([&trigger, &p](NodePtr node) {
			node->handleMouseButtonDown(&trigger, p);
			return true;
		});
		trigger_layout_ |= trigger;
		return claimed;
	}

	bool Document::handleMouseButtonUp(bool claimed, int x, int y, unsigned button)
	{
		bool trigger = false;
		point p(x, y);
		claimed = !preOrderTraversal([&trigger, &p](NodePtr node) {
			node->handleMouseButtonUp(&trigger, p);
			return true;
		});
		trigger_layout_ |= trigger;
		return claimed;
	}

	// Documents do not have an owner document.
	Document::Document(css::StyleSheetPtr ss)
		: Node(NodeId::DOCUMENT, WeakDocumentPtr()),
		  style_sheet_(ss == nullptr ? std::make_shared<css::StyleSheet>() : ss),
		  trigger_layout_(true)
	{
	}

	void Document::processStyles()
	{
		// parse all the style nodes into the style sheet.
		auto ss = style_sheet_;
		preOrderTraversal([&ss](NodePtr n) {
			if(n->hasTag(ElementId::STYLE)) {
				for(auto& child : n->getChildren()) {
					if(child->id() == NodeId::TEXT) {						
						css::Parser::parse(ss, child->getValue());
					}
				}
			}
			if(n->hasTag(ElementId::LINK)) {
				auto rel = n->getAttribute("rel");
				auto href = n->getAttribute("href");
				auto type = n->getAttribute("type");		// expect "type/css"
				//auto media = n->getAttribute("media");	// expect "display" or nullptr
				if(rel && rel->getValue() == "stylesheet") {
					if(href == nullptr) {
						LOG_ERROR("There was no 'href' in the LINK element.");
					} else {
						//auto css_file = get_uri(href->getValue);
						// XXX add a fix for getting data directory,
						auto css_file = sys::read_file("../data/" + href->getValue());
						css::Parser::parse(ss, css_file);
					}
				}
			}
			return true;
		});
		
		processStyleRules();
	}

	void Document::processStyleRules()
	{
		auto& ss = style_sheet_;
		preOrderTraversal([&ss](NodePtr n) {
			ss->applyRulesToElement(n);
			return true;
		});

		// Parse and apply specific element style rules from attributes here.
		preOrderTraversal([](NodePtr n) {
			if(n->id() == NodeId::ELEMENT) {
				// XXX: we should cache this and only re-parse if it changes.
				auto attr = n->getAttribute("style");
				if(attr) {
					auto plist = css::Parser::parseDeclarationList(attr->getValue());
					css::Specificity specificity = {9999, 9999, 9999};
					n->mergeProperties(specificity, plist);
				}
			}
			return true;
		});
	}

	void Node::mergeProperties(const css::Specificity& specificity, const css::PropertyList& plist)
	{
		properties_.merge(specificity, plist);
	}

	std::string Document::toString() const 
	{
		std::ostringstream ss;
		ss << "Document(" << nodeToString() << ")";
		return ss.str();
	}

	DocumentPtr Document::create(css::StyleSheetPtr ss)
	{
		return std::make_shared<DocumentImpl>(ss);
	}

	DocumentFragment::DocumentFragment(WeakDocumentPtr owner)
		: Node(NodeId::DOCUMENT_FRAGMENT, owner)
	{
	}

	DocumentFragmentPtr DocumentFragment::create(WeakDocumentPtr owner)
	{
		return std::make_shared<DocumentFragmentImpl>(owner);
	}

	std::string DocumentFragment::toString() const 
	{
		std::ostringstream ss;
		ss << "DocumentFragment(" << nodeToString() << ")";
		return ss.str();
	}

	Attribute::Attribute(const std::string& name, const std::string& value, WeakDocumentPtr owner)
		: Node(NodeId::ATTRIBUTE, owner),
		  name_(name),
		  value_(value)
	{
	}

	AttributePtr Attribute::create(const std::string& name, const std::string& value, WeakDocumentPtr owner)
	{
		return std::make_shared<AttributeImpl>(name, value, owner);
	}

	std::string Attribute::toString() const 
	{
		std::ostringstream ss;
		ss << "Attribute('" << name_ << ":" << value_ << "'" << nodeToString() << ")";
		return ss.str();
	}
}
