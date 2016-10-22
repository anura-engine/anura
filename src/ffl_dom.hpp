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

#include "RenderFwd.hpp"
#include "SceneFwd.hpp"
#include "WindowManagerFwd.hpp"

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

#include "xhtml.hpp"
#include "xhtml_script_interface.hpp"
#include "SceneTree.hpp"

#include "ffl_dom_fwd.hpp"

namespace xhtml
{
	class StyleObject : public game_logic::FormulaCallable
	{
	public:
		StyleObject(const StyleNodePtr& styles);
		void surrenderReferences(GarbageCollector* collector) override;
	private:
		DECLARE_CALLABLE(StyleObject);
		StyleNodePtr style_node_;
	};
	typedef ffl::IntrusivePtr<StyleObject> StyleObjectPtr;

	class ElementObject : public game_logic::FormulaCallable
	{
	public:
		ElementObject(const NodePtr& element);
		//void setHandler(EventHandlerId evtname, const game_logic::FormulaPtr& handler);
		void runHandler(EventHandlerId evtname, game_logic::FormulaCallable* environment, const variant& params);
		game_logic::MapFormulaCallablePtr createMouseEventCallable(game_logic::FormulaCallable* environment, const variant& params);
		game_logic::MapFormulaCallablePtr createWheelEventCallable(game_logic::FormulaCallable* environment, const variant& params);
		game_logic::MapFormulaCallablePtr createKeyEventCallable(game_logic::FormulaCallable* environment, const variant& params);
		static bool isMouseEvent(EventHandlerId evtname);
		static bool isKeyEvent(EventHandlerId evtname);
		void surrenderReferences(GarbageCollector* collector) override;
		void setScript(EventHandlerId evtname, const variant& script);
		variant getScript(EventHandlerId evtname) const;
	private:
		DECLARE_CALLABLE(ElementObject);
		NodePtr element_;
		std::vector<variant> handlers_;
		StyleObjectPtr styles_;
	};
	typedef ffl::IntrusivePtr<ElementObject> ElementObjectPtr;

	class DocumentObject : public game_logic::FormulaCallable
	{
	public:
		DocumentObject(const variant& v);
		variant write();

		void init(game_logic::FormulaCallable* environment);

		void draw(const KRE::WindowPtr& wnd) const;
		void process();
		bool handleEvents(const point& p, const SDL_Event& e);

		void surrenderReferences(GarbageCollector* collector) override;

		void setEnvironment(game_logic::FormulaCallable* environment) { environment_ = environment; }
		game_logic::FormulaCallable* getEnvironment() const { return environment_; }

		void setLayoutSize(const rect& r) { layout_size_ = r; }
		
		ElementObjectPtr getActiveElement() const;
		ElementObjectPtr getElementById(const std::string& element_id) const;
		ElementObjectPtr getElementByNode(const NodePtr& node) const;
		std::vector<ElementObjectPtr> getElementsByTagName(const std::string& element_tag) const;
		std::vector<ElementObjectPtr> getElementsByClassName(const std::string& class_name) const;
		std::vector<ElementObjectPtr> querySelectorAll(const std::string& selector) const;
	private:
		DECLARE_CALLABLE(DocumentObject);

		game_logic::FormulaCallable* environment_;

		KRE::SceneGraphPtr scene_;
		KRE::SceneNodePtr root_;
		KRE::RenderManagerPtr rmanager_;
		int last_process_time_;
		
		xhtml::DocumentPtr doc_;
		xhtml::StyleNodePtr style_tree_;
		KRE::SceneTreePtr scene_tree_;

		std::string doc_name_;
		std::string ss_name_;

		rect layout_size_;
		bool do_onload_;

		mutable std::map<NodePtr, ElementObjectPtr> element_cache_;
	};
	
	typedef ffl::IntrusivePtr<DocumentObject> DocumentObjectPtr;
}
