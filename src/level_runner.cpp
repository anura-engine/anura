/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <math.h>
#include <climits>

#include <boost/algorithm/string/replace.hpp>

#include "CameraObject.hpp"
#include "Canvas.hpp"
#include "Font.hpp"
#include "DisplayDevice.hpp"
#include "ModelMatrixScope.hpp"
#include "WindowManager.hpp"

#include "background_task_pool.hpp"
#include "base64.hpp"
#include "clipboard.hpp"
#include "collision_utils.hpp"
#include "controls.hpp"
#include "custom_object.hpp"
#include "custom_object_functions.hpp"
#include "debug_console.hpp"
#include "draw_scene.hpp"
#ifndef NO_EDITOR
#include "editor.hpp"
#endif
#include "filesystem.hpp"
#include "formatter.hpp"
#include "formula_profiler.hpp"
#include "formula_callable.hpp"
#include "http_client.hpp"
#ifdef TARGET_BLACKBERRY
#include "userevents.h"
#endif
#include "input.hpp"
#include "joystick.hpp"
#include "json_parser.hpp"
#include "level_runner.hpp"
#include "light.hpp"
#include "load_level.hpp"
#include "message_dialog.hpp"
#include "module.hpp"
#include "object_events.hpp"
#include "pause_game_dialog.hpp"
#include "player_info.hpp"
#include "preferences.hpp"
#include "profile_timer.hpp"
#include "screen_handling.hpp"
#include "settings_dialog.hpp"
#include "sound.hpp"
#include "stats.hpp"
#include "surface_cache.hpp"
#include "tbs_internal_server.hpp"
#include "theme_imgui.hpp"
#include "user_voxel_object.hpp"
#include "utils.hpp"
#include "variant_utils.hpp"
#include "globals.h"

extern bool g_mouse_event_swallowed;

extern std::map<std::string, variant> g_user_info_registry;

PREF_STRING(editor_object, "", "Object to use for the editor");

extern bool g_desktop_fullscreen;
extern bool g_particle_editor;
extern int g_vsync;

void auto_select_resolution(const KRE::WindowPtr& wm, int *width, int *height, bool reduce);

namespace 
{
	PREF_STRING(play_music_function, "", "");

	void play_music_track(EntityPtr obj, const std::string& track)
	{
		if(g_play_music_function.empty() == false) {
			ASSERT_LOG(obj.get(), "No object to play music");
			static game_logic::FormulaPtr ffl(new game_logic::Formula(variant(g_play_music_function), &get_custom_object_functions_symbol_table()));

			game_logic::MapFormulaCallablePtr callable(new game_logic::MapFormulaCallable);
			callable->add("music", variant(track));
			callable->add("obj", variant(obj.get()));

			variant cmd = ffl->execute(*callable);
			obj->executeCommand(cmd);

		} else {
			sound::play_music(track);
		}
	}

	std::vector<std::pair<std::function<void()>,void*>> process_functions;
	std::deque<std::function<void()>> asynchronous_work_items_;

	PREF_BOOL(editor_pause, false, "If true, the editor auto pauses when started");
	PREF_INT(time_quota_async_work_items, 10, "Number of milliseconds allowed each frame for asynchronous/background work items to run");

	PREF_BOOL(allow_debug_console_clicking, true, "Allow clicking on objects in the debug console to select them");
	PREF_BOOL(reload_modified_objects, false, "Reload object definitions when their file is modified on disk");
	PREF_INT(mouse_drag_threshold, 5, "Threshold for how much motion can take place in a mouse drag");

	PREF_FLOAT(global_scale, 1.0f, "Global scale value.");

	PREF_BOOL(allow_builtin_settings_dialog, true, "Allow the builtin Anura settings dialog");

	PREF_INT(max_frame_skips, 3, "Maximum frames to skip due to performance");
	
	PREF_STRING(editor_controller, "", "Object used when the editor is started");
	PREF_BOOL_PERSISTENT(skip_odd_frames, false, "Skips every other frame");

	PREF_BOOL(theme_imgui_ui, false, "Displays a dialog to customize the ImGui User Interface.");

	LevelRunner* current_level_runner = nullptr;

	class current_level_runner_scope 
	{
		LevelRunner* old_;
	public:
		current_level_runner_scope(LevelRunner* value) 
			: old_(current_level_runner)
		{
			current_level_runner = value;
		}

		~current_level_runner_scope() {
			current_level_runner = old_;
		}
	};

	struct upload_screenshot_info 
	{
		upload_screenshot_info() 
			: error(false), done(false), client(new http_client("www.theargentlark.com", "80"))
		{
			client->set_timeout_and_retry();
		}
		void finished(std::string response, bool is_error) {
			LOG_INFO("Finished(" << is_error << ", " << response << ")");
			result = response;
			error = is_error;
			done = true;
		}
		std::string result;
		bool error;
		bool done;

		ffl::IntrusivePtr<http_client> client;
	};

	void upload_screenshot(std::string file, std::shared_ptr<upload_screenshot_info> info)
	{
		// XXX we should read the server address from some sort of configuration file.
		using std::placeholders::_1;
		std::string user_parm;
		if(g_user_info_registry["user"].is_string()) {
			user_parm = "&user=" + g_user_info_registry["user"].as_string();
		}
		info->client->send_request("POST /cgi-bin/upload-screenshot.pl?module=" + module::get_module_name() + user_parm, 
			base64::b64encode(sys::read_file(file)), 
			std::bind(&upload_screenshot_info::finished, info.get(), _1, false),
			std::bind(&upload_screenshot_info::finished, info.get(), _1, true),
			[](size_t,size_t,bool){});
		while(!info->done) {
			info->client->process();
		}
	}

	void done_upload_screenshot(std::shared_ptr<upload_screenshot_info> info)
	{
		try {
			if(info->error == false) {
				LOG_INFO("DONE UPLOAD SCREENSHOT (" << info->result << ")");
				variant v = json::parse(info->result, json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
				debug_console::addMessage(formatter() << "Uploaded screenshot to " << v["url"].as_string() << " (set url in clipboard)");;
				copy_to_clipboard(v["url"].as_string());
			}
		} catch(...) {
			info->error = true;
		}

		if(info->error) {
			debug_console::addMessage("error uploading screenshot");
		}
	}

	int skipping_game = 0;

	int global_pause_time;

	typedef std::function<void(const Level&, screen_position&, float)> TransitionFn;
		
	//prepare to call transition_scene by making sure that frame buffers are
	//filled with the image of the screen.
	void prepare_transition_scene(Level& lvl, screen_position& screen_pos)
	{
		draw_scene(lvl, screen_pos);
		KRE::WindowManager::getMainWindow()->swap();
	}

	void transition_scene(Level& lvl, screen_position& screen_pos, bool transition_out, const std::string& transition_type) 
	{
		const int num_frames = lvl.setup_level_transition(transition_type);

		if(lvl.player()) {
			lvl.player()->getEntity().setInvisible(true);
		}

		Level::set_level_transition_ratio(decimal(0.0f));

		const int start_time = profile::get_tick_time();
		auto wnd = KRE::WindowManager::getMainWindow();

		for(int n = 0; n <= num_frames; ++n) {
			Level::set_level_transition_ratio(decimal(static_cast<float>(n)/static_cast<float>(num_frames)));
			lvl.process_draw();
			render_scene(lvl, screen_pos);
			KRE::WindowManager::getMainWindow()->swap();

			const int target_end_time = start_time + (n+1)*preferences::frame_time_millis();
			const int current_time = profile::get_tick_time();
			const int skip_time = target_end_time - current_time;
			if(skip_time > 0) {
				profile::delay(skip_time);
			}
		}

		Level::set_level_transition_ratio(decimal(0.0f));
	
		if(lvl.player()) {
			lvl.player()->getEntity().setInvisible(false);
		}
	}

	void fade_scene(const Level& lvl, screen_position& screen_pos, float fade) 
	{
		auto& gs = graphics::GameScreen::get();
		KRE::Canvas::getInstance()->drawSolidRect(rect(0,0,gs.getVirtualWidth(),gs.getVirtualHeight()),KRE::Color(0.0f, 0.0f, 0.0f, 0.5f*fade));
	}

	void flip_scene(const Level& lvl, screen_position& screen_pos, float amount) 
	{
		screen_pos.flip_rotate = static_cast<int>(amount*1000);
		draw_scene(lvl, screen_pos);
	}

	void show_end_game()
	{
		auto& gs = graphics::GameScreen::get();
		const std::string msg = "to be continued...";
		auto t = KRE::Font::getInstance()->renderText(msg, KRE::Color::colorWhite(), 48);
		auto wnd = KRE::WindowManager::getMainWindow();
		auto canvas = KRE::Canvas::getInstance();
		const int xpos = gs.getVirtualWidth()/2 - t->width()/2;
		const int ypos = gs.getVirtualHeight()/2 - t->height()/2;
		for(unsigned n = 0; n <= msg.size(); ++n) {
			const float percent = static_cast<float>(n)/static_cast<float>(msg.size());
			canvas->drawSolidRect(rect(0, 0, gs.getVirtualWidth(), gs.getVirtualHeight()), KRE::Color::colorBlack());
			canvas->blitTexture(t, rect(0,0,static_cast<int>(percent*gs.getVirtualWidth()),0), 0, 
				rect(xpos, ypos,static_cast<int>(t->width()*percent), t->height()));
			wnd->swap();
			profile::delay(40);
		}

		bool done = false;
		while(!done) {
			SDL_Event event;
			while(input::sdl_poll_event(&event)) {
				switch(event.type) {
				case SDL_QUIT:
				case SDL_KEYDOWN:
					done = true;
					break;
				}
			}
			joystick::update();
			for(int n = 0; n != 6; ++n) {
				if(joystick::button(n)) {
					done = true;
				}
			}
		}
	}
}

namespace {
int TranslateCoordinatesPhysicalToVirtual(int pos, int virt, int phys) {
	return (pos * virt) / phys;
}
}

void mapSDLEventScreenCoordinatesToVirtual(SDL_Event& event)
{
	switch(event.type) {
	case SDL_MOUSEMOTION: {
		auto& gs = graphics::GameScreen::get();
		event.motion.x = TranslateCoordinatesPhysicalToVirtual(event.motion.x, gs.getVirtualWidth(), gs.getWidth());
		event.motion.y = TranslateCoordinatesPhysicalToVirtual(event.motion.y, gs.getVirtualHeight(), gs.getHeight());
		event.motion.xrel = TranslateCoordinatesPhysicalToVirtual(event.motion.xrel, gs.getVirtualWidth(), gs.getWidth());
		event.motion.yrel = TranslateCoordinatesPhysicalToVirtual(event.motion.yrel, gs.getVirtualHeight(), gs.getHeight());
		break;
	}

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP: {
		auto& gs = graphics::GameScreen::get();
		event.button.x = TranslateCoordinatesPhysicalToVirtual(event.button.x, gs.getVirtualWidth(), gs.getWidth());
		event.button.y = TranslateCoordinatesPhysicalToVirtual(event.button.y, gs.getVirtualHeight(), gs.getHeight());
		break;
	}
	}
}

void addProcessFunction(std::function<void()> fn, void* tag)
{
	process_functions.push_back(std::pair<std::function<void()>,void*>(fn, tag));
}

void removeProcessFunction(void* tag)
{
	process_functions.erase(std::remove_if(process_functions.begin(), process_functions.end(), [&tag](const std::pair<std::function<void()>,void*>& p) { return p.second == tag; }), process_functions.end());
}

void addAsynchronousWorkItem(std::function<void()> fn)
{
	asynchronous_work_items_.push_back(fn);
}

void begin_skipping_game() 
{
	++skipping_game;
}

void end_skipping_game() 
{
	skipping_game = 0;
}

bool is_skipping_game() 
{
	return skipping_game > 0;
}

// XXX We should handle the window resize event in the WindowManager code
void video_resize(const SDL_Event &event) 
{
	if(preferences::get_screen_mode() == preferences::ScreenMode::WINDOWED) {
		int width = event.window.data1;
		int height = event.window.data2;

		const float aspect = graphics::GameScreen::get().getAspectRatio();
		const float wa = static_cast<float>(width) * aspect;
		const float ha = static_cast<float>(height) * aspect;

		if(graphics::GameScreen::get().getSquareArea() < width * height) {
			//making the window larger
			if(ha > static_cast<float>(width)) {
				width = static_cast<int>(ha);
			} else if(ha < static_cast<float>(width)) {
				height = static_cast<int>(wa);
			}
		} else {
			//making the window smaller
			if(ha > static_cast<float>(width)) {
				height = static_cast<int>(wa);
			} else if(ha < static_cast<float>(width)) {
				width = static_cast<int>(ha);
			}
		}

		//make sure we don't have some ugly fractional aspect ratio
		while(static_cast<float>(width)/static_cast<float>(height) != aspect) {
			++width;
			height = static_cast<int>(static_cast<float>(width)/aspect);
		}

		// XXX If we're in the editor this isn't right.
		graphics::GameScreen::get().setDimensions(width, height);

		KRE::WindowManager::getMainWindow()->notifyNewWindowSize(width, height);
	}
}

void LevelRunner::video_resize_event(const SDL_Event &event)
{
	// event.window.data1/event.window.data2 may not reflect the correct window size.
	static const int WindowResizeEventID = get_object_event_id("window_resize");
	game_logic::MapFormulaCallablePtr callable(new game_logic::MapFormulaCallable);
	callable->add("width", variant(KRE::WindowManager::getMainWindow()->width()));
	callable->add("height", variant(KRE::WindowManager::getMainWindow()->height()));
	lvl_->player()->getEntity().handleEvent(WindowResizeEventID, callable.get());
}

void LevelRunner::on_player_set(EntityPtr e)
{
#ifndef NO_EDITOR
	if(console_.get()) {
		console_->setFocus(e);
	}
#endif
}

#if 0
void LevelRunner::handle_mouse_over_voxel_objects(const SDL_Event &event,
	const std::vector<voxel::UserVoxelObjectPtr>& voxel_objs, 
	game_logic::MapFormulaCallablePtr callable, 
	const int basic_evt, 
	const int catch_all_event)
{
	static const int MouseEnterID = get_object_event_id("mouse_enter");
	static const int MouseLeaveID = get_object_event_id("mouse_leave");

	if(!lvl_->iso_world()) {
		return;
	}

	std::set<voxel::UserVoxelObjectPtr> mouse_in;

	for(auto obj : voxel_objs) {
		if(event.type == SDL_MOUSEBUTTONDOWN) {
		} else if(event.type == SDL_MOUSEMOTION) {
			if(obj->is_mouseover_object() == false) {
				obj->set_mouseover_object();
				obj->handleEvent(MouseEnterID, callable.get());
			}
			mouse_in.insert(obj);
		}
		obj->handleEvent(basic_evt, callable.get());
	}

	for(auto obj : lvl_->iso_world()->getObjects()) {
		obj->handleEvent(catch_all_event, callable.get());

		if(event.type == SDL_MOUSEMOTION) {
			if(mouse_in.find(obj) == mouse_in.end() && obj->is_mouseover_object()) {
				obj->set_mouseover_object(false);
				obj->handleEvent(MouseLeaveID, callable.get());
			}
		}
	}
}
#endif

bool LevelRunner::handle_mouse_events(const SDL_Event &event)
{
	static const int MouseDownEventID = get_object_event_id("mouse_down");
	static const int MouseUpEventID = get_object_event_id("mouse_up");
	static const int MouseMoveEventID = get_object_event_id("mouse_move");
	static const int MouseDownEventAllID = get_object_event_id("mouse_down*");
	static const int MouseUpEventAllID = get_object_event_id("mouse_up*");
	static const int MouseMoveEventAllID = get_object_event_id("mouse_move*");


	static const int MouseEnterID = get_object_event_id("mouse_enter");
	static const int MouseLeaveID = get_object_event_id("mouse_leave");

	static const int MouseClickID = get_object_event_id("click");
	//static const int MouseDblClickID = get_object_event_id("dblclick");
	static const int MouseDragID = get_object_event_id("drag");
	static const int MouseDragStartID = get_object_event_id("drag_start");
	static const int MouseDragEndID = get_object_event_id("drag_end");

	static const int MouseWheelID = get_object_event_id("mouse_wheel");

	g_mouse_event_swallowed = false;

	if(paused) {
		// skip mouse event handling when paused.
		// XXX: when we become unpaused we need to reset the state of drag operations
		// and partial clicks.
		return false;
	}

	const int DragThresholdPx = g_mouse_drag_threshold;

	switch(event.type)
	{
		case SDL_MOUSEWHEEL: {
			game_logic::MapFormulaCallablePtr callable(new game_logic::MapFormulaCallable);
			callable->add("yscroll", variant(event.wheel.y));
			std::vector<EntityPtr> chars = lvl_->get_active_chars();
			for(auto e : chars) {
				e->handleEvent(MouseWheelID, callable.get());
			}
			break;
		}

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			mouse_drag_count_ = 0;

		case SDL_MOUSEMOTION: {
		    int x, mx = event.type == SDL_MOUSEMOTION ? event.motion.x : event.button.x;
			int y, my = event.type == SDL_MOUSEMOTION ? event.motion.y : event.button.y;
			int event_type = event.type;
			int event_button_button = event.button.button;
            
			x = mx;
			y = my;

			const int basic_evt = event_type == SDL_MOUSEBUTTONDOWN
				? MouseDownEventID 
				: event_type == SDL_MOUSEMOTION
					? MouseMoveEventID : MouseUpEventID;
			const int catch_all_event = event_type == SDL_MOUSEBUTTONDOWN 
				? MouseDownEventAllID 
				: event_type == SDL_MOUSEMOTION
					? MouseMoveEventAllID : MouseUpEventAllID;
			Uint8 button_state = input::sdl_get_mouse_state(0,0);

			x += last_draw_position().x/100;
			y += last_draw_position().y/100;
			game_logic::MapFormulaCallablePtr callable(new game_logic::MapFormulaCallable);
			callable->add("mouse_x", variant(x));
			callable->add("mouse_y", variant(y));
			if(event_type != SDL_MOUSEMOTION) {
				callable->add("mouse_button", variant(event_button_button));
			} else {
				callable->add("mouse_button", variant(button_state));
			}

			std::vector<EntityPtr> wcs;

			std::vector<variant> items;
			// Grab characters around point, z-order sort them, so that when
			// we process them we go from highest to lowest, allowing a higher
			// object to swallow an event before the lower ones get it.
			std::vector<EntityPtr> cs = lvl_->get_characters_at_point(x, y, last_draw_position().x/100, last_draw_position().y/100);
			cs.insert(cs.end(), wcs.begin(), wcs.end());

			//zorder_compare sorts lowest-to-highest, so we do that
			//then reverse.
			std::sort(cs.begin(), cs.end(), zorder_compare);
			std::reverse(cs.begin(), cs.end());

			bool handled = false;
			bool click_handled = false;
			std::set<EntityPtr> mouse_in;
			for(auto& it : cs) {
				EntityPtr& e = it;
				rect m_area = e->getMouseOverArea();
				m_area += point(e->x(), e->y());
				// n.b. mouse_over_area is relative to the object.
				if(m_area.w() != 0) {
					point p(x,y);
					if(e->useAbsoluteScreenCoordinates()) {
						p = point(mx + lvl_->absolute_object_adjust_x(),my + lvl_->absolute_object_adjust_y());
					}
					if(pointInRect(p, m_area) == false) {
						continue;
					}
				}

				rect clip_area;
				if(e->getClipArea(&clip_area)) {
					point p(x,y);
					if(e->useAbsoluteScreenCoordinates()) {
						int xadj = 0, yadj = 0;
						auto& gs = graphics::GameScreen::get();
						if(gs.getVirtualWidth() > lvl_->boundaries().w()) {
							xadj = (gs.getVirtualWidth() - lvl_->boundaries().w());
						}

						if(gs.getVirtualHeight() > lvl_->boundaries().h()) {
							yadj = (gs.getVirtualHeight() - lvl_->boundaries().h());
						}

						p = point(mx + lvl_->absolute_object_adjust_x() - xadj,my + lvl_->absolute_object_adjust_y() - yadj);

					}
					if(pointInRect(p, clip_area) == false) {
						continue;
					}
				}

				if(event_type == SDL_MOUSEBUTTONDOWN) {
					e->setMouseButtons(e->getMouseButtons() | SDL_BUTTON(event_button_button));
				} else if(event_type == SDL_MOUSEMOTION) {

					mouse_drag_count_ += abs(event.motion.xrel) + abs(event.motion.yrel);

					// handling for mouse_enter
					if(e->isMouseOverEntity() == false) {
						if((e->getMouseoverDelay() == 0 || static_cast<unsigned>(lvl_->cycle()) > e->getMouseoverTriggerCycle())) {
							e->handleEvent(MouseEnterID, callable.get());
							e->setMouseOverEntity();
						} else if(e->getMouseoverTriggerCycle() == std::numeric_limits<int>::max()) {
							e->setMouseoverTriggerCycle(e->getMouseoverDelay() + lvl_->cycle());
						}
					}
					mouse_in.insert(e);
				}

				if(e->isMouseOverEntity() || basic_evt != MouseMoveEventID) {
					//only give mouse move events if we've actually
					//recordered a mouse_enter event.
					handled |= e->handleEvent(basic_evt, callable.get());
				}

				if(event_type == SDL_MOUSEBUTTONUP && mouse_clicking_ && !click_handled && e->isBeingDragged() == false && mouse_drag_count_ <= DragThresholdPx) {
					e->handleEvent(MouseClickID, callable.get());
					if(g_mouse_event_swallowed) {
						click_handled = true;
						g_mouse_event_swallowed = false;
					}
				}
				items.push_back(variant(e.get()));
			}
			// Handling for "catch all" mouse events.
			callable->add("handled", variant::from_bool(handled));
			variant obj_ary(&items);
			callable->add("objects_under_mouse", obj_ary);
			std::vector<EntityPtr> level_chars(Level::current().get_chars());
			//make events happen with highest zorder objects first.
			std::sort(level_chars.begin(), level_chars.end(), zorder_compare);
			std::reverse(level_chars.begin(), level_chars.end());

			bool drag_handled = false;
			for(EntityPtr object : level_chars) {
				if(object) {
					object->handleEvent(catch_all_event, callable.get());

					// drag handling
					if(event_type == SDL_MOUSEBUTTONUP && !drag_handled) {
						object->setMouseButtons(object->getMouseButtons() & ~SDL_BUTTON(event_button_button));
						if(object->getMouseButtons() == 0 && object->isBeingDragged()) {
							object->handleEvent(MouseDragEndID, callable.get());
							object->setBeingDragged(false);
							if(g_mouse_event_swallowed) {
								drag_handled = true;
								g_mouse_event_swallowed = false;
							}
						}
					} else if(event_type == SDL_MOUSEMOTION && !drag_handled) {
						// drag check.
						if(object->isBeingDragged()) {
							if(object->getMouseButtons() & button_state) {
								object->handleEvent(MouseDragID, callable.get());
							} else {
								object->handleEvent(MouseDragEndID, callable.get());
								object->setBeingDragged(false);
							}
							if(g_mouse_event_swallowed) {
								drag_handled = true;
								g_mouse_event_swallowed = false;
							}
						} else if(object->getMouseButtons() & button_state && mouse_drag_count_ > object->mouseDragThreshold(DragThresholdPx)) {
							// start drag.
							object->handleEvent(MouseDragStartID, callable.get());
							object->setBeingDragged();
							if(g_mouse_event_swallowed) {
								drag_handled = true;
								g_mouse_event_swallowed = false;
							}
						}
					}
				}
			}

			if(event_type == SDL_MOUSEMOTION) {

				// handling for mouse_leave
				level_chars = Level::current().get_chars();

				//make events happen with highest zorder objects first.
				std::sort(level_chars.begin(), level_chars.end(), zorder_compare);
				std::reverse(level_chars.begin(), level_chars.end());

				for(const EntityPtr& e : level_chars) {
					if(!e) {
						continue;
					}

					rect m_area = e->getMouseOverArea();
					m_area += point(e->x(), e->y());
					bool has_m_area = m_area.w() != 0;
					point p(x,y);
					if(e->useAbsoluteScreenCoordinates()) {
						p = point(mx + lvl_->absolute_object_adjust_x(),my + lvl_->absolute_object_adjust_y());
					}

					if(mouse_in.find(e) == mouse_in.end()) {
						e->setMouseoverTriggerCycle(std::numeric_limits<int>::max());
					}

					if(mouse_in.find(e) == mouse_in.end()) {
						if(has_m_area == false) {
							if(e->isMouseOverEntity()) {
								e->handleEvent(MouseLeaveID, callable.get());
								e->setMouseOverEntity(false);
							}
						} else {
							if(pointInRect(p, m_area) == false && e->isMouseOverEntity()) {
								e->handleEvent(MouseLeaveID, callable.get());
								e->setMouseOverEntity(false);
							}
						}								
					}
				}
			}

			if(event.type == SDL_MOUSEMOTION && mouse_drag_count_ <= DragThresholdPx) {
				break;
			}

			mouse_clicking_ = event.type == SDL_MOUSEBUTTONDOWN;
			break;
		} //end mouse motion event case
	}
	return false;
}

extern bool g_enable_graphical_fonts;

void LevelRunner::show_pause_title()
{
	if(!editor_ && g_enable_graphical_fonts) {
		set_scene_title("Paused\n\n\n(ctrl-p)", paused ? std::numeric_limits<int>::max() : 25);
	}
}

LevelRunner* LevelRunner::getCurrent()
{
	return current_level_runner;
}

LevelRunner::LevelRunner(LevelPtr& lvl, std::string& level_cfg, std::string& original_level_cfg)
  : lvl_(lvl), 
	level_cfg_(level_cfg), 
	original_level_cfg_(original_level_cfg),
#ifndef NO_EDITOR
	history_trails_state_id_(-1), 
	object_reloads_state_id_(-1),
	tile_rebuild_state_id_(-1),
#endif
	editor_(nullptr)
{
	quit_ = false;
	force_return_ = false;

	current_second_ = time(nullptr);
	current_max_ = 0;
	next_max_ = 0;
	current_fps_ = 0;
	next_fps_ = 0;
	current_cycles_ = 0;
	next_cycles_ = 0;
	current_delay_ = 0;
	next_delay_ = 0;
	current_draw_ = 0;
	next_draw_ = 0;
	current_flip_ = 0;
	next_flip_ = 0;
	current_process_ = 0;
	next_process_ = 0;
	current_events_ = 0;

	nskip_draw_ = 0;

	cycle = 0;
	die_at = -1;
	paused = false;
	done = false;
	start_time_ = profile::get_tick_time();
	pause_time_ = -global_pause_time;
	mouse_clicking_ = false;
	mouse_drag_count_ = 0;
}

void LevelRunner::start_editor()
{
#ifndef NO_EDITOR
	if(!editor_) {
		controls::control_backup_scope ctrl_backup;
		editor_ = editor::get_editor(lvl_->id().c_str());
		editor_resolution_manager_.reset(new EditorResolutionManager(editor_->xres(), editor_->yres()));
		editor_->set_playing_level(lvl_);
		editor_->setup_for_editing();
		lvl_->set_editor();
		lvl_->setAsCurrentLevel();
		lvl_->player()->getEntity().handleEvent("open_editor");
		initHistorySlider();

		if(g_editor_object.empty() == false) {
			ffl::IntrusivePtr<CustomObject> obj(new CustomObject(g_editor_object, 0, 0, 1));
			obj->construct();
			lvl_->add_character(obj);
		}

		if(g_editor_pause) {
			start_editor();
		}
	} else {
		//Pause the game and set the level to its original
		//state if the user presses ctrl+e twice.
		paused = !paused;
		show_pause_title();
		editor_->reset_playing_level(false);
		last_draw_position().init = false;
		initHistorySlider();
		if(!paused) {
			controls::read_until(lvl_->cycle());
		}
	}
#endif
}

void LevelRunner::close_editor()
{
	LOG_DEBUG("LevelRunner::close_editor()");
#ifndef NO_EDITOR
	if(editor_->mouselook_mode()) {
		SDL_SetRelativeMouseMode(SDL_FALSE);
	}
	editor_ = nullptr;
	editor_resolution_manager_.reset();
	history_slider_.reset();
	history_button_.reset();
	history_trails_.clear();
	lvl_->mutateValue("zoom", variant(1));
	lvl_->set_editor(false);
	lvl_->player()->getEntity().handleEvent("close_editor");
	paused = false;
	show_pause_title();
//	controls::read_until(lvl_->cycle());
	initHistorySlider();
#endif
}

bool LevelRunner::play_level()
{
	const current_level_runner_scope current_level_runner_setter(this);

	sound::stop_looped_sounds(nullptr);

	lvl_->setAsCurrentLevel();
	bool reversing = false;

	if(preferences::edit_on_start()) {
		start_editor();	
	}

	if(!lvl_->music().empty()) {
		EntityPtr e;
		if(lvl_->player()) {
			e = EntityPtr(&lvl_->player()->getEntity());
		}
		play_music_track(e, lvl_->music());
	}

	while(!done && !quit_ && !force_return_) {
		const Uint8 *key = SDL_GetKeyboardState(nullptr);
		if(key[SDL_SCANCODE_T] && preferences::record_history()
#ifndef NO_EDITOR
			&& (!editor_ || !editor_->hasKeyboardFocus())
			&& (!console_ || !console_->hasKeyboardFocus())
#endif
		) {
				if(!reversing) {
					pause_time_ -= profile::get_tick_time();
				}
				reverse_cycle();
				reversing = true;
		} else {
			if(reversing) {
				controls::read_until(lvl_->cycle());
				pause_time_ += profile::get_tick_time();
			}
			reversing = false;
			bool res = play_cycle();
			if(!res) {
				return quit_;
			}

			if(preferences::record_history()) {
				lvl_->backup();
			}
		}
	}

	for(EntityPtr e : lvl_->get_chars()) {
		e->handleEvent(OBJECT_EVENT_BEING_REMOVED);
	}

	return quit_;
}

namespace 
{
	std::set<std::string> g_levels_modified;

	void level_file_modified(std::string lvl_path) 
	{
		g_levels_modified.insert(lvl_path);
	}
}

void process_tbs_matchmaking_server();

bool LevelRunner::play_cycle()
{
	formula_profiler::pump();

	const int start_cycle_time = profile::get_tick_time();

	auto wnd = KRE::WindowManager::getMainWindow();
	static SettingsDialog settingsDialog;

	for(auto& p : process_functions) {
		p.first();
	}

	const preferences::alt_frame_time_scope alt_frame_time_scoper(preferences::has_alt_frame_time() && SDL_GetModState()&KMOD_ALT);
	if(controls::first_invalid_cycle() >= 0) {
		lvl_->replay_from_cycle(controls::first_invalid_cycle());
		controls::mark_valid();
	}

	background_task_pool::pump();

	performance_data current_perf(current_max_,current_fps_,50,0,0,0,0,0,CustomObject::events_handled_per_second,"");

	if(preferences::internal_tbs_server()) {
		tbs::internal_server::process();
	}

	process_tbs_matchmaking_server();

	if(controls::num_players() > 1) {
		lvl_->backup();
	}
	
#if defined(USE_BOX2D)
	box2d::world_ptr world = box2d::world::our_world_ptr();
	if(world && !paused) {
		world->step(1.0f/50.0f);
	}
#endif

	std::unique_ptr<controls::local_controls_lock> controls_lock;
#ifndef NO_EDITOR
	if((editor_ && editor_->hasKeyboardFocus()) ||
	   (console_ && console_->hasKeyboardFocus())) {
		controls_lock.reset(new controls::local_controls_lock);
	}

	if(editor_ && lvl_->player() == nullptr && !paused) {
		//force the game to paused in the editor with no player.
		paused = true;
	}

	static bool pumped_file_mods = false;
	if(editor_ || console_ || pumped_file_mods || g_reload_modified_objects) {
		sys::pump_file_modifications();
		pumped_file_mods = true;
	}

	if(!editor_ && g_reload_modified_objects) {
		CustomObjectType::reloadModifiedCode();
	}

	if(editor_) {
		controls::control_backup_scope ctrl_backup;
		auto& gs = graphics::GameScreen::get();
		editor_->setPos(last_draw_position().x/100 - (editor_->zoom() - 1) * gs.getVirtualWidth()/2, 
			last_draw_position().y/100 - (editor_->zoom() - 1) * gs.getVirtualHeight()/2);
		//editor_->setPos(last_draw_position().x/100 - (editor_->zoom()-1)*(gs.getVirtualWidth()-editor::sidebar_width())/2, last_draw_position().y/100 - (editor_->zoom()-1)*(gs.getVirtualHeight())/2);
		editor_->process();
		lvl_->complete_rebuild_tiles_in_background();
		lvl_->setAsCurrentLevel();

		lvl_->mutateValue("zoom", variant(decimal(1.0/editor_->zoom())));

		CustomObjectType::reloadModifiedCode();
		// XXX graphics::texture::clear_modified_files_from_cache();

		if(lvl_->cycle()%25 == 0) {
			Background::loadModifiedBackgrounds();
		}

		if(history_trails_.empty() == false && (tile_rebuild_state_id_ != Level::tileRebuildStateId() || history_trails_state_id_ != editor_->level_state_id() || object_reloads_state_id_ != CustomObjectType::numObjectReloads())) {
			update_history_trails();
		}

		static std::set<std::string> monitoring_level_files;
		const std::string& level_path = get_level_path(lvl_->id());
		if(monitoring_level_files.count(level_path) == 0) {
			monitoring_level_files.insert(level_path);

			sys::notify_on_file_modification(level_path, std::bind(level_file_modified, level_path));
		}

		if(g_levels_modified.count(level_path)) {
			g_levels_modified.erase(level_path);
// THIS FEATURE DISABLED FOR NOW. TODO: FIX IT. It allows the level to be
// replayed from the start when the level.cfg is modified, allowing
// 'instant updates'. It would occur even if the game saved the level though
// which is bad and makes saves slow.
//			replay_level_from_start();
		}
	}
#endif

	static bool mouselook_state = false;
	if(mouselook_state != lvl_->is_mouselook_enabled() && editor_ == nullptr && !paused) {
		mouselook_state = lvl_->is_mouselook_enabled();
		SDL_SetRelativeMouseMode(lvl_->is_mouselook_enabled() ? SDL_TRUE : SDL_FALSE);
		SDL_GetRelativeMouseState(nullptr, nullptr);
	}
	if(editor_ && mouselook_state) {
		SDL_SetRelativeMouseMode(SDL_FALSE);
		mouselook_state = false;
	}

	const bool is_multiplayer = controls::num_players() > 1;

	int desired_end_time = g_vsync != 0 ? profile::get_tick_time() + preferences::frame_time_millis() :
	                                      start_time_ + pause_time_ + global_pause_time + cycle*preferences::frame_time_millis() + preferences::frame_time_millis();

	if(!is_multiplayer) {
		const int ticks = profile::get_tick_time();
		if(desired_end_time < ticks || alt_frame_time_scoper.active()) {
			const int new_desired_end_time = ticks + preferences::frame_time_millis();
			pause_time_ += new_desired_end_time - desired_end_time;
			desired_end_time = new_desired_end_time;
		}
	}

	//record player movement every minute on average.
#if !defined(MOBILE_BUILD)
	if(rand()%3000 == 0 && lvl_->player()) {
		point p = lvl_->player()->getEntity().getMidpoint();

		if(last_stats_point_level_ == lvl_->id()) {
			stats::Entry("move").addPlayerPos();
		}

		last_stats_point_ = p;
		last_stats_point_level_ = lvl_->id();
	}
#endif

	if(die_at <= 0 && lvl_->players().size() == 1 && lvl_->player() && lvl_->player()->getEntity().getHitpoints() <= 0) {
		die_at = cycle;
	}

	if(editor_ && die_at > 0 && cycle >= die_at + 30) {
#ifndef NO_EDITOR
		die_at = -1;

		//If the player dies in the editor, return this level to its
		//initial state.
		editor_->reset_playing_level(false);
		last_draw_position().init = false;
#endif

	} else if(die_at > 0 && cycle >= die_at + 30) {
		die_at = -1;

		std::vector<EntityPtr> chars = lvl_->get_chars();

		for(EntityPtr e : chars) {
			e->handleEvent(OBJECT_EVENT_PLAYER_DEATH);
		}

		for(EntityPtr e : chars) {
			e->handleEvent(OBJECT_EVENT_BEING_REMOVED);
		}

		//record stats of the player's death
		lvl_->player()->getEntity().recordStatsMovement();
		stats::Entry("die").addPlayerPos();
		last_stats_point_level_ = "";

		EntityPtr save = lvl_->player()->getEntity().saveCondition();
		if(!save) {
			return false;
		}

		prepare_transition_scene(*lvl_, last_draw_position());

		preload_level(save->getPlayerInfo()->currentLevel());
		transition_scene(*lvl_, last_draw_position(), true, "fade");
		sound::stop_looped_sounds(nullptr);
		ffl::IntrusivePtr<Level> new_level = load_level(save->getPlayerInfo()->currentLevel());

		if(!new_level->music().empty()) {
			play_music_track(save, new_level->music());
		}

		set_scene_title(new_level->title());
		new_level->add_player(save);
		new_level->setAsCurrentLevel();
		save->saveGame();
		save->handleEvent(OBJECT_EVENT_LOAD_CHECKPOINT);
		place_entity_in_level(*new_level, *save);
		lvl_ = new_level;
		last_draw_position() = screen_position();

		//trigger a garbage collection of objects now.
		runGarbageCollection();
		reapGarbageCollection();
	} else if(lvl_->players().size() > 1) {
		for(const EntityPtr& c : lvl_->players()) {
			if(c->getHitpoints() <= 0) {
				//in multiplayer we respawn on death
				c->respawnPlayer();
			}
		}
	}

	const Level::portal* portal = lvl_->get_portal();
	if(portal) {
		//we might want to change the portal, so copy it and make it mutable.
		Level::portal mutable_portal = *portal;
		portal = &mutable_portal;

		level_cfg_ = portal->level_dest;
		if(level_cfg_.empty() && portal->level_dest_obj.get() == nullptr) {
			//the portal is within the same level

			if(portal->dest_label.empty() == false) {
				ConstEntityPtr dest_door = lvl_->get_entity_by_label(portal->dest_label);
				if(dest_door) {
					mutable_portal.dest = point(dest_door->x() + dest_door->getTeleportOffsetX()*dest_door->getFaceDir(), dest_door->y() + dest_door->getTeleportOffsetY());
					mutable_portal.dest_starting_pos = false;
				}

			}
			last_draw_position() = screen_position();

			PlayerInfo* player = lvl_->player();
			if(portal->new_playable) {
				game_logic::MapFormulaCallablePtr callable(new game_logic::MapFormulaCallable());
				callable->add("new_playable", variant(portal->new_playable.get()));
				player->getEntity().handleEvent("player_change_on_teleport", callable.get());
				lvl_->add_player(portal->new_playable);
				player = lvl_->player();
			}

			if(player) {
				player->getEntity().setPos(portal->dest);
				if(!player->getEntity().hasNoMoveToStanding() && !portal->no_move_to_standing){
					player->getEntity().moveToStanding(*lvl_);
				}
			}
		} else {
			//the portal is to another level
#ifndef NO_EDITOR
			if(editor_) {
				editor_->confirm_quit(false);
			}
#endif
			
			if (preferences::load_compiled())
			{
				Level::Summary summary = Level::getSummary(level_cfg_);
				if(!summary.music.empty()) {
					EntityPtr e;
					if(lvl_->player()) {
						e = EntityPtr(&lvl_->player()->getEntity());
					}
					play_music_track(e, summary.music);
				}
			}

			prepare_transition_scene(*lvl_, last_draw_position());

			const std::string transition = portal->transition;

			if(transition == "flip") {
				transition_scene(*lvl_, last_draw_position(), true, transition);
			} else if(transition == "instant") {
				//do nothing
			} else if(transition != "fade") {
				transition_scene(*lvl_, last_draw_position(), true, transition);
			} else if(!portal->level_dest_obj) {
				preload_level(level_cfg_);
				transition_scene(*lvl_, last_draw_position(), true, transition);
			}

			sound::stop_looped_sounds(nullptr);

			std::vector<EntityPtr> chars = lvl_->get_chars();
			for(EntityPtr e : chars) {
				e->handleEvent(OBJECT_EVENT_BEING_REMOVED);
			}

			ffl::IntrusivePtr<Level> new_level;
			if(portal->level_dest_obj) {
				new_level = portal->level_dest_obj;
			} else {
				new_level = load_level(level_cfg_);
			}
			if (!preferences::load_compiled() && !new_level->music().empty()) {
				EntityPtr e;
				if(lvl_->player()) {
					e = EntityPtr(&lvl_->player()->getEntity());
				}
				play_music_track(e, new_level->music());
			}

			if(portal->dest_label.empty() == false) {
				//the label of an object was specified as an entry point,
				//so set our position there.
				ConstEntityPtr dest_door = new_level->get_entity_by_label(portal->dest_label);
				if(dest_door) {
					mutable_portal.dest = point(dest_door->x() + dest_door->getTeleportOffsetX()*dest_door->getFaceDir(), dest_door->y() + dest_door->getTeleportOffsetY());
					mutable_portal.dest_starting_pos = false;
				}
			}

			if(editor_) {
				new_level->set_editor();
			}

			new_level->setAsCurrentLevel();

			set_scene_title(new_level->title());
			point dest = portal->dest;
			if(portal->dest_str.empty() == false) {
				dest = new_level->get_dest_from_str(portal->dest_str);
			} else if(portal->dest_starting_pos) {
				const PlayerInfo* new_player;
				if(portal->new_playable) {
					new_player = portal->new_playable->getPlayerInfo();
				} else {
					new_player = new_level->player();
				}
				if(new_player) {
					dest = point(new_player->getEntity().x(), new_player->getEntity().y());
				}
			}

			PlayerInfo* player = lvl_->player();
			if(portal->new_playable) {
				game_logic::MapFormulaCallablePtr callable(new game_logic::MapFormulaCallable());
				callable->add("new_playable", variant(portal->new_playable.get()));
				player->getEntity().handleEvent("player_change_on_teleport", callable.get());
			}

			if(player && portal->saved_game == false && !portal->level_dest_obj) {
				if(portal->new_playable) {
					player = portal->new_playable->getPlayerInfo();
					ASSERT_LOG(player != nullptr, "Object is not playable: " << portal->new_playable->getDebugDescription());
				}
				player->getEntity().setPos(dest);
				new_level->add_player(&player->getEntity());
				if(!player->getEntity().hasNoMoveToStanding() && !portal->no_move_to_standing){
					player->getEntity().moveToStanding(*new_level);
				}
				player->getEntity().handleEvent("enter_level");
			} else {
				player = new_level->player();
			}

			//if we're in a multiplayer level then going through a portal
			//will take us out of multiplayer.
			if(lvl_->players().size() != new_level->players().size() || portal->level_dest_obj) {
				lvl_ = new_level;
				done = true;
				throw multiplayer_exception();
			}

			lvl_ = new_level;
			last_draw_position() = screen_position();

			//garbage collect objects from the last level.
			runGarbageCollection();
			reapGarbageCollection();
			CustomObject::run_garbage_collection();

//			if(transition == "flip") {
//				transition_scene(*lvl_, last_draw_position(), false, flip_scene);
//			}
#ifndef NO_EDITOR
			if(editor_) {
				editor_ = editor::get_editor(lvl_->id().c_str());
				editor_->set_playing_level(lvl_);
				editor_->setup_for_editing();
				lvl_->setAsCurrentLevel();
				lvl_->set_editor();
				initHistorySlider();
			}
#endif

			//we always want to exit this function so that we don't
			//draw the new level when it hasn't had a chance to process.
			return !done;
		}
	}

	joystick::update();
	bool should_pause = false;

	static bool pause_next = false;
	if(pause_next) {
		paused = true;
		pause_next = false;
		show_pause_title();
		controls::read_until(lvl_->cycle());
	}

	static int joystick_pause_lockout = 0;
	if(joystick_pause_lockout) {
		--joystick_pause_lockout;
	}
	if(joystick::button(4) && !joystick_pause_lockout) {
		should_pause = true;
		joystick_pause_lockout = 10;
	}


	SDL_StartTextInput();
	if(MessageDialog::get() == nullptr) {

		bool have_motion_event = false;

		//Poll all the events. Sometimes we can have multiple mouse motion events
		//in one frame, but that isn't really useful, so coalesce multiple motion
		//events into a single event.
		std::vector<SDL_Event> events;
		SDL_Event ev;
		while(input::sdl_poll_event(&ev)) {
			if(ev.type == SDL_MOUSEMOTION) {
				if(have_motion_event) {
					for(auto it = events.begin(); it != events.end(); ++it) {
						if(it->type == SDL_MOUSEMOTION) {
							ev.motion.xrel += it->motion.xrel;
							ev.motion.yrel += it->motion.yrel;
							events.erase(it);
							break;
						}
					}
				}

				have_motion_event = true;
			}
			events.push_back(ev);
		}

		static int nevent_frame = 0;
		++nevent_frame;

		for(SDL_Event& event : events) {
			bool swallowed = false;

			//formula profiler and console doesn't use virtual co-ords so it gets the event before virtual processing
			swallowed = formula_profiler::handle_sdl_event(event, swallowed) || swallowed;
#ifndef NO_EDITOR
			if(console_) {
				swallowed = console_->processEvent(point(), event, swallowed);
			}
#endif

			mapSDLEventScreenCoordinatesToVirtual(event);

#ifndef NO_EDITOR

			if(history_slider_ && paused) {
				swallowed = history_slider_->processEvent(point(), event, swallowed) || swallowed;
				swallowed = history_button_->processEvent(point(), event, false) || swallowed;
			}

			if(editor_) {
				swallowed = editor_->handleEvent(event, swallowed) || swallowed;
				lvl_->setAsCurrentLevel();

				if(editor::last_edited_level() != lvl_->id() && editor_->confirm_quit()) {

					ffl::IntrusivePtr<Level> new_level = load_level(editor::last_edited_level());
					if(editor_) {
						new_level->set_editor();
					}

					new_level->setAsCurrentLevel();

					if(!new_level->music().empty()) {
						EntityPtr e;
						if(new_level->player()) {
							e = EntityPtr(&new_level->player()->getEntity());
						}
						play_music_track(e, new_level->music());
					}

					set_scene_title(new_level->title());
					lvl_ = new_level;

					lvl_->editor_clear_selection();
					editor_ = editor::get_editor(lvl_->id().c_str());
					editor_->set_playing_level(lvl_);
					editor_->setup_for_editing();
					lvl_->setAsCurrentLevel();
					lvl_->set_editor();
					initHistorySlider();

				}

				if(editor_->done()) {
					close_editor();
				}
			}

#endif

			swallowed = joystick::pump_events(event, swallowed);

			{
				const std::vector<EntityPtr> active_chars = lvl_->get_active_chars();

				if(event.type == SDL_TEXTINPUT) {
					const std::string text = event.text.text;

					game_logic::MapFormulaCallable* callable(new game_logic::MapFormulaCallable);
					variant holder(callable);
					callable->add("text", variant(text));

					for(const EntityPtr& e : active_chars) {
						e->handleEvent(OBJECT_EVENT_TEXT_INPUT, callable);
					}
				}

				for(const EntityPtr& e : active_chars) {
					CustomObject* custom_obj = dynamic_cast<CustomObject*>(e.get());
					swallowed = custom_obj->handle_sdl_event(event, swallowed);
				}
			}

			if(swallowed) {
				continue;
			}

			switch(event.type) {
			case SDL_QUIT: {
				stats::Entry("quit").addPlayerPos();
				quit_ = true;
				std::vector<EntityPtr> active_chars = lvl_->get_active_chars();
				for(const auto& c : active_chars) {
					c->handleEvent(OBJECT_EVENT_QUIT_GAME);
				}

				if(quit_) {
					done = true;
				}
				break;
			}
			case SDL_WINDOWEVENT:
				if((event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED 
					|| event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
					&& preferences::allow_autopause()) {
					if(event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
						if(!paused && !editor_) {
							toggle_pause();
						}
					} else {
						if(paused) {
							toggle_pause();
						}
					}
				} else if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
					LOG_INFO("SDL WINDOW EVENT MINIMIZED");
				} else if (event.window.event == SDL_WINDOWEVENT_MAXIMIZED) {
					LOG_INFO("SDL WINDOW EVENT MAXIMIZED");
				} else if (event.window.event == SDL_WINDOWEVENT_RESTORED) {
					LOG_INFO("SDL WINDOW EVENT RESTORED");
				} else if(event.window.event == SDL_WINDOWEVENT_RESIZED) {
					LOG_INFO("SDL WINDOW EVENT RESIZED to " << event.window.data1 << " x " << event.window.data2);
					//video_resize(event); 
					video_resize_event(event);
				}
			break;

			case SDL_KEYDOWN: {
				const SDL_Keymod mod = SDL_GetModState();
				const SDL_Keycode key = event.key.keysym.sym;
				if(key == SDLK_ESCAPE) {
					if(editor_) {
#ifndef NO_EDITOR
						close_editor();
#endif
					} else {
						should_pause = true;
					}
					break;
				} else if(key == SDLK_d && (mod&KMOD_CTRL)) {
#ifndef NO_EDITOR
					if(!console_ && lvl_->player()) {
						console_.reset(new debug_console::ConsoleDialog(*lvl_, lvl_->player()->getEntity()));
					} else {
						console_.reset();
					}
#endif

				} else if(key == SDLK_e && (mod&KMOD_CTRL)) {
#ifndef NO_EDITOR
					start_editor();
#endif
				} else if(key == SDLK_r && (mod&KMOD_CTRL) && editor_) {
#ifndef NO_EDITOR
					//We're in the editor and we want to refresh the level
					//to its original state. If alt is held, we also
					//reset the player.
					const bool reset_pos = mod & KMOD_ALT ? true : false;
					editor_->reset_playing_level(!reset_pos);

					if(reset_pos) {
						//make the camera jump to the player
						last_draw_position().init = false;
					}
#endif
				} else if(key == SDLK_s && (mod&KMOD_CTRL) && !editor_) {
					LOG_INFO("SAVING...");
					std::string data;
					
					variant lvl_node = lvl_->write();
					if(sound::current_music().empty() == false) {
						lvl_node = lvl_node.add_attr(variant("music"), variant(sound::current_music()));
					}
					sys::write_file(preferences::save_file_path(), lvl_node.write_json(true));
				} else if(key == SDLK_l && (mod&KMOD_CTRL) && !editor_) {
					LOG_INFO("LOADING...");

					std::string level_dest = preferences::save_file_path();

					if(sys::file_exists(level_dest)) {
						boost::replace_first(level_dest, preferences::user_data_path(), "");
						Level::portal p;
						p.level_dest = level_dest;
						p.dest_starting_pos = true;
						p.saved_game = true;
						p.transition = "instant";
						lvl_->force_enter_portal(p);
					}
				} else if(key == SDLK_s && (mod&KMOD_ALT)) {
					const std::string fname = KRE::WindowManager::getMainWindow()->saveFrameBuffer("screenshot.png");
					if(!fname.empty()) {
						std::shared_ptr<upload_screenshot_info> info(new upload_screenshot_info);
						background_task_pool::submit(
						  std::bind(upload_screenshot, fname, info),
						  std::bind(done_upload_screenshot, info));
					}
				} else if(key == SDLK_m && mod & KMOD_CTRL) {
					sound::mute(!sound::muted()); //toggle sound
				} else if((key == SDLK_p || (key == SDLK_n && paused)) && mod & KMOD_CTRL) {
					paused = !paused;
#ifndef NO_EDITOR
					initHistorySlider();
#endif
					show_pause_title();
					if(!paused) {
						controls::read_until(lvl_->cycle());
					}

					if(key == SDLK_n) {
						pause_next = true;
					}
					
				} else if(key == SDLK_f && mod & KMOD_CTRL && !preferences::no_fullscreen_ever()) {

					static int last_pushed = -1;

					if(last_pushed >= nevent_frame-1) {
						//If we switch between fullscreen and non-fullscreen we can get repeated
						//key press events resulting in us switching back and forth, so ignore
						//ctrl+f key events in successive frames.
						continue;
					}

					auto& gs = graphics::GameScreen::get();
					const int virtual_width = gs.getVirtualWidth();
					const int virtual_height = gs.getVirtualHeight();

					last_pushed = nevent_frame;
					LOG_DEBUG("ctrl-f pushed");
					// XXX this changes if editor is active.
					if(wnd->fullscreenMode() == KRE::FullScreenMode::WINDOWED) {

						LOG_DEBUG("Enter full-screen mode");
						wnd->setFullscreenMode(KRE::FullScreenMode::FULLSCREEN_WINDOWED);

						if(preferences::auto_size_window() || g_desktop_fullscreen) {
							SDL_DisplayMode dm;
							if(SDL_GetDesktopDisplayMode(0, &dm) == 0) {
								preferences::adjust_virtual_width_to_match_physical(dm.w, dm.h);
								wnd->setWindowSize(dm.w, dm.h);
								gs.setDimensions(dm.w, dm.h);
								gs.setVirtualDimensions(preferences::requested_virtual_window_width(), preferences::requested_virtual_window_height());
							}

						}

					} else {
						LOG_DEBUG("Enter windowed mode");
						wnd->setFullscreenMode(KRE::FullScreenMode::WINDOWED);

						if(preferences::auto_size_window() || g_desktop_fullscreen) {
							int width = 0, height = 0;

							if(preferences::requested_window_width() > 0 && preferences::requested_window_height() > 0) {
								width = preferences::requested_window_width();
								height = preferences::requested_window_height();
							} else {
								auto_select_resolution(wnd, &width, &height, true);
							}

							preferences::adjust_virtual_width_to_match_physical(width, height);

							wnd->setWindowSize(width, height);
							gs.setDimensions(width, height);
							gs.setVirtualDimensions(preferences::requested_virtual_window_width(), preferences::requested_virtual_window_height());
						}
					}
				} else if(key == SDLK_F7) {
					if(formula_profiler::Manager::get()) {
						if(formula_profiler::Manager::get()->is_profiling()) {
							formula_profiler::Manager::get()->halt();
						} else {
							formula_profiler::Manager::get()->init("profile.dat");
						}
					}
				} else if(key == SDLK_F8) {
					if(formula_profiler::Manager::get()) {
						if(formula_profiler::Manager::get()->is_profiling()) {
							formula_profiler::Manager::get()->halt();
						} else {
							formula_profiler::Manager::get()->init("profile.dat", true);
						}
					}
				} else if(key == SDLK_F6) {
					g_particle_editor = !g_particle_editor;
				} else if(key == SDLK_F3) {
					LOG_DEBUG("F3 pressed");
					preferences::set_show_fps(!preferences::show_fps());
				} else if(key == SDLK_F9) {
					g_theme_imgui_ui = !g_theme_imgui_ui;
					if(mod & KMOD_CTRL) {
						// resets to default ui and shows it
						theme_imgui_default();
						g_theme_imgui_ui = true;
					}
				}
				break;
			}
			case SDL_MOUSEWHEEL:
				handle_mouse_events(event);
				break;

#ifndef NO_EDITOR
			case SDL_MOUSEBUTTONDOWN: {
				const SDL_Keymod mod = SDL_GetModState();
				const bool ctrl_pressed = (mod&KMOD_CTRL) != 0;
				if(console_.get() && (g_allow_debug_console_clicking || ctrl_pressed)) {
					int mousex, mousey;
					input::sdl_get_mouse_state(&mousex, &mousey);
					EntityPtr selected = lvl_->get_next_character_at_point(last_draw_position().x/100 + mousex, last_draw_position().y/100 + mousey, last_draw_position().x/100, last_draw_position().y/100, console_->getFocus().get());
					if(selected) {
						lvl_->set_editor_highlight(selected);
						console_->setFocus(selected);
					}
				} else {
					handle_mouse_events(event);
				}
				break;
			}

			case SDL_MOUSEMOTION:
				handle_mouse_events(event);
				break;

			case SDL_MOUSEBUTTONUP:
				handle_mouse_events(event);
				break;
#endif // NO_EDITOR
			default:
				break;
			}
		}

		if(should_pause) {
			lvl_->set_show_builtin_settingsDialog(true);
			std::vector<EntityPtr> active_chars = lvl_->get_active_chars();
			for(const auto& c : active_chars) {
				c->handleEvent(OBJECT_EVENT_SETTINGS_MENU);
			}
		}
		
		if(lvl_->show_builtin_settingsDialog() && g_allow_builtin_settings_dialog)
		{
			lvl_->set_show_builtin_settingsDialog(false);

			if(mouselook_state) {
				SDL_SetRelativeMouseMode(SDL_FALSE);
			}
			settingsDialog.reset();
			const PAUSE_GAME_RESULT result = show_pause_game_dialog();

			handle_pause_game_result(result);

			if(done) {
				mouselook_state = false;
			}
			if(mouselook_state) {
				SDL_SetRelativeMouseMode(SDL_TRUE);
				SDL_GetRelativeMouseState(nullptr, nullptr);
			}
		}
	}

	if(MessageDialog::get()) {
		MessageDialog::get()->process();
		pause_time_ += preferences::frame_time_millis();
	} else {
		if (!paused && g_pause_stack == 0) {
			const int start_process = profile::get_tick_time();

			try {
				debug_console::process_graph();
				lvl_->process();
			} catch(InterruptGameException& e) {
				handle_pause_game_result(e.result);
			}

			const int process_time = profile::get_tick_time() - start_process;
			next_process_ += process_time;
			current_perf.process = process_time;
		} else {
			pause_time_ += preferences::frame_time_millis();
		}
	}

	if(lvl_->end_game()) {
		transition_scene(*lvl_, last_draw_position(), false, "fade");
		show_end_game();
		done = true;
		return true;
	}

	const bool skip_odd_frame = g_skip_odd_frames && nskip_draw_ == 0;

	const int start_draw = profile::get_tick_time();
	if(!skip_odd_frame && (start_draw < desired_end_time || nskip_draw_ >= g_max_frame_skips)) {
		formula_profiler::Instrument instrument("DRAW");
		bool should_draw = true;

#ifndef NO_EDITOR		
		if(editor_) {
			editor_->handle_tracking_to_mouse(); //This call should be moved to editor.cpp, but it freezes the display there. (Even here, framerate is terrible.)
		}
#endif
		
		if(editor_ && paused) {
#ifndef NO_EDITOR
			const int xpos = editor_->xpos();
			const int ypos = editor_->ypos();
			editor_->handle_scrolling();
			last_draw_position().x += (editor_->xpos() - xpos)*100;
			last_draw_position().y += (editor_->ypos() - ypos)*100;

			float target_zoom = 1.0f/editor_->zoom();
			float diff = target_zoom - last_draw_position().zoom;
			float amount = diff/10.0f;
			float dir = amount > 0.0f ? 1.0f : -1.0f;
			if(amount*dir < 0.02f) {
				amount = 0.02f*dir;
			}

			if(amount*dir > diff*dir) {
				amount = diff;
			}
			last_draw_position().zoom += amount;
#endif
		} else {
			should_draw = update_camera_position(*lvl_, last_draw_position(), nullptr, !is_skipping_game());
		}

// XXX Needs rework
#if 0
		bool editor_mouselook = false;
#ifndef NO_EDITOR
		if(editor_) {
			editor_mouselook = editor_->mouselook_mode();
		}
#endif
		if(mouselook_state || editor_mouselook) {
			const camera_callable_ptr& cam = lvl_->camera();
			if(cam) {
				int rmx, rmy;
				SDL_GetRelativeMouseState(&rmx, &rmy);
				if(lvl_->is_mouselook_inverted()) {
					rmy = -rmy;
				}
				cam->set_hangle(cam->hangle() + cam->mousespeed() * rmx);
				cam->set_vangle(cam->vangle() + cam->mousespeed() * rmy);

				cam->compute_view();
			}
		}
#endif

		{
		formula_profiler::Instrument instrument("PROC_DRAW");
		lvl_->process_draw();
		}

		if(should_draw) {
			wnd->setClearColor(KRE::Color(0, 0, 0, 0));
			wnd->clear(KRE::ClearFlags::ALL);
#ifndef NO_EDITOR
			const Uint8 *key = SDL_GetKeyboardState(nullptr);
			if(editor_ && key[SDL_SCANCODE_L] 
				&& !editor_->hasKeyboardFocus() 
				&& (!console_ || !console_->hasKeyboardFocus())) {
#endif
				editor_->toggle_active_level();
				render_scene(editor_->get_level(), last_draw_position());
				editor_->toggle_active_level();
				lvl_->setAsCurrentLevel();
			} else {
				std::vector<variant> alpha_values;
				if(!history_trails_.empty()) {
					for(EntityPtr e : history_trails_) {
						alpha_values.push_back(e->queryValue("alpha"));
						e->mutateValue("alpha", variant(32));
						lvl_->add_draw_character(e);
					}
				}
				{
					//profile::manager pman("render_scene");
					//KRE::ModelManager2D(0, 0, 0.0f, static_cast<float>(g_global_scale));
					render_scene(*lvl_, last_draw_position());
				}
#ifndef NO_EDITOR
				int index = 0;
				if(!history_trails_.empty()) {
					for(EntityPtr e : history_trails_) {
						e->mutateValue("alpha", alpha_values[index++]);
					}

					lvl_->set_active_chars();
				}
			}

			if(editor_) {
				editor_->draw_gui();
			}

			if(history_slider_ && paused) {
				history_slider_->draw();
				history_button_->draw();
			}

			if(console_) {
				console_->draw();
			}

			if(g_theme_imgui_ui) {
				imgui_theme_ui();
			}

			formula_profiler::Instrument instrument_profiler("DRAW_PROFILE");
			formula_profiler::draw();
#endif
		}

#if defined(USE_BOX2D)
	box2d::world_ptr world = box2d::world::our_world_ptr();
	if(world) {
		if(world->draw_debug_data()) {
			world->getCurrentPtr()->DrawDebugData();
		}
	}
#endif

		performance_data perf(current_max_,current_fps_, current_cycles_, current_delay_, current_draw_, current_process_, current_flip_, cycle, current_events_, profiling_summary_);
	
		if(!is_skipping_game() && preferences::show_fps()) {
			draw_fps(*lvl_, perf);
		}

		const int draw_time = profile::get_tick_time() - start_draw;
		next_draw_ += draw_time;
		current_perf.draw = draw_time;

		const int start_flip = profile::get_tick_time();
		if(!is_skipping_game()) {
			KRE::WindowManager::getMainWindow()->swap();
		}

		const int flip_time = profile::get_tick_time() - start_flip;
		next_flip_ += flip_time;
		current_perf.flip = flip_time;
		++next_fps_;
		nskip_draw_ = 0;
	} else {
		++nskip_draw_;
	}

	++next_cycles_;
	current_perf.cycle = next_cycles_;

	static int prev_events_per_second = 0;
	current_perf.nevents = CustomObject::events_handled_per_second - prev_events_per_second;
	prev_events_per_second = CustomObject::events_handled_per_second;

	const time_t this_second = time(nullptr);
	if(this_second != current_second_) {
		current_second_ = this_second;
		current_max_ = next_max_;
		current_fps_ = next_fps_;
		current_cycles_ = next_cycles_;
		current_delay_ = next_delay_;
		current_draw_ = next_draw_;
		current_flip_ = next_flip_;
		current_process_ = next_process_;
		current_events_ = CustomObject::events_handled_per_second;
		next_max_ = 0;
		next_fps_ = 0;
		next_cycles_ = 0;
		next_delay_ = 0;
		next_draw_ = 0;
		next_process_ = 0;
		next_flip_ = 0;
		prev_events_per_second = CustomObject::events_handled_per_second = 0;

		profiling_summary_ = formula_profiler::get_profile_summary();
	}

	const int raw_wait_time = desired_end_time - profile::get_tick_time();
	int wait_time = std::max<int>(1, desired_end_time - profile::get_tick_time());

	static int async_work_items_starvation = 0;
	if(asynchronous_work_items_.empty() == false) {
		++async_work_items_starvation;
	}

	while(!asynchronous_work_items_.empty() && (wait_time >= g_time_quota_async_work_items || async_work_items_starvation > 60)) {
		async_work_items_starvation = 0;
		std::function<void()> fn = asynchronous_work_items_.front();
		asynchronous_work_items_.pop_front();
		fn();

		wait_time = std::max<int>(1, desired_end_time - profile::get_tick_time());
	}

	next_delay_ += wait_time;
	current_perf.delay = wait_time;

	if (wait_time != 1 && !is_skipping_game()) {
		formula_profiler::Instrument instrument("SLEEP");
		profile::delay(wait_time);
	}

	performance_data::set_current(current_perf);
	
	if(is_skipping_game()) {
		const int adjust_time = desired_end_time - profile::get_tick_time();
		if(adjust_time > 0) {
			pause_time_ -= adjust_time;
		}
	}

	if (!paused && g_pause_stack == 0) ++cycle;

	const int end_cycle_time = profile::get_tick_time();
	if(end_cycle_time - start_cycle_time > next_max_) {
		next_max_ = end_cycle_time - start_cycle_time;
	}

	return !quit_;
}

void LevelRunner::toggle_pause()
{
	paused = !paused;
#ifndef NO_EDITOR
	initHistorySlider();
#endif
	show_pause_title();
	if(!paused) {
		controls::read_until(lvl_->cycle());
	}
}

void LevelRunner::reverse_cycle()
{
	const int begin_time = profile::get_tick_time();
	lvl_->reverse_one_cycle();
	lvl_->set_active_chars();
	lvl_->process_draw();

	//remove the control history
	controls::unread_local_controls();

	SDL_Event event;
	while(input::sdl_poll_event(&event)) {
	}

	const bool should_draw = update_camera_position(*lvl_, last_draw_position(), nullptr, !is_skipping_game());
	render_scene(*lvl_, last_draw_position());
	KRE::WindowManager::getMainWindow()->swap();

	const int wait_time = begin_time + 20 - profile::get_tick_time();
	if(wait_time > 0) {
		profile::delay(wait_time);
	}
}

namespace {
//variable to mark if we are already pausing. If so, don't pause again
//based on a new pause scope.
bool pause_scope_active = false;
}

pause_scope::pause_scope() : ticks_(profile::get_tick_time()), active_(!pause_scope_active)
{
	pause_scope_active = true;
}

pause_scope::~pause_scope()
{
	if(active_) {
		const int t = profile::get_tick_time() - ticks_;
		global_pause_time += t;
		pause_scope_active = false;
	}
}

void LevelRunner::quit_game()
{
	//record a quit event in stats
	if(lvl_->player()) {
		lvl_->player()->getEntity().recordStatsMovement();
		stats::Entry("quit").addPlayerPos();
	}
	
	done = true;
	quit_ = true;
}

void LevelRunner::handle_pause_game_result(PAUSE_GAME_RESULT result)
{
	if(result == PAUSE_GAME_RESULT::QUIT) {
		quit_game();

	} else if(result == PAUSE_GAME_RESULT::GO_TO_TITLESCREEN) {
		done = true;
		original_level_cfg_ = "titlescreen.cfg";
	} else if(result == PAUSE_GAME_RESULT::GO_TO_LOBBY) {
		done = true;
		lvl_->launch_new_module("lobby");
	}
}

extern bool g_editor_history;

#ifndef NO_EDITOR
void LevelRunner::initHistorySlider()
{
	if(paused && editor_ && g_editor_history) {
		using std::placeholders::_1;
		history_slider_.reset(new gui::Slider(110, std::bind(&LevelRunner::onHistoryChange, this, _1)));
		history_slider_->setLoc(370, 4);
		history_slider_->setPosition(1.0);
		history_button_.reset(new gui::Button("Trails", std::bind(&LevelRunner::toggle_history_trails, this)));
		history_button_->setLoc(history_slider_->x() + history_slider_->width(), history_slider_->y());
	} else {
		history_slider_.reset();
		history_button_.reset();
		history_trails_.clear();
	}
}

void LevelRunner::onHistoryChange(float value)
{
	const int first_frame = lvl_->earliest_backup_cycle();
	const int last_frame = controls::local_controls_end();
	int targetFrame = first_frame + static_cast<int>((last_frame + 1 - first_frame)*value);
	if(targetFrame > last_frame) {
		targetFrame = last_frame;
	}

	LOG_INFO("TARGET FRAME: " << targetFrame << " IN [" << first_frame << ", " << last_frame << "]");

	if(targetFrame < lvl_->cycle()) {
		lvl_->reverse_to_cycle(targetFrame);
	} else if(targetFrame > lvl_->cycle()) {
		LOG_INFO("STEPPING FORWARD FROM " << lvl_->cycle() << " TO " << targetFrame << " /" << controls::local_controls_end());

		const controls::control_backup_scope ctrl_scope;
		
		while(lvl_->cycle() < targetFrame) {
			lvl_->process();
			lvl_->process_draw();
			lvl_->backup();
		}
	}

	lvl_->set_active_chars();
}

void LevelRunner::toggle_history_trails()
{
	if(history_trails_.empty() && lvl_->player()) {
		update_history_trails();
	} else {
		history_trails_.clear();
		history_trails_label_.clear();
	}
}

void LevelRunner::update_history_trails()
{
	EntityPtr e;
	if(history_trails_label_.empty() == false && lvl_->get_entity_by_label(history_trails_label_)) {
		e = lvl_->get_entity_by_label(history_trails_label_);
	} else if(lvl_->editor_selection().empty() == false) {
		e = lvl_->editor_selection().front();
	} else if(lvl_->player()) {
		e = EntityPtr(&lvl_->player()->getEntity());
	}

	if(e) {
		const int first_frame = lvl_->earliest_backup_cycle();
		const int last_frame = controls::local_controls_end();

		const int ncycles = (last_frame - first_frame) + 1;
		history_trails_ = lvl_->predict_future(e, ncycles);
		history_trails_state_id_ = editor_->level_state_id();
		object_reloads_state_id_ = CustomObjectType::numObjectReloads();
		tile_rebuild_state_id_ = Level::tileRebuildStateId();

		history_trails_label_ = e->label();
	}
}

void LevelRunner::replay_level_from_start()
{
	std::unique_ptr<controls::control_backup_scope> backup_ctrl_ptr(new controls::control_backup_scope);
	ffl::IntrusivePtr<Level> new_level = load_level(lvl_->id());
	if(editor_) {
		new_level->set_editor();
	}

	new_level->setAsCurrentLevel();

	if(!new_level->music().empty()) {
		EntityPtr e;
		if(new_level->player()) {
			e = EntityPtr(&new_level->player()->getEntity());
		}
		play_music_track(e, new_level->music());
	}

	lvl_ = new_level;

	lvl_->editor_clear_selection();
	editor_ = editor::get_editor(lvl_->id().c_str());
	editor_->set_playing_level(lvl_);
	editor_->setup_for_editing();
	lvl_->setAsCurrentLevel();
	lvl_->set_editor();
	initHistorySlider();

	backup_ctrl_ptr.reset();

	const int last_frame = controls::local_controls_end();

	if(last_frame > lvl_->cycle()) {
		const controls::control_backup_scope ctrl_scope;
		
		while(lvl_->cycle() < last_frame) {
			lvl_->process();
			lvl_->process_draw();
			lvl_->backup();
		}
	}

	lvl_->set_active_chars();
}

#endif
