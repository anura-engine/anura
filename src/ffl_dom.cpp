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

#include "ffl_dom.hpp"
#include "module.hpp"
#include "profile_timer.hpp"
#include "screen_handling.hpp"
#include "variant_utils.hpp"

namespace xhtml
{
	namespace
	{
		const std::string default_user_agent_style_sheet = "data/user_agent.css";
	}

	using namespace KRE;
	DocumentObject::DocumentObject(const variant& v, game_logic::FormulaCallable* environment)
		: environment_(environment), 
		  scene_(SceneGraph::create("xhtml::DocumentObject")),
		  root_(scene_->getRootNode()),
		  rmanager_(),
		  last_process_time_(-1),
		  doc_(nullptr),
		  style_tree_(nullptr),
		  display_list_(nullptr),
		  doc_name_(),
		  ss_name_()
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

		auto user_agent_style_sheet = std::make_shared<css::StyleSheet>();
		css::Parser::parse(user_agent_style_sheet, sys::read_file(module::map_file(ss_name_)));

		auto doc_frag = xhtml::parse_from_file(doc_name_);
		doc_ = Document::create(user_agent_style_sheet);
		doc_->addChild(doc_frag);
		doc_->processStyles();
		// whitespace can only be processed after applying styles.
		doc_->processWhitespace();

		display_list_ = std::make_shared<DisplayList>(scene_);
		root_->attachNode(display_list_);
		doc_->triggerLayout();

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
			auto& gs = graphics::GameScreen::get();
			layout = xhtml::Box::createLayout(style_tree_, gs.getWidth(), gs.getHeight());
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
	
	bool DocumentObject::handleEvents(const SDL_Event& e)
	{
		bool claimed = false;
		if(e.type == SDL_MOUSEMOTION) {
			claimed = doc_->handleMouseMotion(false, e.motion.x, e.motion.y);
		} else if(e.type == SDL_MOUSEBUTTONDOWN) {
			claimed = doc_->handleMouseButtonDown(false, e.button.x, e.button.y, e.button.button);
		} else if(e.type == SDL_MOUSEBUTTONUP) {
			claimed = doc_->handleMouseButtonUp(false, e.button.x, e.button.y, e.button.button);
		}

		return claimed;
	}

	void DocumentObject::surrenderReferences(GarbageCollector* collector)
	{
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(DocumentObject)
		DEFINE_FIELD(dummy, "null")
			return variant();
	END_DEFINE_CALLABLE(DocumentObject)
}
