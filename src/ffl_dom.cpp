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

#include <boost/algorithm/string.hpp>

#include "ModelMatrixScope.hpp"
#include "SceneGraph.hpp"
#include "SceneNode.hpp"
#include "RenderManager.hpp"
#include "WindowManager.hpp"

#include "css_parser.hpp"
#include "xhtml.hpp"
#include "xhtml_element.hpp"
#include "xhtml_layout_engine.hpp"
#include "xhtml_root_box.hpp"
#include "xhtml_style_tree.hpp"
#include "xhtml_node.hpp"
#include "xhtml_render_ctx.hpp"
#include "xhtml_script_interface.hpp"

#include "custom_object.hpp"
#include "ffl_dom.hpp"
#include "formula.hpp"
#include "json_parser.hpp"
#include "level.hpp"
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
				eo->setScript(evtname, variant(script));
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

		std::vector<std::string> split_string(const std::string& s) 
		{
			std::vector<std::string> res;
			boost::split(res, s, boost::is_any_of(" \n\r\t\f"), boost::token_compress_on);
			return res;
		}

		class EntityObject : public ObjectProxy
		{
		public:
			EntityObject(const AttributeMap& am) 
				: ObjectProxy(am),
				  entity_(),
				  handle_process_on_entity_(true),
				  commands_handler_()
			{
				auto attr_data = am.find("data");
				std::string obj_str;
				if(attr_data != am.end()) {
					obj_str = attr_data->second->getValue();
				} else {
					auto attr_clsid = am.find("classid");
					if(attr_clsid != am.end()) {
						obj_str = attr_clsid->second->getValue();
					} else {
						LOG_ERROR("No data or clasid tag for ImageObject");
					}
				}

				const variant v = json::parse(obj_str);

				handle_process_on_entity_ = v["handle_process"].as_bool(false);
				if(v["object"].is_string()) {
					// type name, has obj_x, obj_y, facing			
					entity_ = EntityPtr(new CustomObject(v["object"].as_string(), v["obj_x"].as_int(0), v["obj_y"].as_int(0), v["facing"].as_int(1) > 0 ? true : false));
					entity_->finishLoading(NULL);
				} else if(v["object"].is_map()) {
					entity_ = EntityPtr(new CustomObject(v["object"]));
					entity_->finishLoading(NULL);
				} else {
					entity_ = v["object"].try_convert<Entity>();
					ASSERT_LOG(entity_ != NULL, "Couldn't convert 'object' attribue to an entity");
					entity_->finishLoading(NULL);
					entity_->validate_properties();
				}
				if(v.has_key("properties")) {
					ASSERT_LOG(v["properties"].is_map(), "properties field must be a map");
					const variant& properties = v["properties"];
					variant keys = properties.getKeys();
					for(int n = 0; n != keys.num_elements(); ++n) {
						variant value = properties[keys[n]];
						entity_->mutateValue(keys[n].as_string(), value);
					}
				}
				if(v.has_key("commands")) {
					commands_handler_ = entity_->createFormula(v["commands"]);
					using namespace game_logic;
					MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(entity_.get()));
					variant value = commands_handler_->execute(*callable);
					entity_->executeCommand(value);
				}
			}
			KRE::SceneObjectPtr getRenderable() override
			{
				return nullptr;
			}
			void process(float dt) override
			{
				if(entity_ && handle_process_on_entity_) {
					CustomObject* obj = static_cast<CustomObject*>(entity_.get());
					obj->process(Level::current());
				}
			}
			// XXX should probably handle and pass mouse and keyboard events here.
		private:
			EntityPtr entity_;
			bool handle_process_on_entity_;
			game_logic::FormulaPtr commands_handler_;
		};

		ObjectProxyRegistrar obj_png("application/x.entity", [](const AttributeMap& attributes){
			return std::make_shared<EntityObject>(attributes);
		});

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
		  scene_tree_(nullptr),
		  doc_name_(),
		  ss_name_(),
		  layout_size_(),
		  do_onload_(true)
	{
		if(v.is_map() && v.has_key("xhtml") && v["xhtml"].is_string()) {
			doc_name_ = module::map_file(v["xhtml"].as_string());
		} else if(v.is_string()) {
			doc_name_ = module::map_file(v.as_string());
			// XXX should test if file exists if not then set the name to a fake document name
			// and try loading contents directly from the string.
		} else {
			ASSERT_LOG(false, "No xhtml document was specified.");
		}

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

		doc_ = Document::create(user_agent_style_sheet);
		auto doc_frag = xhtml::parse_from_file(doc_name_, doc_);
		doc_->addChild(doc_frag, doc_);
		doc_->processStyles();
		// whitespace can only be processed after applying styles.
		doc_->processWhitespace();

		//style_tree_ = xhtml::StyleNode::createStyleTree(doc_);
		//scene_tree_ = style_tree_->getSceneTree();

		/*
		doc_->preOrderTraversal([](xhtml::NodePtr n) {
			std::stringstream ss;
			ss << n.get() << ", tag: " << n->getTag() << ", owner: " << n->getOwnerDoc().get();
			LOG_DEBUG(ss.str());
			//LOG_DEBUG(n->toString());
			return true;
		});
		*/
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
		//scene_->renderScene(rmanager_);
		//rmanager_->render(wnd);
		if(scene_tree_ != nullptr) {
			scene_tree_->preRender(wnd);
			scene_tree_->render(wnd);
		}
	}
	
	void DocumentObject::process()
	{
		auto st = doc_->process(style_tree_, layout_size_.x(), layout_size_.y(), layout_size_.w(), layout_size_.h());
		if(st != nullptr) {
			scene_tree_ = st;
		}
		if(do_onload_) {
			do_onload_ = false;
			if(environment_ != nullptr) {
				auto* obj = dynamic_cast<CustomObject*>(environment_);
				if(obj != nullptr) {
					obj->handleEvent("onload");
				}
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
		if(adj_x >= 0 && adj_y >= 0) {
			if(e.type == SDL_MOUSEMOTION) {
				claimed = doc_->handleMouseMotion(false, adj_x, adj_y);
			} else if(e.type == SDL_MOUSEBUTTONDOWN) {
				claimed = doc_->handleMouseButtonDown(false, adj_x, adj_y, e.button.button);
			} else if(e.type == SDL_MOUSEBUTTONUP) {
				claimed = doc_->handleMouseButtonUp(false, adj_x, adj_y, e.button.button);
			}
		}

		return claimed;
	}

	ElementObjectPtr DocumentObject::getActiveElement() const
	{
		auto element = doc_->getActiveElement();
		if(element != nullptr) {
			ElementObjectPtr& eo = element_cache_[element];
			if(eo == nullptr) {
				eo.reset(new ElementObject(element));
			}
			return eo;
		} 
		return nullptr;
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

	std::vector<ElementObjectPtr> DocumentObject::getElementsByTagName(const std::string& element_tag) const
	{
		std::vector<ElementObjectPtr> vec;
		auto& ec = element_cache_;
		doc_->preOrderTraversal([&vec, &ec, element_tag](NodePtr n) {
			if(n->id() == NodeId::ELEMENT && n->hasTag(element_tag)) {
				ElementObjectPtr& eo = ec[n];
				if(eo == nullptr) {
					eo.reset(new ElementObject(n));
				}
				vec.emplace_back(eo);
			}
			return true;
		});
		return vec;
	}

	std::vector<ElementObjectPtr> DocumentObject::getElementsByClassName(const std::string& class_name) const
	{
		std::vector<ElementObjectPtr> vec;
		auto& ec = element_cache_;

		std::vector<std::string> class_strs = split_string(class_name);

		doc_->preOrderTraversal([&vec, &ec, &class_strs](NodePtr n) {
			auto attr = n->getAttribute("class");
			if(attr != nullptr) {
				std::vector<std::string> attr_class_strs = split_string(attr->getValue());
				bool matched = true;
				for(auto& cname : class_strs) {
					bool matched_this = false;
					for(auto& acs : attr_class_strs) {
						if(acs == cname) {
							matched_this = true;
						}
					}
					if(!matched_this) {
						matched = false;
					}
				}
				if(matched) {
					ElementObjectPtr& eo = ec[n];
					if(eo == nullptr) {
						eo.reset(new ElementObject(n));
					}
					vec.emplace_back(eo);
				}
			}
			return true;
		});
		return vec;
	}

	std::vector<ElementObjectPtr> DocumentObject::querySelectorAll(const std::string& selector) const
	{
		std::vector<ElementObjectPtr> vec;
		auto& ec = element_cache_;

		css::Tokenizer tokens(selector);
		auto selectors = css::Selector::parseTokens(tokens.getTokens());

		doc_->preOrderTraversal([&vec, &ec, &selectors](NodePtr n) {
			bool matched = false;

			for(auto& select : selectors) {
				if(select->match(n)) {
					matched = true;
				}
			}

			if(matched) {
				ElementObjectPtr& eo = ec[n];
				if(eo == nullptr) {
					eo.reset(new ElementObject(n));
				}
				vec.emplace_back(eo);
			}
			return true;
		});
		return vec;
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

		DEFINE_FIELD(x, "int")
			return variant(obj.layout_size_.x());
		DEFINE_SET_FIELD
			obj.layout_size_.set_x(value.as_int());
		
		DEFINE_FIELD(y, "int")
			return variant(obj.layout_size_.y());
		DEFINE_SET_FIELD
			obj.layout_size_.set_y(value.as_int());
		
		DEFINE_FIELD(xy, "[int,int]")
			std::vector<variant> v;
			v.emplace_back(obj.layout_size_.x());
			v.emplace_back(obj.layout_size_.y());
			return variant(&v);
		DEFINE_SET_FIELD
			obj.layout_size_.set_x(value[0].as_int());
			obj.layout_size_.set_y(value[1].as_int());

		DEFINE_FIELD(activeElement, "builtin element_object|null")
			ElementObjectPtr eo = obj.getActiveElement();
			return eo == nullptr ? variant() : variant(eo.get());

		BEGIN_DEFINE_FN(getElementById, "(string) ->builtin element_object|null")
			const std::string element_id = FN_ARG(0).as_string();
			ElementObjectPtr eo = obj.getElementById(element_id);
			return eo == nullptr ? variant() : variant(eo.get());
			END_DEFINE_FN

		BEGIN_DEFINE_FN(getElementsByTagName, "(string) ->[builtin element_object]")
			const std::string element_tag = FN_ARG(0).as_string();
			auto el_vec = obj.getElementsByTagName(element_tag);
			std::vector<variant> v;
			for(auto& el : el_vec) {
				v.emplace_back(el.get());
			}
			return variant(&v);
		END_DEFINE_FN

		BEGIN_DEFINE_FN(getElementsByClassName, "(string) ->[builtin element_object]")
			const std::string class_name = FN_ARG(0).as_string();
			auto el_vec = obj.getElementsByClassName(class_name);
			std::vector<variant> v;
			for(auto& el : el_vec) {
				v.emplace_back(el.get());
			}
			return variant(&v);
		END_DEFINE_FN

		BEGIN_DEFINE_FN(querySelectorAll, "(string) ->[builtin element_object]")
			const std::string selector_string = FN_ARG(0).as_string();
			auto el_vec = obj.querySelectorAll(selector_string);
			std::vector<variant> v;
			for(auto& el : el_vec) {
				v.emplace_back(el.get());
			}
			return variant(&v);
		END_DEFINE_FN

		BEGIN_DEFINE_FN(rebuildTree, "() ->commands")
			return variant(new game_logic::FnCommandCallable("dom::rebuildTree", [=]() {
				obj.doc_->rebuildTree();
			}));
		END_DEFINE_FN
		
	END_DEFINE_CALLABLE(DocumentObject)


	// ElementObject
	ElementObject::ElementObject(const NodePtr& element)
		: element_(element),
		  handlers_(),
		  styles_(nullptr)
	{
		ASSERT_LOG(element_ != nullptr && element_->id() == NodeId::ELEMENT, "Tried to construct an ElementObject, without a valid Node.");
		handlers_.resize(static_cast<int>(EventHandlerId::MAX_EVENT_HANDLERS));

		auto id_attr = element_->getAttribute("id");
		ASSERT_LOG(element_->getStylePointer() != nullptr, "Element Style ptr was null: " << element_->toString());
		styles_.reset(new StyleObject(element_->getStylePointer()));
	}

	/*void ElementObject::setHandler(EventHandlerId evtname, const game_logic::FormulaPtr& handler)
	{
		int index = static_cast<int>(evtname);
		ASSERT_LOG(index < static_cast<int>(handlers_.size()), "Handler index exceeds bounds. " << index << " >= " << static_cast<int>(handlers_.size()));
		handlers_[index] = handler;
	}*/

	void ElementObject::setScript(EventHandlerId evtname, const variant& script)
	{
		int index = static_cast<int>(evtname);
		ASSERT_LOG(index < static_cast<int>(handlers_.size()), "Handler index exceeds bounds. " << index << " >= " << static_cast<int>(handlers_.size()));
		handlers_[index] = script;
	}

	variant ElementObject::getScript(EventHandlerId evtname) const
	{
		int index = static_cast<int>(evtname);
		ASSERT_LOG(index < static_cast<int>(handlers_.size()), "Handler index exceeds bounds. " << index << " >= " << static_cast<int>(handlers_.size()));
		return handlers_[index];
	}

	void ElementObject::runHandler(EventHandlerId evtname, game_logic::FormulaCallable* environment, const variant& params)
	{
		int index = static_cast<int>(evtname);
		ASSERT_LOG(index < static_cast<int>(handlers_.size()), "Handler index exceeds bounds. " << index << " >= " << static_cast<int>(handlers_.size()));
		auto& script = handlers_[index];
		if(!script.is_null()) {
			game_logic::FormulaPtr handler = environment->createFormula(script);

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
		callable->add("screenY", variant(my));
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

	void ElementObject::surrenderReferences(GarbageCollector* collector)
	{
		collector->surrenderPtr(&styles_, "XHTML::STYLE_OBJECT");
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(ElementObject)
		
		DEFINE_FIELD(tagName, "string")
			return variant(obj.element_->getTag());

		DEFINE_FIELD(style, "builtin style_object|null")
			if(obj.styles_ == nullptr) {
				return variant();
			}
			return variant(obj.styles_.get());

		DEFINE_FIELD(attributes, "{ string -> string }")
			std::map<variant, variant> res;
			for(auto& a : obj.element_->getAttributes()) {
				res[variant(a.first)] = variant(a.second->getValue());
			}
			return variant(&res);

		DEFINE_FIELD(innerHTML, "string")
			return variant(obj.element_->writeXHTML());
		DEFINE_SET_FIELD
			obj.element_->setInnerXHTML(value.as_string());

		BEGIN_DEFINE_FN(getAttribute, "(string) ->string|null")
			const std::string attr_name = FN_ARG(0).as_string();
			auto attr = obj.element_->getAttribute(attr_name);
			if(attr == nullptr) {
				return variant();
			}
			return variant(attr->getValue());
		END_DEFINE_FN

		BEGIN_DEFINE_FN(hasAttribute, "(string) ->bool")
			const std::string attr_name = FN_ARG(0).as_string();
			return variant::from_bool(obj.element_->getAttribute(attr_name) != nullptr);
		END_DEFINE_FN

		BEGIN_DEFINE_FN(setAttribute, "(string, string) ->commands")
			const std::string attr_name = FN_ARG(0).as_string();
			const std::string attr_value = FN_ARG(1).as_string();

			ffl::IntrusivePtr<const ElementObject> ptr(&obj);
			return variant(new game_logic::FnCommandCallable("dom::setAttribute", [ptr, attr_name, attr_value]() {
				ptr->element_->setAttribute(attr_name, attr_value);
			}));
		END_DEFINE_FN

	END_DEFINE_CALLABLE(ElementObject)

	StyleObject::StyleObject(const StyleNodePtr& styles)
		: style_node_(styles)
	{
	}

	void StyleObject::surrenderReferences(GarbageCollector* collector)
	{
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(StyleObject)

		DEFINE_FIELD(color, "string")
			std::stringstream ss; 
			ss << *obj.style_node_->getColor();
			return variant(ss.str());
		DEFINE_SET_FIELD
			obj.style_node_->setPropertyFromString(css::Property::COLOR, value.as_string());

		DEFINE_FIELD(backgroundColor, "string")
			std::stringstream ss; 
			ss << *obj.style_node_->getBackgroundColor();
			return variant(ss.str());
		DEFINE_SET_FIELD
			obj.style_node_->setPropertyFromString(css::Property::BACKGROUND_COLOR, value.as_string());

		DEFINE_FIELD(width, "string")
			std::stringstream ss; 
			ss << obj.style_node_->getWidth()->toString(css::Property::WIDTH);
			return variant(ss.str());
		DEFINE_SET_FIELD
			obj.style_node_->setPropertyFromString(css::Property::WIDTH, value.as_string());

		DEFINE_FIELD(height, "string")
			std::stringstream ss; 
			ss << obj.style_node_->getHeight()->toString(css::Property::HEIGHT);
			return variant(ss.str());
		DEFINE_SET_FIELD
			obj.style_node_->setPropertyFromString(css::Property::HEIGHT, value.as_string());

		DEFINE_FIELD(position, "string")
			return variant(obj.style_node_->getPositionStyle()->toString(css::Property::POSITION));
		DEFINE_SET_FIELD
			obj.style_node_->setPropertyFromString(css::Property::POSITION, value.as_string());

		DEFINE_FIELD(display, "string")
			return variant(obj.style_node_->getDisplayStyle()->toString(css::Property::DISPLAY));
		DEFINE_SET_FIELD
			obj.style_node_->setPropertyFromString(css::Property::DISPLAY, value.as_string());

		DEFINE_FIELD(backgroundRepeat, "string")
			return variant(obj.style_node_->getBackgroundRepeatStyle()->toString(css::Property::BACKGROUND_REPEAT));
		DEFINE_SET_FIELD
			obj.style_node_->setPropertyFromString(css::Property::BACKGROUND_REPEAT, value.as_string());

	END_DEFINE_CALLABLE(StyleObject)
}
