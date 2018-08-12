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

#include <algorithm>
#include <cmath>
#include <iostream>

#include <boost/lexical_cast.hpp>

#include "Canvas.hpp"
#include "Font.hpp"
#include "ModelMatrixScope.hpp"
#include "RenderTarget.hpp"
#include "WindowManager.hpp"

#include "asserts.hpp"
#include "controls.hpp"
#include "debug_console.hpp"
#include "draw_scene.hpp"
#include "editor.hpp"
#include "formula_profiler.hpp"
#include "globals.h"
#include "graphical_font.hpp"
#include "gui_section.hpp"
#include "i18n.hpp"
#include "level.hpp"
#include "message_dialog.hpp"
#include "module.hpp"
#include "player_info.hpp"
#include "preferences.hpp"
#include "screen_handling.hpp"
#include "speech_dialog.hpp"

#include "tooltip.hpp"

namespace 
{
	int g_flash_disable = 0;

	std::vector<rect> current_debug_rects;
	int current_debug_rects_valid_cycle = -1;

	std::string& scene_title() 
	{
		static std::string title;
		return title;
	}

	AchievementPtr current_achievement;
	int current_achievement_duration = 0;

	struct screen_flash 
	{
		KRE::ColorTransform color, delta;
		int duration;
	};

	std::vector<screen_flash>& flashes() 
	{
		static std::vector<screen_flash> obj;
		return obj;
	}

	int scene_title_duration_;

	screen_position last_position;
}

bool isAchievementDisplayed() 
{
	return current_achievement && current_achievement_duration > 0;
}

screen_position& last_draw_position()
{
	return last_position;
}

disable_flashes_scope::disable_flashes_scope()
{
	++g_flash_disable;
}

disable_flashes_scope::~disable_flashes_scope()
{
	--g_flash_disable;
}

void screen_color_flash(const KRE::ColorTransform& color, const KRE::ColorTransform& color_delta, int duration)
{
	if(!g_flash_disable) {
		screen_flash f = { color, color_delta, duration };
		flashes().push_back(f);
	}
}

void draw_last_scene() 
{
	draw_scene(Level::current(), last_draw_position());
}

void set_scene_title(const std::string& msg, int duration) 
{
	//explicitly translate all level titles
	scene_title() = (msg.size() > 0) ? _(msg) : msg;
	scene_title_duration_ = duration;
}

void set_displayed_Achievement(AchievementPtr a)
{
	current_achievement = a;
	current_achievement_duration = 250;
}

void draw_scene(const Level& lvl, screen_position& pos, const Entity* focus, bool doDraw) 
{
	const bool draw_ready = update_camera_position(lvl, pos, focus, doDraw);
	if(draw_ready) {
		render_scene(const_cast<Level&>(lvl), pos);
	}
}

bool update_camera_position(const Level& lvl, screen_position& pos, const Entity* focus, bool doDraw) 
{
	if(focus == nullptr && lvl.player()) {
		focus = &lvl.player()->getEntity();
	}

	//flag which gets set to false if we abort drawing, due to the
	//screen position being initialized now.
	const bool draw_level = doDraw && pos.init;
	
	const int screen_width = graphics::GameScreen::get().getVirtualWidth();
	const int screen_height = graphics::GameScreen::get().getVirtualHeight();

	ASSERT_LOG(focus || lvl.in_editor(), "No player found in level. Must have a player object. (An object with is_human: true marked");

	if(focus) {
		const float target_zoom = lvl.zoom_level();
		const float zoom_speed = 0.03f;
		if(std::abs(target_zoom - pos.zoom) < zoom_speed) {
			pos.zoom = target_zoom;
		} else if(pos.zoom > target_zoom) {
			pos.zoom -= zoom_speed;
		} else {
			pos.zoom += zoom_speed;
		}
        
        //if we've set the zoom inside the very first cycle of a level (i.e. using on_start_level), then we're doing some kind of cutscene which has the camera start zoomed out.  We want the camera to immediately start in this state, not "progress to this state gradually from the normal zoom". 
        if(lvl.instant_zoom_level_set() || lvl.cycle() == 1){
            pos.zoom = target_zoom;
        }

		// If the camera is automatically moved along by the level (e.g. a 
		// hurtling through the sky level) do that here.
		pos.x_pos += lvl.auto_move_camera_x()*100;
		pos.y_pos += lvl.auto_move_camera_y()*100;

		//find how much padding will have to be on the edge of the screen due
		//to the level being wider than the screen. This value will be 0
		//if the level is larger than the screen (i.e. most cases)
		const int x_screen_pad = lvl.constrain_camera() ? std::max<int>(0, screen_width - lvl.boundaries().w()) : 0;

		const int y_screen_pad = lvl.constrain_camera() ? std::max<int>(0, screen_height - lvl.boundaries().h()) : 0;
		pos.x_border = x_screen_pad / 2;
		pos.y_border = y_screen_pad / 2;

		//find the boundary values for the camera position based on the size
		//of the level. These boundaries keep the camera from ever going out
		//of the bounds of the level.
		
		const float inverse_zoom_level = 1.0f / pos.zoom;

		//we look a certain number of frames ahead -- assuming the focus
		//keeps moving at the current velocity, we converge toward the point
		//they will be at in x frames.
		const int predictive_frames_horz = 20;
		const int predictive_frames_vert = 5;

		int displacement_x = 0, displacement_y = 0;
		if(pos.focus_x || pos.focus_y) {
			displacement_x = focus->getFeetX() - pos.focus_x;
			displacement_y = focus->getFeetY() - pos.focus_y;
		}

		pos.focus_x = focus->getFeetX();
		pos.focus_y = focus->getFeetY();

		//find the point we want the camera to converge toward. It will be the
		//feet of the player, but inside the boundaries we calculated above.
		int x = focus->getFeetX() + displacement_x * predictive_frames_horz;

		//calculate the adjustment to the camera's target position based on
		//our vertical look. This is calculated as the square root of the
		//vertical look, to make the movement slowly converge.
		const int vertical_look = focus->verticalLook();

		//find the y point for the camera to converge toward
		int y = focus->getFeetY() - static_cast<int>(screen_height / (5.0f * target_zoom)) + displacement_y * predictive_frames_vert + vertical_look;

		if(lvl.focus_override().empty() == false) {
			std::vector<EntityPtr> v = lvl.focus_override();
			int left = 0, right = 0, top = 0, bottom = 0;
			while(v.empty() == false) {
				left = v[0]->getFeetX();
				right = v[0]->getFeetX();
				top = v[0]->getFeetY();
				bottom = v[0]->getFeetY();
				for(const EntityPtr& e : v) {
					left = std::min<int>(e->getFeetX(), left);
					right = std::max<int>(e->getFeetX(), right);
					top = std::min<int>(e->getFeetY(), top);
					bottom = std::min<int>(e->getFeetY(), bottom);
				}

				const int border_size = 20;
				if(v.size() == 1 
					|| (right - left < static_cast<int>(screen_width / target_zoom) - border_size 
					&& bottom - top < static_cast<int>(screen_height / target_zoom) - border_size)) {
					break;
				}

				break;

				v.pop_back();
			}

			x = (left + right)/2;
			y = (top + bottom)/2 - static_cast<int>(screen_height / (5.0f * target_zoom));
		}

		pos.target_xpos = 100*(x - screen_width/2);
		pos.target_ypos = 100*(y - screen_height/2);

		if(lvl.lock_screen()) {
			x = lvl.lock_screen()->x;
			y = lvl.lock_screen()->y;
		}

		//for small screens the speech dialog arrows cover the entities they are
		//pointing to. adjust to that by looking up a little bit.
		if (lvl.current_speech_dialog() && graphics::GameScreen::get().getVirtualHeight() < 600)
			y += (600 - screen_height) / static_cast<int>(2.0f * target_zoom);

		//find the target x,y position of the camera in centi-pixels. Note that
		//(x,y) represents the position the camera should center on, while
		//now we're calculating the top-left point.
		//
		//the actual camera position will converge toward this point
		const int target_xpos = 100 * (x - screen_width/2);
		const int target_ypos = 100 * (y - screen_height/2);

		if(pos.init == false) {
			pos.x_pos = target_xpos;
			pos.y_pos = target_ypos;
			pos.init = true;
		} else {

			//Make (pos.x_pos, pos.y_pos) converge toward (target_xpos,target_ypos).
			//We do this by moving asymptotically toward the target, which
			//makes the camera have a nice acceleration/decceleration effect
			//as the target position moves.
			const int horizontal_move_speed = static_cast<int>(30.0f / target_zoom);
			const int vertical_move_speed = static_cast<int>(10.0f / target_zoom);
			int xdiff = (target_xpos - pos.x_pos) / horizontal_move_speed;
			int ydiff = (target_ypos - pos.y_pos) / vertical_move_speed;

			pos.x_pos += xdiff;
			pos.y_pos += ydiff;
		}
		
		
		//shake decay is handled automatically; just by giving it an offset and velocity,
		//it will automatically return to equilibrium
		
		//shake speed		
		pos.x_pos += (pos.shake_x_offset);
		pos.y_pos += (pos.shake_y_offset);
		
		//shake velocity
		pos.shake_x_offset += pos.shake_x_vel;
		pos.shake_y_offset += pos.shake_y_vel;
			
		//shake acceleration
		if((std::abs(pos.shake_x_vel) < 50) && (std::abs(pos.shake_x_offset) < 50)){
			//prematurely end the oscillation if it's in the asymptote
			pos.shake_x_offset = 0;
			pos.shake_x_vel = 0;
		} else {
			//extraneous signs kept for consistency with conventional spring physics, also
			//the value that "offset" is divided by, is (the inverse of) 'k', aka "spring stiffness"
			//the value that "velocity" is divided by, is (the inverse of) 'b', aka "oscillation damping",
			//which causes the spring to come to rest.
			//These values are very sensitive, and tweaking them wrongly will cause the spring to 'explode',
			//and increase its motion out of game-bounds. 
			if(pos.shake_x_offset > 0) {
				pos.shake_x_vel -= 1 * pos.shake_x_offset/3 + pos.shake_x_vel/15;
			} else if(pos.shake_x_offset < 0) {
				pos.shake_x_vel += -1 * pos.shake_x_offset/3 - pos.shake_x_vel/15;
			}
		}

		if((std::abs(pos.shake_y_vel) < 50) && (std::abs(pos.shake_y_offset) < 50)) {
			//prematurely end the oscillation if it's in the asymptote
			pos.shake_y_offset = 0;
			pos.shake_y_vel = 0;
		} else {
			if(pos.shake_y_offset > 0) {
				pos.shake_y_vel -= (1 * pos.shake_y_offset/3 + pos.shake_y_vel/15);
			} else if(pos.shake_y_offset < 0) {
				pos.shake_y_vel += (-1 * pos.shake_y_offset/3 - pos.shake_y_vel/15);
			}
		}

		const int minmax_x_adjust = static_cast<int>(screen_width*(1.0f - inverse_zoom_level)*0.5f);
		const int minmax_y_adjust = static_cast<int>(screen_height*(1.0f - inverse_zoom_level)*0.5f);
	
		int min_x = (lvl.boundaries().x() - minmax_x_adjust) * 100;
		int min_y = (lvl.boundaries().y() - minmax_y_adjust) * 100;
		int max_x = (lvl.boundaries().x2() - minmax_x_adjust - static_cast<int>(screen_width * inverse_zoom_level)) * 100;
		int max_y = (lvl.boundaries().y2() - minmax_y_adjust - static_cast<int>(screen_height * inverse_zoom_level)) * 100;

		if(min_x > max_x) {
			min_x = max_x = (min_x + max_x)/2;
		}

		if(min_y > max_y) {
			min_y = max_y = (min_y + max_y)/2;
		}

		if (lvl.constrain_camera()) {
			pos.x = std::min(std::max(pos.x_pos, min_x), max_x);
			pos.y = std::min(std::max(pos.y_pos, min_y), max_y);
		} else {
			pos.x = pos.x_pos;
			pos.y = pos.y_pos;
		}
	}

	last_position = pos;

	return draw_level;
}

void render_scene(Level& lvl, const screen_position& pos) 
{
	auto& gs = graphics::GameScreen::get();

	const int screen_width = gs.getVirtualWidth();
	const int screen_height = gs.getVirtualHeight();

	const bool need_rt = gs.getVirtualWidth() != gs.getWidth() || gs.getVirtualHeight() != gs.getHeight();

	static KRE::RenderTargetPtr rt;
	
	if(need_rt && (!rt || rt->width() != screen_width || rt->height() != screen_height)) {
		rt = (KRE::RenderTarget::create(screen_width, screen_height, 1, false, false));
		rt->setBlendState(false);
	}

	if(rt && need_rt) {
		rt->renderToThis(rect(0,0,screen_width,screen_height));
		rt->setClearColor(KRE::Color(0,0,0,255));
		rt->clear();

	}

	auto wnd = KRE::WindowManager::getMainWindow();
	auto canvas = KRE::Canvas::getInstance();

	graphics::GameScreen::Manager screen_manager(wnd);
	KRE::ModelManager2D model(gs.x(), gs.y(), 0, 1.0f); //glm::vec2(1.0f/gs.getScaleW(), 1.0f/gs.getScaleH()));

	KRE::Canvas::CameraScope cam_scope(gs.getCurrentCamera());
	KRE::Canvas::DimScope dim_scope(screen_width, screen_height);

	const int camera_rotation = lvl.camera_rotation();
	if(camera_rotation) {
		//lvl.setRotation(rotate, glm::vec3(0.0f, 0.0f, 1.0f));
		// XXX fixme
	}

	if(pos.flip_rotate) {
		/*wnd->setClearColor(KRE::Color(0.0f, 0.0f, 0.0f, 0.0f));
		wnd->clear(KRE::ClearFlags::COLOR);

		const double angle = sin(0.5f*static_cast<float>(M_PI*pos.flip_rotate)/1000.0f);
		const int pixels = static_cast<int>((graphics::GameScreen::get().getVirtualWidth()/2)*angle);
		
		//then squish all future drawing inwards
		wnd->setViewPort(pixels, 0, graphics::GameScreen::get().getVirtualWidth() - pixels*2, graphics::GameScreen::get().getVirtualHeight());
		*/
		ASSERT_LOG(false, "Fix pos.flip_rotate");
	}

	int xscroll = (pos.x / 100) & preferences::xypos_draw_mask;
	int yscroll = (pos.y / 100) & preferences::xypos_draw_mask;

	int bg_xscroll = xscroll;
	int bg_yscroll = yscroll;

	xscroll += static_cast<int>((screen_width / 2) * (1.0f - 1.0f / pos.zoom));
	yscroll += static_cast<int>((screen_height / 2) * (1.0f - 1.0f / pos.zoom));


	float xdelta = 0.0f, ydelta = 0.0f;
	if(pos.zoom < 1.0f) {
		xdelta = bg_xscroll - xscroll;
		ydelta = bg_yscroll - yscroll;
	}

	{
		KRE::ModelManager2D model_matrix(-xscroll, -yscroll, 0, pos.zoom);

		lvl.draw_background(bg_xscroll, bg_yscroll, camera_rotation, xdelta, ydelta);

		int draw_width = screen_width;
		int draw_height = screen_height;
		if(pos.zoom < 1.0f) {
			draw_width = static_cast<int>(draw_width / pos.zoom);
			draw_height = static_cast<int>(draw_height / pos.zoom);
		}
		lvl.draw(xscroll, yscroll, draw_width, draw_height);

		for(const rect& r : current_debug_rects) {
			canvas->drawSolidRect(r, KRE::Color(0, 0, 255, 175));
		}

		if(current_debug_rects_valid_cycle != lvl.cycle()) {
			current_debug_rects.clear();
		}

		current_debug_rects_valid_cycle = lvl.cycle();

		lvl.drawLater(xscroll, yscroll, draw_width, draw_height);
	}

	for(std::vector<screen_flash>::iterator i = flashes().begin();
	    i != flashes().end(); ) {
		const KRE::Color& tint = i->color.toColor();
		if(tint.a() > 0) {
			auto canvas = KRE::Canvas::getInstance();
			canvas->drawSolidRect(rect(0, 0, screen_width, screen_height), tint);
		}

		i->color = i->color + i->delta;

		if(--i->duration <= 0) {
			i = flashes().erase(i);
		} else {
			++i;
		}
	}
	
	//draw borders around the screen if the screen is bigger than the level.
	if(pos.x_border > 0) {
		canvas->drawSolidRect(rect(0, 0, pos.x_border, screen_height), KRE::Color::colorBlack());
		canvas->drawSolidRect(rect(screen_width - pos.x_border, 0, pos.x_border, screen_width), KRE::Color::colorBlack());
	}

	if(pos.y_border > 0) {
		canvas->drawSolidRect(rect(pos.x_border, 0, screen_width - pos.x_border*2, pos.y_border), KRE::Color::colorBlack());
		canvas->drawSolidRect(rect(pos.x_border, screen_height - pos.y_border, screen_width - pos.x_border*2, pos.y_border), KRE::Color::colorBlack());
	}


#ifndef NO_EDITOR
	debug_console::draw();
#endif
	gui::draw_tooltip();

	debug_console::draw_graph();

	if (!g_pause_stack) {
		lvl.draw_status();
	}

	if(scene_title_duration_ > 0 && scene_title().empty() == false) {
		--scene_title_duration_;
		const ConstGraphicalFontPtr f = GraphicalFont::get("default");
		ASSERT_LOG(f.get() != nullptr, "COULD NOT LOAD DEFAULT FONT");
		const rect r = f->dimensions(scene_title());
		const float alpha = scene_title_duration_ > 10 ? 1.0f : scene_title_duration_/10.0f;
		f->draw(screen_width/2 - r.w()/2 + 2, screen_height/2 - r.h()/2 + 2, scene_title(), 2, KRE::Color(0.f, 0.f, 0.f, 0.5f*alpha));
		f->draw(screen_width/2 - r.w()/2, screen_height/2 - r.h()/2, scene_title(), 2, KRE::Color(1.f,1.f,1.f,alpha));
	}
	
	if(current_achievement && current_achievement_duration > 0) {
		--current_achievement_duration;

		ConstGuiSectionPtr left = GuiSection::get("achievements_left");
		ConstGuiSectionPtr right = GuiSection::get("achievements_right");
		ConstGuiSectionPtr mainsec = GuiSection::get("achievements_main");

		const ConstGraphicalFontPtr title_font = GraphicalFont::get("white_outline");
		const ConstGraphicalFontPtr main_font = GraphicalFont::get("door_label");

		const std::string title_text = _("achievement Unlocked!");
		const std::string name = current_achievement->name();
		const std::string description = "(" + current_achievement->description() + ")";
		const int width = std::max<int>(std::max<int>(
		  title_font->dimensions(title_text).w(),
		  main_font->dimensions(name).w()),
		  main_font->dimensions(description).w()
		  ) + 8;
		
		const int xpos = wnd->width() - 16 - left->width() - right->width() - width;
		const int ypos = 16;

		const float alpha = current_achievement_duration > 10 ? 1.0f : current_achievement_duration/10.0f;

		KRE::Canvas::ColorManager cm1(KRE::Color(1.0f, 1.0f, 1.0f, alpha));
		left->blit(xpos, ypos);
		mainsec->blit(xpos + left->width(), ypos, width, mainsec->height());
		right->blit(xpos + left->width() + width, ypos);

		title_font->draw(xpos + left->width(), ypos - 10, title_text);
		main_font->draw(xpos + left->width(), ypos + 24, name);

		KRE::Canvas::ColorManager cm2(KRE::Color(0.0f, 1.0f, 0.0f, alpha));
		main_font->draw(xpos + left->width(), ypos + 48, description);
	}
	
	if(pos.flip_rotate) {
		ASSERT_LOG(false, "fix flip_rotate");
		/*
		const double angle = sin(0.5*3.141592653589*GLfloat(pos.flip_rotate)/1000.0);
		const int pixels = (preferences::actual_screen_width()/2)*angle;
		
		
		//first draw black over the sections of the screen which aren't to be drawn to
		GLshort varray1[8] = {0,0,  GLshort(pixels),0,  GLshort(pixels),GLshort(preferences::actual_screen_height()),   0, GLshort(preferences::actual_screen_height())};
		GLshort varray2[8] = {GLshort(preferences::actual_screen_width() - pixels),0,  GLshort(preferences::actual_screen_width()),0,  GLshort(preferences::actual_screen_width()), GLshort(preferences::actual_screen_height()),  GLshort(preferences::actual_screen_width() - pixels), GLshort(preferences::actual_screen_height())};
		glColor4ub(0, 0, 0, 255);
#if defined(USE_SHADERS)
		gles2::manager gles2_manager(gles2::get_simple_shader());
		glViewport(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height());

		gles2::active_shader()->shader()->vertex_array(2, GL_SHORT, 0, 0,varray1);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		gles2::active_shader()->shader()->vertex_array(2, GL_SHORT, 0, 0, varray2);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
#else
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);		
		
		//glViewport(0, 0, fb->w, fb->h);
		glViewport(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height());
		glVertexPointer(2, GL_SHORT, 0, &varray1);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		glVertexPointer(2, GL_SHORT, 0, &varray2);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		glEnable(GL_TEXTURE_2D);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
#endif
		glColor4ub(255, 255, 255, 255);
		*/
	}

	if(need_rt && rt) {
		rt->renderToPrevious();
		rt->preRender(wnd);
		wnd->render(rt.get());
	}
}

namespace 
{
	boost::shared_ptr<performance_data> current_perf_data;
}

variant performance_data::getValue(const std::string& key) const
{
#define PERF_ATTR(m) if(key == #m) return variant(m);
	PERF_ATTR(fps);
	PERF_ATTR(cycles_per_second);
	PERF_ATTR(delay);
	PERF_ATTR(draw);
	PERF_ATTR(process);
	PERF_ATTR(flip);
	PERF_ATTR(cycle);
	PERF_ATTR(nevents);
#undef PERF_ATTR

	return variant();
}

void performance_data::getInputs(std::vector<game_logic::FormulaInput>* inputs) const
{
#define PERF_ATTR(m) inputs->push_back(std::string(#m))
	PERF_ATTR(fps);
	PERF_ATTR(cycles_per_second);
	PERF_ATTR(delay);
	PERF_ATTR(draw);
	PERF_ATTR(process);
	PERF_ATTR(flip);
	PERF_ATTR(cycle);
	PERF_ATTR(nevents);
#undef PERF_ATTR
}

void performance_data::set_current(const performance_data& d)
{
	current_perf_data.reset(new performance_data(d));
}

performance_data* performance_data::current()
{
	return current_perf_data.get();
}

void draw_fps(const Level& lvl, const performance_data& data)
{
	if(!preferences::debug()) {
		return;
	}

	std::ostringstream s;
	s << data.fps << "/" << data.cycles_per_second << "fps; max: " << data.max_frame_time << "ms; " << (data.draw/10) << "% draw; " << (data.flip/10) << "% flip; " << (data.process/10) << "% process; " << (data.delay/10) << "% idle; " << lvl.num_active_chars() << " objects; " << data.nevents << " events";

	std::ostringstream nets;

	if(controls::num_players() > 1) {
		//draw networking stats
		std::ostringstream s;
		nets << controls::packets_received() << " packets received; " << controls::num_errors() << " errors; " << controls::cycles_behind() << " behind; " << controls::their_highest_confirmed() << " remote cycles " << controls::last_packet_size() << " packet";

	}

	if(module::get_default_font() == "bitmap") {
		ConstGraphicalFontPtr font(GraphicalFont::get("door_label"));
		if(!font) {
			return;
		}

		rect area = font->draw(10, 60, s.str());
		if(!nets.str().empty()) {
			area = font->draw(10, area.y2() + 5, nets.str());
		}

		if(!data.profiling_info.empty()) {
			font->draw(10, area.y2() + 5, data.profiling_info);
		}
	} else {
		int y = 60;
		const int font_size = 18;
		auto canvas = KRE::Canvas::getInstance();
		auto t = KRE::Font::getInstance()->renderText(s.str(), KRE::Color::colorWhite(), font_size, false, module::get_default_font());
		canvas->blitTexture(t, 0, 10, y);
		y += t->surfaceHeight() + 5;
		if(!nets.str().empty()) {
			t = KRE::Font::getInstance()->renderText(nets.str(), KRE::Color::colorWhite(), font_size, false, module::get_default_font()); 
			canvas->blitTexture(t, 0, 10, y);
			y += t->surfaceHeight() + 5;
		}
		if(!data.profiling_info.empty()) {
			t = KRE::Font::getInstance()->renderText(data.profiling_info, KRE::Color::colorWhite(), font_size, false, module::get_default_font()); 
			canvas->blitTexture(t, 0, 10, y);
		}
	}
}

void add_debug_rect(const rect& r)
{
	current_debug_rects.push_back(r);
}
