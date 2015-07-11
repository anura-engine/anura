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
#include <map>
#include <vector>

#include "geometry.hpp"
#include "SceneFwd.hpp"

#include "css_stylesheet.hpp"
#include "css_transition.hpp"
#include "xhtml.hpp"
#include "xhtml_element_id.hpp"
#include "xhtml_script_interface.hpp"

namespace xhtml
{
	enum class NodeId {
		DOCUMENT,
		ELEMENT,
		ATTRIBUTE,
		DOCUMENT_FRAGMENT,
		TEXT,
	};

	struct Keystate
	{
		// true if pressed, false if released
		bool pressed;
		// true if is repeat key.
		bool repeat;
		// keyboard scan code
		int scancode;
		// unicode sysmbol
		char32_t symbol;
		// control key modifiers
		unsigned short modifiers;
	};

	typedef std::map<std::string, AttributePtr> AttributeMap;
	typedef std::vector<NodePtr> NodeList;

	struct Word
	{
		explicit Word(const std::string& w) : word(w), advance() {}
		std::string word;
		std::vector<geometry::Point<FixedPoint>> advance;
	};

	struct Line
	{
		Line() : line(), is_end_line(false), space_advance(0) {}
		Line(int cnt, const Word& w) : line(cnt, w), is_end_line(false), space_advance(0) {}
		std::vector<Word> line;
		bool is_end_line;
		FixedPoint space_advance;
	};
	typedef std::shared_ptr<Line> LinePtr;

	struct Lines
	{
		Lines() : space_advance(0), lines(1, Line()) {}
		long space_advance;
		std::vector<Line> lines;
		double line_height;
	};
	typedef std::shared_ptr<Lines> LinesPtr;

	class Node : public std::enable_shared_from_this<Node>
	{
	public:
		explicit Node(NodeId id, WeakDocumentPtr owner);
		virtual ~Node();
		NodeId id() const { return id_; }
		void setOwner(const DocumentPtr& owner) { owner_document_ = owner; }
		void addChild(NodePtr child, const DocumentPtr& owner=nullptr);
		void removeChild(NodePtr child);
		void addAttribute(AttributePtr a);
		void setAttribute(const std::string& name, const std::string& value);
		// Called after children and attributes have been added.
		virtual void init() {}
		NodePtr getLeft() const { return left_.lock(); }
		NodePtr getRight() const { return right_.lock(); }
		NodePtr getParent() const { return parent_.lock(); }
		void setParent(WeakNodePtr p) { parent_ = p; }
		void setStylePointer(const StyleNodePtr& style) { style_node_ = style; }
		StyleNodePtr getStylePointer() const { return style_node_.lock(); }
		DocumentPtr getOwnerDoc() const { return owner_document_.lock(); }
		virtual std::string toString() const = 0;
		const AttributeMap& getAttributes() const { return attributes_; }
		const NodeList& getChildren() const { return children_; }
		// top-down scanning of the tree
		bool preOrderTraversal(std::function<bool(NodePtr)> fn);
		// bottom-up scanning of the tree
		bool postOrderTraversal(std::function<bool(NodePtr)> fn);
		// scanning from a child node up through parents
		bool ancestralTraverse(std::function<bool(NodePtr)> fn);
		virtual bool hasTag(const std::string& tag) const { return false; }
		virtual bool hasTag(ElementId tag) const { return false; }
		AttributePtr getAttribute(const std::string& name);
		virtual const std::string& getValue() const;
		void normalize();
		void mergeProperties(const css::Specificity& specificity, const css::PropertyList& plist);
		const css::PropertyList& getProperties() const { return properties_; }

		void processWhitespace();

		NodePtr getElementById(const std::string& id);
		
		void addPseudoClass(css::PseudoClass pclass) { pclass_ = pclass_ | pclass; }
		bool hasPseudoClass(css::PseudoClass pclass) { return (pclass_ & pclass) != css::PseudoClass::NONE; }
		bool hasPsuedoClassActive(css::PseudoClass pclass) { return (active_pclass_ & pclass) != css::PseudoClass::NONE; }
		css::PseudoClass getPseudoClass() const { return pclass_; }
		// This sets the rectangle that should be active for mouse presses.
		void setActiveRect(const rect& r) { active_rect_ = r; }
		const rect& getActiveRect() const { return active_rect_; }
		void processScriptAttributes();
		virtual void layoutComplete() {}

		virtual void process(float dt) {}

		bool handleMouseMotion(bool* trigger, const point& p);
		bool handleMouseButtonUp(bool* trigger, const point& p, unsigned button);
		bool handleMouseButtonDown(bool* trigger, const point& p, unsigned button);

		void clearProperties() { properties_.clear(); }
		void inheritProperties();
		
		// for elements
		const rect& getDimensions() { return dimensions_; }
		void setDimensions(const rect& r) { dimensions_ = r; handleSetDimensions(r); }
		virtual KRE::SceneObjectPtr getRenderable() { return nullptr; }
		// is this element replaced, replaced elements generate a seperate box during layout.
		virtual bool isReplaced() const { return false; }
		virtual bool ignoreForLayout() const { return false; }
		virtual const std::string& getTag() const { static const std::string tag("none"); return tag; }

		void setScriptHandler(const ScriptPtr& script_handler);
		ScriptPtr getScriptHandler() const { return script_handler_; }
		void setActiveHandler(EventHandlerId id, bool active=true);
		bool hasActiveHandler(EventHandlerId id);
	protected:
		std::string nodeToString() const;
	private:
		Node() = delete;
		virtual bool handleMouseMotionInt(bool* trigger, const point& p) { return true; }
		virtual bool handleMouseButtonUpInt(bool* trigger, const point& p) { return true; }
		virtual bool handleMouseButtonDownInt(bool* trigger, const point& p) { return true; }
		virtual void handleSetDimensions(const rect& r) {}

		NodeId id_;
		NodeList children_;
		AttributeMap attributes_;

		WeakNodePtr left_, right_;
		WeakNodePtr parent_;

		WeakDocumentPtr owner_document_;

		css::PropertyList properties_;
		css::PseudoClass pclass_;
		css::PseudoClass active_pclass_;
		rect active_rect_;

		rect dimensions_;

		ScriptPtr script_handler_;
		std::vector<bool> active_handlers_;

		bool mouse_entered_;

		// back reference to the tree node holding computer values for us.
		WeakStyleNodePtr style_node_;
	};

	class Document : public Node
	{
	public:
		static DocumentPtr create(css::StyleSheetPtr ss=nullptr);
		std::string toString() const override;
		void processStyles();
		void processStyleRules();

		bool handleMouseMotion(bool claimed, int x, int y);
		bool handleMouseButtonDown(bool claimed, int x, int y, unsigned button);
		bool handleMouseButtonUp(bool claimed, int x, int y, unsigned button);

		void triggerLayout() { trigger_layout_ = true; }
		void triggerRender() { trigger_render_ = true; }
		bool needsLayout() const { return trigger_layout_; }
		bool needsRender() const { return trigger_render_; }
		void layoutComplete() override { trigger_render_ = false; trigger_layout_ = false; }
		void renderComplete() { trigger_render_ = false;  }

		// type is expected to be a content type i.e. "text/javascript"
		static void registerScriptHandler(const std::string& type, std::function<ScriptPtr()> fn);
		static ScriptPtr findScriptHandler(const std::string& type=std::string());
	protected:
		Document(css::StyleSheetPtr ss);
		css::StyleSheetPtr style_sheet_;
		bool trigger_layout_;
		bool trigger_render_;
	};

	class DocumentFragment : public Node
	{
	public:
		static DocumentFragmentPtr create(WeakDocumentPtr owner=WeakDocumentPtr());
		std::string toString() const override;
	protected:
		DocumentFragment(WeakDocumentPtr owner);
	};

	class Attribute : public Node
	{
	public:
		static AttributePtr create(const std::string& name, const std::string& value, WeakDocumentPtr owner=WeakDocumentPtr());
		const std::string& getName() const { return name_; }
		const std::string& getValue() const { return value_; }
		std::string toString() const override;
	protected:
		explicit Attribute(const std::string& name, const std::string& value, WeakDocumentPtr owner);
	private:
		std::string name_;
		std::string value_;
	};

	struct ScriptHandlerRegistrar
	{
		ScriptHandlerRegistrar(const std::string& type, std::function<ScriptPtr()> create_fn)
		{
			// register the class factory function 
			Document::registerScriptHandler(type, create_fn);
		}
	};
}
