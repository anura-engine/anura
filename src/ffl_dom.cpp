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

#include "ModelMatrixScope.hpp"
#include "SceneGraph.hpp"
#include "SceneNode.hpp"
#include "RenderManager.hpp"
#include "WindowManager.hpp"

#include "css_parser.hpp"
#include "display_list.hpp"
#include "xhtml.hpp"
#include "xhtml_layout_engine.hpp"
#include "xhtml_root_box.hpp"
#include "xhtml_style_tree.hpp"
#include "xhtml_node.hpp"
#include "xhtml_render_ctx.hpp"
#include "xhtml_script_interface.hpp"

#include "ffl_dom.hpp"
#include "formula.hpp"
#include "module.hpp"
#include "profile_timer.hpp"
#include "screen_handling.hpp"
#include "variant_utils.hpp"

namespace xhtml
{
	namespace
	{
		const std::string default_user_agent_style_sheet = "data/user_agent.css";

		class FFLScript : public Script
		{
		public:
			FFLScript(DocumentObject* docobj) : document_object_(docobj) {}
			void runScriptFile(const std::string& filename) override
			{
			}
			void runScript(const std::string& script)  override
			{
				game_logic::FormulaPtr handler = document_object_->getEnvironment()->createFormula(variant(script));
				if(document_object_->getEnvironment()) {
					variant value = handler->execute(*document_object_->getEnvironment());
					document_object_->getEnvironment()->executeCommand(value);
				} else {
					LOG_ERROR("FFLScript::runScript() called without environment!");
				}
			}
			void preProcess(const NodePtr& element, EventHandlerId evtname, const std::string& script) override
			{
				ElementObjectPtr eo = document_object_->getElementByNode(element);
				ASSERT_LOG(eo != nullptr, "Bad juju. ElementObjectPtr == nullptr");
				game_logic::FormulaPtr handler = document_object_->getEnvironment()->createFormula(variant(script));
				eo->setHandler(evtname, handler);
			}
			void runEventHandler(const NodePtr& element, EventHandlerId evtname, const variant& params) override
			{
				ElementObjectPtr eo = document_object_->getElementByNode(element);
				ASSERT_LOG(eo != nullptr, "Bad juju. ElementObjectPtr == nullptr");
				if(document_object_->getEnvironment()) {
					eo->runHandler(evtname, document_object_->getEnvironment(), params);
				} else {
					LOG_ERROR("FFLScript::runScript() called without environment!");
				}
			}
		private:
			DocumentObject* document_object_;			
		};
	}

	using namespace KRE;
	DocumentObject::DocumentObject(const variant& v)
		: environment_(nullptr), 
		  scene_(SceneGraph::create("xhtml::DocumentObject")),
		  root_(scene_->getRootNode()),
		  rmanager_(),
		  last_process_time_(-1),
		  doc_(nullptr),
		  style_tree_(nullptr),
		  display_list_(nullptr),
		  doc_name_(),
		  ss_name_(),
		  layout_size_()
	{
		ASSERT_LOG(v.has_key("xhtml") && v["xhtml"].is_string(), "No xhtml document was specified.");
		doc_name_ = module::map_file(v["xhtml"].as_string());

		root_->setNodeName("xhtml_root_node");

		rmanager_ = std::make_shared<RenderManager>();
		rmanager_->addQueue(0, "XHTML/CSS");

		ss_name_ = default_user_agent_style_sheet;
		if(v.has_key("style_sheet") && v["style_sheet"].is_string()) {
			ss_name_ = v["style_sheet"].as_string();
		}

		if(v.has_key("layout_size")) {
			layout_size_ = rect(v["layout_size"]);
		} else {
			auto& gs = graphics::GameScreen::get();			
			layout_size_ = rect(0, 0, gs.getWidth(), gs.getHeight());
		}
	}

	void DocumentObject::init(game_logic::FormulaCallable* environment)
	{
		environment_ = environment;
		ASSERT_LOG(environment_ != nullptr, "DocumentObject::init called without a valid environment");

		ScriptHandlerRegistrar ffl_reg1("text/ffl", [this]() ->ScriptPtr {
			return std::make_shared<FFLScript>(this);
		});
		ScriptHandlerRegistrar ffl_reg2("application/ffl", [this]() ->ScriptPtr {
			return std::make_shared<FFLScript>(this);
		});

		auto user_agent_style_sheet = std::make_shared<css::StyleSheet>();
		css::Parser::parse(user_agent_style_sheet, sys::read_file(module::map_file(ss_name_)));

		auto doc_frag = xhtml::parse_from_file(doc_name_);
		doc_ = Document::create(user_agent_style_sheet);
		doc_->addChild(doc_frag);
		doc_->processStyles();
		// whitespace can only be processed after applying styles.
		doc_->processWhitespace();
		doc_->processScriptAttributes();

		display_list_ = std::make_shared<DisplayList>(scene_);
		root_->attachNode(display_list_);		

		/*doc_->preOrderTraversal([](xhtml::NodePtr n) {
			LOG_DEBUG(n->toString());
			return true;
		});*/
	}
	
	variant DocumentObject::write()
	{
		variant_builder builder;
		builder.add("xhtml", doc_name_);
		if(ss_name_ != default_user_agent_style_sheet) {
			builder.add("stylesheet", ss_name_);
		}
		return variant();
	}

	void DocumentObject::draw(const KRE::WindowPtr& wnd) const
	{
		ModelManager2D mm(layout_size_.x(), layout_size_.y());
		scene_->renderScene(rmanager_);
		rmanager_->render(wnd);
	}
	
	void DocumentObject::process()
	{
		if(doc_->needsLayout()) {
			LOG_DEBUG("Triggered layout!");

			display_list_->clear();

			// XXX should we should have a re-process styles flag here.

			{
			profile::manager pman("apply styles");
			doc_->processStyleRules();
			}

			{
				profile::manager pman("update style tree");
				if(style_tree_ == nullptr) {
					style_tree_ = xhtml::StyleNode::createStyleTree(doc_);
				} else {
					style_tree_->updateStyles();
				}
			}

			xhtml::RootBoxPtr layout = nullptr;
			{
			profile::manager pman("layout");
			layout = xhtml::Box::createLayout(style_tree_, layout_size_.w(), layout_size_.h());
			}

			{
			profile::manager pman_render("render");
			layout->render(display_list_, point());
			}
		}

		float delta_time = 0.0f;
		if(last_process_time_ == -1) {
			last_process_time_ = profile::get_tick_time();
		}
		auto current_time = profile::get_tick_time();
		delta_time = (current_time - last_process_time_) / 1000.0f;
		scene_->process(delta_time);
		if(style_tree_ != nullptr) {
			style_tree_->process(delta_time);
		}
		last_process_time_ = current_time;
	}
	
	bool DocumentObject::handleEvents(const point& p, const SDL_Event& e)
	{
		const int adj_x = (e.type == SDL_MOUSEMOTION ? e.motion.x : e.button.x) - p.x - layout_size_.x();
		const int adj_y = (e.type == SDL_MOUSEMOTION ? e.motion.y : e.button.y) - p.y - layout_size_.y();
		bool claimed = false;
		if(e.type == SDL_MOUSEMOTION) {
			claimed = doc_->handleMouseMotion(false, adj_x, adj_y);
		} else if(e.type == SDL_MOUSEBUTTONDOWN) {
			claimed = doc_->handleMouseButtonDown(false, adj_x, adj_y, e.button.button);
		} else if(e.type == SDL_MOUSEBUTTONUP) {
			claimed = doc_->handleMouseButtonUp(false, adj_x, adj_y, e.button.button);
		}

		return claimed;
	}

	ElementObjectPtr DocumentObject::getElementById(const std::string& element_id) const
	{
		NodePtr element = doc_->getElementById(element_id);
		if(element != nullptr) {
			ElementObjectPtr& eo = element_cache_[element];
			if(eo == nullptr) {
				eo.reset(new ElementObject(element));
			}
			return eo;
		} 
		return nullptr;
	}

	ElementObjectPtr DocumentObject::getElementByNode(const NodePtr& element) const
	{
		ASSERT_LOG(element != nullptr, "DocumentObject::getElementByNode passed in element was null.");
		ElementObjectPtr& eo = element_cache_[element];
		if(eo == nullptr) {
			eo.reset(new ElementObject(element));
		}
		return eo;
	}

	void DocumentObject::surrenderReferences(GarbageCollector* collector)
	{

		for(auto& ele : element_cache_) {
			collector->surrenderPtr(&ele.second, "XHTML::ELEMENT_OBJECT");
		}
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(DocumentObject)
	
		DEFINE_FIELD(width, "int")
			return variant(obj.layout_size_.w());
		DEFINE_SET_FIELD
			obj.layout_size_.set_w(value.as_int());
			obj.doc_->triggerLayout();
		
		DEFINE_FIELD(height, "int")
			return variant(obj.layout_size_.h());
		DEFINE_SET_FIELD
			obj.layout_size_.set_h(value.as_int());
			obj.doc_->triggerLayout();
		
		DEFINE_FIELD(wh, "[int,int]")
			std::vector<variant> v;
			v.emplace_back(obj.layout_size_.w());
			v.emplace_back(obj.layout_size_.h());
			return variant(&v);
		DEFINE_SET_FIELD
			obj.layout_size_.set_w(value[0].as_int());
			obj.layout_size_.set_h(value[1].as_int());
			obj.doc_->triggerLayout();
			
		BEGIN_DEFINE_FN(getElementById, "(string) ->builtin element_object|null")
			const std::string element_id = FN_ARG(0).as_string();
			ElementObjectPtr eo = obj.getElementById(element_id);
			if(eo == nullptr) {
				return variant();
			}
			return variant(eo.get());
		END_DEFINE_FN
		
	END_DEFINE_CALLABLE(DocumentObject)


	// ElementObject
	ElementObject::ElementObject(const NodePtr& element)
		: element_(element),
		  handlers_()
	{
		ASSERT_LOG(element_ != nullptr && element_->id() == NodeId::ELEMENT, "Tried to construct an ElementObject, without a valid Node.");
		handlers_.resize(static_cast<int>(EventHandlerId::MAX_EVENT_HANDLERS));
	}

	void ElementObject::setHandler(EventHandlerId evtname, const game_logic::FormulaPtr& handler)
	{
		int index = static_cast<int>(evtname);
		ASSERT_LOG(index < static_cast<int>(handlers_.size()), "Handler index exceeds bounds. " << index << " >= " << static_cast<int>(handlers_.size()));
		handlers_[index] = handler;
	}

	void ElementObject::runHandler(EventHandlerId evtname, game_logic::FormulaCallable* environment, const variant& params)
	{
		int index = static_cast<int>(evtname);
		ASSERT_LOG(index < static_cast<int>(handlers_.size()), "Handler index exceeds bounds. " << index << " >= " << static_cast<int>(handlers_.size()));
		auto& handler = handlers_[index];
		if(handler != nullptr) {
			// XXX add parameters as needed.
			game_logic::MapFormulaCallablePtr callable = nullptr;
			if(ElementObject::isMouseEvent(evtname)) {
				callable = createMouseEventCallable(environment, params);
			} else if(ElementObject::isKeyEvent(evtname)) {
				callable = createKeyEventCallable(environment, params);
			}
			variant value = handler->execute(callable != nullptr ? *callable : *environment);
			environment->executeCommand(value);
		}
	}

	bool ElementObject::isKeyEvent(EventHandlerId evtname)
	{
		if(evtname == EventHandlerId::KEY_DOWN
			|| evtname == EventHandlerId::KEY_PRESS
			|| evtname == EventHandlerId::KEY_UP) {
			return true;
		}
		return false;
	}

	bool ElementObject::isMouseEvent(EventHandlerId evtname)
	{
		if(evtname == EventHandlerId::MOUSE_DOWN
			|| evtname == EventHandlerId::MOUSE_UP
			|| evtname == EventHandlerId::MOUSE_MOVE
			|| evtname == EventHandlerId::MOUSE_ENTER
			//|| evtname == EventHandlerId::CLICK
			//|| evtname == EventHandlerId::DBL_CLICK
			//|| evtname == EventHandlerId::MOUSE_OVER
			//|| evtname == EventHandlerId::MOUSE_OUT
			|| evtname == EventHandlerId::MOUSE_LEAVE) {
			return true;
		}
		return false;
	}

	game_logic::MapFormulaCallablePtr ElementObject::createMouseEventCallable(game_logic::FormulaCallable* environment, const variant& params)
	{
		// MouseEvent
		// screenX (long)
		// screenY (long)
		// clientX (long)
		// clientY (long)
		// ctrlKey (boolean)
		// shiftKey (boolean)
		// altKey (boolean)
		// metaKey (boolean)
		// button (short) (0 - primary(left), 1 - auxillary(middle), 2 - secondary(right)) -- indicates which button changed state
		// EventTarget? (relatedTarget)
		// buttons (unsigned short) (bitmask of buttons (1 << button)) -- indicates which buttons are active

		using namespace game_logic;
		int mx, my;
		Uint32 buttons = SDL_GetMouseState(&mx, &my);
		SDL_Keymod key_state = SDL_GetModState();
		MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(environment));
		if(params.is_map()) {
			for(auto& p : params.as_map()) {
				callable->add(p.first.as_string(), p.second);
			}
		}
		callable->add("screenX", variant(mx));
		callable->add("screenY", variant(mx));
		//callable->add("clientX", variant(adj_x));
		//callable->add("clientY", variant(adj_y));
		//callable->add("button", variant(button));
		// conveniently SDL2 uses the same definition as the DOM for button bit positions.
		callable->add("buttons", variant(buttons));
		callable->add("ctrlKey", variant::from_bool(key_state & KMOD_CTRL ? true : false));
		callable->add("shiftKey", variant::from_bool(key_state & KMOD_SHIFT ? true : false));
		callable->add("altKey", variant::from_bool(key_state & KMOD_ALT ? true : false));
		callable->add("metaKey", variant::from_bool(key_state & KMOD_GUI ? true : false));
		return callable;
	}

	game_logic::MapFormulaCallablePtr ElementObject::createWheelEventCallable(game_logic::FormulaCallable* environment, const variant& params)
	{
		// WheelEvent
		// const unsigned long DOM_DELTA_PIXEL = 0x00;
		// const unsigned long DOM_DELTA_LINE = 0x01;
		// const unsigned long DOM_DELTA_PAGE = 0x02;
		// readonly    attribute double        deltaX;
		// readonly    attribute double        deltaY;
		// readonly    attribute double        deltaZ;
		// readonly    attribute unsigned long deltaMode;
		using namespace game_logic;
		if(params.is_map()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(environment));
			for(auto& p : params.as_map()) {
				callable->add(p.first.as_string(), p.second);
			}
			// XXX just making this up.
			callable->add("deltaMode", variant(0));
			return callable;
		}
		return nullptr;
	}

	game_logic::MapFormulaCallablePtr ElementObject::createKeyEventCallable(game_logic::FormulaCallable* environment, const variant& params)
	{
		using namespace game_logic;
		SDL_Keymod key_state = SDL_GetModState();
		MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(environment));
		if(params.is_map()) {
			for(auto& p : params.as_map()) {
				callable->add(p.first.as_string(), p.second);
			}
		}
		callable->add("ctrlKey", variant::from_bool(key_state & KMOD_CTRL ? true : false));
		callable->add("shiftKey", variant::from_bool(key_state & KMOD_SHIFT ? true : false));
		callable->add("altKey", variant::from_bool(key_state & KMOD_ALT ? true : false));
		callable->add("metaKey", variant::from_bool(key_state & KMOD_GUI ? true : false));
		return callable;
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(ElementObject)
		
		DEFINE_FIELD(dummy, "null")
			return variant();

	END_DEFINE_CALLABLE(ElementObject)
}
