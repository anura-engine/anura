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
#include "xhtml_box.hpp"
#include "xhtml_text_node.hpp"
#include "xhtml_render_ctx.hpp"
#include "xhtml_root_box.hpp"
#include "xhtml_script_interface.hpp"
#include "xhtml_style_tree.hpp"

#include "filesystem.hpp"
#include "profile_timer.hpp"

namespace xhtml
{
	namespace 
	{
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

		typedef std::map<std::string, ScriptPtr> script_map_t;
		script_map_t& get_script_map()
		{
			static script_map_t res;
			return res;
		}

		const std::map<std::string, EventHandlerId>& get_event_handlers() 
		{
			static std::map<std::string, EventHandlerId> res;
			if(res.empty()) {
				//res["onclick"] = EventHandlerId::CLICK;
				//res["ondblclick"] = EventHandlerId::DBL_CLICK;
				res["onmousedown"] = EventHandlerId::MOUSE_DOWN;
				res["onmouseup"] = EventHandlerId::MOUSE_UP;
				res["onmousemove"] = EventHandlerId::MOUSE_MOVE;
				res["onmouseenter"] = EventHandlerId::MOUSE_ENTER;
				res["onmouseleave"] = EventHandlerId::MOUSE_LEAVE;
				//res["onmouseover"] = EventHandlerId::MOUSE_OVER;
				//res["onmouseout"] = EventHandlerId::MOUSE_OUT;
				res["onkeypress"] = EventHandlerId::KEY_PRESS;
				res["onkeyup"] = EventHandlerId::KEY_UP;
				res["onkeydown"] = EventHandlerId::KEY_DOWN;
				res["onload"] = EventHandlerId::LOAD;
				res["onunload"] = EventHandlerId::UNLOAD;
				res["onresize"] = EventHandlerId::RESIZE;
				res["onwheel"] = EventHandlerId::WHEEL;
				//res["onblur"] = EventHandlerId::BLUR;
				//res["onscroll"] = EventHandlerId::SCROLL;
				//res["onfocus"] = EventHandlerId::FOCUS;
				//res["onfocusin"] = EventHandlerId::FOCUSIN;
				//res["onfocusout"] = EventHandlerId::FOCUSOUT;
				//res["onselect"] = EventHandlerId::SELECT;
				//res["onerror"] = EventHandlerId::ERROR;
				//res["oncompositionstart"] = EventHandlerId::COMPOSITIONSTART;
				//res["oncompositionupdate"] = EventHandlerId::COMPOSITIONUPDATE;
				//res["oncompositionend"] = EventHandlerId::COMPOSITIONEND;
				//res["onabort"] = EventHandlerId::ABORT;
				//res["onbeforeinput"] = EventHandlerId::BEFOREINPUT;
				//res["oninput"] = EventHandlerId::INPUT;
				
			}
			return res;
		}
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
		  model_matrix_(1.0f),
		  dimensions_(),
		  script_handler_(nullptr),
		  active_handlers_(),
		  mouse_entered_(false),
		  style_node_()
	{
		active_handlers_.resize(static_cast<int>(EventHandlerId::MAX_EVENT_HANDLERS));
	}

	Node::~Node()
	{
	}
	
	void Node::setActiveHandler(EventHandlerId id, bool active)
	{
		int index = static_cast<int>(id);
		ASSERT_LOG(index < static_cast<int>(active_handlers_.size()), "index exceeds bounds.");
		active_handlers_[index] = active;
	}

	bool Node::hasActiveHandler(EventHandlerId id)
	{
		int index = static_cast<int>(id);
		ASSERT_LOG(index < static_cast<int>(active_handlers_.size()), "index exceeds bounds.");
		return active_handlers_[index];
	}

	void Node::addChild(NodePtr child, const DocumentPtr& owner)
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

	void Node::setAttribute(const std::string& name, const std::string& value)
	{
		attributes_[name] = Attribute::create(name, value, getOwnerDoc());
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

	void Node::processScriptAttributes()
	{
		if(id() == NodeId::ELEMENT) {
			auto handler = Document::findScriptHandler();
			script_handler_ = handler;
			if(handler != nullptr) {
				for(auto& attr : getAttributes()) {
					auto it = get_event_handlers().find(attr.first);
					if(it != get_event_handlers().end()) {
						handler->addEventHandler(shared_from_this(), it->second, attr.second->getValue());
					}
				}
			}
		}
		for(auto& c : children_) {
			c->processScriptAttributes();
		}
	}

	void Node::setScrollbar(const scrollable::ScrollbarPtr& scrollbar)
	{
		ASSERT_LOG(scrollbar != nullptr, "setting a null scrollbar isn't allowed. Use removeScrollbar() instead.");	
		if(scrollbar->getDirection() == scrollable::Scrollbar::Direction::VERTICAL) {
			scrollbar_vert_ = scrollbar;
		} else {
			scrollbar_horz_ = scrollbar;
		}
	}

	void Node::removeScrollbar(scrollable::Scrollbar::Direction d)
	{
		if(d == scrollable::Scrollbar::Direction::VERTICAL) {
			scrollbar_vert_.reset();
		} else {
			scrollbar_horz_.reset();
		}
	}

	bool Node::handleMouseWheel(bool* trigger, const point& p, const point& delta, int direction)
	{
		if(!active_rect_.empty() && geometry::pointInRect(p, active_rect_)) {
			if(scrollbar_vert_ && delta.y != 0) {
				scrollbar_vert_->scrollLines((direction ? -1 : 1) * delta.y);
			} else if(scrollbar_horz_ && delta.x != 0) {
				scrollbar_horz_->scrollLines((direction ? -1 : 1) * delta.x);
			}
		}
		return false;
	}

	bool Node::handleMouseButtonUp(bool* trigger, const point& mp, unsigned button)
	{
		auto pos = model_matrix_ * glm::vec4(static_cast<float>(mp.x), static_cast<float>(mp.y), 0.0f, 1.0f);
		point p(static_cast<int>(pos.x), static_cast<int>(pos.y));
		if(!active_rect_.empty()) {
			if(geometry::pointInRect(p, active_rect_)) {
				if(getScriptHandler() && hasActiveHandler(EventHandlerId::MOUSE_UP)) {
					std::map<variant, variant> m;
					m[variant("clientX")] = variant(p.x);
					m[variant("clientY")] = variant(p.y);
					m[variant("button")] = variant(static_cast<int>(button - 1));
					getScriptHandler()->runEventHandler(shared_from_this(), EventHandlerId::MOUSE_UP, variant(&m));
				}
			}
		}

		if(!handleMouseButtonUpInt(trigger, p)) {
			return false;
		}
		// XXX
		bool focus = hasPseudoClass(css::PseudoClass::FOCUS);
		if(!focus || active_rect_.empty()) {
			return true;
		}
		if(mouse_entered_) {
			if((active_pclass_ & css::PseudoClass::FOCUS) != css::PseudoClass::FOCUS) {
				active_pclass_ = active_pclass_ | css::PseudoClass::FOCUS;
				getOwnerDoc()->setActiveElement(shared_from_this());
				*trigger = true;
			}
			return true;
		} else if((active_pclass_ & css::PseudoClass::FOCUS) == css::PseudoClass::FOCUS) {
			active_pclass_ = active_pclass_ & ~css::PseudoClass::FOCUS;
			getOwnerDoc()->setActiveElement(nullptr);
			*trigger = true;
		}

		return true;
	}

	bool Node::handleMouseButtonDown(bool* trigger, const point& mp, unsigned button)
	{
		auto pos = model_matrix_ * glm::vec4(static_cast<float>(mp.x), static_cast<float>(mp.y), 0.0f, 1.0f);
		point p(static_cast<int>(pos.x), static_cast<int>(pos.y));
		if(!active_rect_.empty()) {
			if(geometry::pointInRect(p, active_rect_)) {
				if(getScriptHandler() && hasActiveHandler(EventHandlerId::MOUSE_DOWN)) {
					std::map<variant, variant> m;
					m[variant("clientX")] = variant(p.x);
					m[variant("clientY")] = variant(p.y);
					m[variant("button")] = variant(static_cast<int>(button - 1));
					getScriptHandler()->runEventHandler(shared_from_this(), EventHandlerId::MOUSE_DOWN, variant(&m));
				}
			}
		}

		if(!handleMouseButtonDownInt(trigger, p)) {
			return false;
		}

		// XXX
		return true;
	}

	bool Node::handleMouseMotion(bool* trigger, const point& mp)
	{
		auto pos = model_matrix_ * glm::vec4(static_cast<float>(mp.x), static_cast<float>(mp.y), 0.0f, 1.0f);
		point p(static_cast<int>(pos.x), static_cast<int>(pos.y));
		//LOG_DEBUG("mp: " << mp << ", p: " << p << ", ar: " <<  active_rect_);
		if(!active_rect_.empty()) {
			if(geometry::pointInRect(p, active_rect_)) {
				if(mouse_entered_ == false && getScriptHandler() && hasActiveHandler(EventHandlerId::MOUSE_ENTER)) {
					std::map<variant, variant> m;
					m[variant("clientX")] = variant(p.x);
					m[variant("clientY")] = variant(p.y);
					getScriptHandler()->runEventHandler(shared_from_this(), EventHandlerId::MOUSE_ENTER, variant(&m));
				}
				mouse_entered_ = true;
				if(scrollbar_vert_ != nullptr) {
					scrollbar_vert_->triggerFadeIn();
				}
				if(scrollbar_horz_ != nullptr) {
					scrollbar_horz_->triggerFadeIn();
				}
			} else {
				if(mouse_entered_ == true && getScriptHandler() && hasActiveHandler(EventHandlerId::MOUSE_LEAVE)) {
					std::map<variant, variant> m;
					m[variant("clientX")] = variant(p.x);
					m[variant("clientY")] = variant(p.y);
					getScriptHandler()->runEventHandler(shared_from_this(), EventHandlerId::MOUSE_LEAVE, variant(&m));
				}
				mouse_entered_ = false;
				if(scrollbar_vert_ != nullptr) {
					scrollbar_vert_->triggerFadeOut();
				}
				if(scrollbar_horz_ != nullptr) {
					scrollbar_horz_->triggerFadeOut();
				}
			}

			if(getScriptHandler() && hasActiveHandler(EventHandlerId::MOUSE_MOVE)) {
				std::map<variant, variant> m;
				m[variant("clientX")] = variant(p.x);
				m[variant("clientY")] = variant(p.y);
				getScriptHandler()->runEventHandler(shared_from_this(), EventHandlerId::MOUSE_MOVE, variant(&m));
			}
		}

		if(!handleMouseMotionInt(trigger, p)) {
			return false;
		}
		bool hover = hasPseudoClass(css::PseudoClass::HOVER);
		if(!hover || active_rect_.empty()) {
			return true;
		}
		if(mouse_entered_) {
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

	std::string Node::writeXHTML()
	{
		std::ostringstream ss;
		for(auto& child : children_) {
			switch(child->id()) {
				case NodeId::DOCUMENT: break;
				case NodeId::ATTRIBUTE: break;
				case NodeId::DOCUMENT_FRAGMENT: break;
				case NodeId::ELEMENT: {
					ss << "<" << child->getTag();
					for(auto& attr : attributes_) {
						ss << " \"" << attr.first << "\"=\"" << attr.second << "\"";
					}
					std::string child_xml = child->writeXHTML();
					if(child_xml.empty()) {
						ss << "/>";
					} else {
						ss << ">" << child_xml << "</" << child->getTag() << ">";
					}
					break;
				}
				case NodeId::TEXT:
					ss << child->getValue();
					break;
				default: break;
			}
		}
		return ss.str();
	}

	void Node::setInnerXHTML(const std::string& s)
	{
		auto owner = owner_document_.lock();
		ASSERT_LOG(owner != nullptr, "Unable to lock owner document.");
		children_.clear();
		// Pre-check if the string is just plain text (i.e. no markup).
		DocumentFragmentPtr frag = nullptr;
		if(s.find('<') != std::string::npos && s.find('>') != std::string::npos) {
			frag = parse_from_string(s, owner);
		} else {
			frag = DocumentFragment::create();
			frag->addChild(Text::create(s, owner), owner);
		}
		addChild(frag, owner);
		owner->rebuildTree();
	}

	bool Document::handleMouseMotion(bool claimed, int x, int y)
	{
		point p(x, y);
		for(auto& evt : event_listeners_) {
			Uint32 buttons = SDL_GetMouseState(nullptr, nullptr);
			claimed |= evt->mouse_motion(claimed, p, SDL_GetModState());
		}
		if(claimed) {
			return claimed;
		}

		bool trigger = false;		
		claimed = !preOrderTraversal([&trigger, &p](NodePtr node) {
			node->handleMouseMotion(&trigger, p);
			return true;
		});
		trigger_layout_ |= trigger;
		return claimed;
	}

	bool Document::handleMouseButtonDown(bool claimed, int x, int y, unsigned button)
	{
		point p(x, y);
		for(auto& evt : event_listeners_) {
			Uint32 buttons = SDL_GetMouseState(nullptr, nullptr);
			claimed |= evt->mouse_button_down(claimed, p, buttons, SDL_GetModState());
		}
		if(claimed) {
			return claimed;
		}

		bool trigger = false;
		claimed = !preOrderTraversal([&trigger, &p, button](NodePtr node) {
			node->handleMouseButtonDown(&trigger, p, button);
			return true;
		});
		trigger_layout_ |= trigger;
		return claimed;
	}

	bool Document::handleMouseButtonUp(bool claimed, int x, int y, unsigned button)
	{
		point p(x, y);
		for(auto& evt : event_listeners_) {
			Uint32 buttons = SDL_GetMouseState(nullptr, nullptr);
			claimed |= evt->mouse_button_up(claimed, p, buttons, SDL_GetModState());
		}
		if(claimed) {
			return claimed;
		}

		bool trigger = false;
		claimed = !preOrderTraversal([&trigger, &p, button](NodePtr node) {
			node->handleMouseButtonUp(&trigger, p, button);
			return true;
		});		
		trigger_layout_ |= trigger;
		return claimed;
	}

	bool Document::handleMouseWheel(bool claimed, int x, int y, int direction)
	{
		point delta(x, y);
		int mx, my;
		Uint32 buttons = SDL_GetMouseState(&mx, &my);
		point p(mx, my);
		for(auto& evt : event_listeners_) {
			claimed |= evt->mouse_wheel(claimed, p, delta, direction);
		}
		if(claimed) {
			return claimed;
		}
		
		bool trigger = false;
		claimed = !preOrderTraversal([&trigger, &p, &delta, direction](NodePtr node) {
			node->handleMouseWheel(&trigger, p, delta, direction);
			return true;
		});		
		trigger_layout_ |= trigger;
		return claimed;
	}

	void Document::addEventListener(event_listener_ptr evt)
	{
		event_listeners_.emplace(evt);
	}

	void Document::removeEventListener(event_listener_ptr evt)
	{
		event_listeners_.erase(evt);
	}

	void Document::clearEventListeners(void)
	{
		event_listeners_.clear();
	}

	// Documents do not have an owner document.
	Document::Document(css::StyleSheetPtr ss)
		: Node(NodeId::DOCUMENT, WeakDocumentPtr()),
		  style_sheet_(ss == nullptr ? std::make_shared<css::StyleSheet>() : ss),
		  trigger_layout_(true),
		  trigger_render_(false),
		  trigger_rebuild_(false),
		  active_element_(),
		  event_listeners_()
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

	KRE::SceneTreePtr Document::process(StyleNodePtr& style_tree, int w, int h)
	{
		RootBoxPtr layout = nullptr;
		bool changed = false;

		if(needsRebuild()) {
			LOG_INFO("Rebuild layout!");
			style_tree.reset();
			trigger_rebuild_ = false;
			triggerLayout();
		}

		if(needsLayout()) {
			LOG_INFO("Triggered layout!");
			clearEventListeners();

			// XXX should we should have a re-process styles flag here.
			{
				profile::manager pman("apply styles");
				processStyleRules();
			}

			{
				profile::manager pman("update style tree");
				if(style_tree == nullptr) {
					style_tree = StyleNode::createStyleTree(std::static_pointer_cast<Document>(shared_from_this()));
					processScriptAttributes();
				} else {
					style_tree->updateStyles();
				}
			}

			{
				profile::manager pman("layout");
				layout = Box::createLayout(style_tree, w, h);
			}

			triggerRender();
			trigger_layout_ = false;
		}

		if(needsRender() && layout != nullptr) {
			profile::manager pman_render("render");
			layout->getSceneTree()->clear();
			layout->render(point());
			trigger_render_ = false;
			changed = true;
		}

		return layout != nullptr ? layout->getSceneTree() : nullptr;
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

	void Document::registerScriptHandler(const std::string& type, std::function<ScriptPtr()> fn)
	{
		get_script_map()[type] = fn();
	}

	ScriptPtr Document::findScriptHandler(const std::string& type)
	{
		if(type.empty()) {
			if(get_script_map().empty()) {
				return nullptr;
			}
			return get_script_map().begin()->second;
		}
		auto it = get_script_map().find(type);
		if(it == get_script_map().end()) {
			return nullptr;
		}
		return it->second;
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
