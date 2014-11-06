/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#include "graphics.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <boost/lexical_cast.hpp>
#include <boost/scoped_ptr.hpp>

#include "asserts.hpp"
#include "controls.hpp"
#include "debug_console.hpp"
#include "draw_number.hpp"
#include "draw_scene.hpp"
#include "editor.hpp"
#include "font.hpp"
#include "foreach.hpp"
#include "formula_profiler.hpp"
#include "globals.h"
#include "graphical_font.hpp"
#include "gui_section.hpp"
#include "i18n.hpp"
#include "level.hpp"
#include "message_dialog.hpp"
#include "player_info.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "speech_dialog.hpp"
#include "texture.hpp"
#include "texture_frame_buffer.hpp"

#include "tooltip.hpp"

namespace {

std::vector<rect> current_debug_rects;
int current_debug_rects_valid_cycle = -1;

std::string& scene_title() {
	static std::string title;
	return title;
}

achievement_ptr current_achievement;
int current_achievement_duration = 0;

	
struct screen_flash {
	graphics::color_transform color, delta;
	int duration;
};

std::vector<screen_flash>& flashes() {
	static std::vector<screen_flash> obj;
	return obj;
}

int scene_title_duration_;

screen_position last_position;
}

bool is_achievement_displayed() {
	return current_achievement && current_achievement_duration > 0;
}

screen_position& last_draw_position()
{
	return last_position;
}

namespace {
int g_flash_disable = 0;
}

disable_flashes_scope::disable_flashes_scope()
{
	++g_flash_disable;
}

disable_flashes_scope::~disable_flashes_scope()
{
	--g_flash_disable;
}

void screen_color_flash(const graphics::color_transform& color, const graphics::color_transform& color_delta, int duration)
{
	if(!g_flash_disable) {
		screen_flash f = { color, color_delta, duration };
		flashes().push_back(f);
	}
}

void set_scene_title(const std::string& msg, int duration) {
	//explicitly translate all level titles
	scene_title() = (msg.size() > 0) ? _(msg) : msg;
	scene_title_duration_ = duration;
}

void set_displayed_achievement(achievement_ptr a)
{
	current_achievement = a;
	current_achievement_duration = 250;
}

GLfloat hp_ratio = -1.0;
void draw_scene(const level& lvl, screen_position& pos, const entity* focus, bool do_draw) {
	const bool draw_ready = update_camera_position(lvl, pos, focus, do_draw);
	if(draw_ready) {
		render_scene(lvl, pos);
	}
}

bool update_camera_position(const level& lvl, screen_position& pos, const entity* focus, bool do_draw) {
	if(focus == NULL && lvl.player()) {
		focus = &lvl.player()->get_entity();
	}

	//flag which gets set to false if we abort drawing, due to the
	//screen position being initialized now.
	const bool draw_level = do_draw && pos.init;
	
#ifndef NO_EDITOR
	const int sidebar_width = editor::sidebar_width();
	const int codebar_height = editor::codebar_height();
#else
	const int sidebar_width = 0;
	const int codebar_height = 0;
#endif

	const int screen_width = graphics::screen_width() - (lvl.in_editor() ? sidebar_width : 0);
	const int screen_height = graphics::screen_height() - (lvl.in_editor() ? codebar_height : 0);

	ASSERT_LOG(focus || lvl.in_editor(), "No player found in level. Must have a player object. (An object with is_human: true marked");

	if(focus) {

		const float target_zoom = lvl.zoom_level().as_float();
		const float ZoomSpeed = 0.03;
		const float prev_zoom = pos.zoom;
		if(std::abs(target_zoom - pos.zoom) < ZoomSpeed) {
			pos.zoom = target_zoom;
		} else if(pos.zoom > target_zoom) {
			pos.zoom -= ZoomSpeed;
		} else {
			pos.zoom += ZoomSpeed;
		}
        
        //if we've set the zoom inside the very first cycle of a level (i.e. using on_start_level), then we're doing some kind of cutscene which has the camera start zoomed out.  We want the camera to immediately start in this state, not "progress to this state gradually from the normal zoom". 
        if(lvl.cycle() == 1){
            pos.zoom = target_zoom;
        }

		// If the camera is automatically moved along by the level (e.g. a 
		// hurtling through the sky level) do that here.
		pos.x_pos += lvl.auto_move_camera_x()*100;
		pos.y_pos += lvl.auto_move_camera_y()*100;

		//find how much padding will have to be on the edge of the screen due
		//to the level being wider than the screen. This value will be 0
		//if the level is larger than the screen (i.e. most cases)
		const int x_screen_pad = std::max<int>(0, screen_width - lvl.boundaries().w());

		const int y_screen_pad = std::max<int>(0, screen_height - lvl.boundaries().h());
		pos.x_border = x_screen_pad/2;
		pos.y_border = y_screen_pad/2;

		//find the boundary values for the camera position based on the size
		//of the level. These boundaries keep the camera from ever going out
		//of the bounds of the level.
		
		const float inverse_zoom_level = 1.0/pos.zoom;

		//std::cerr << "BOUNDARIES: " << lvl.boundaries().x() << ", " << lvl.boundaries().x2() << " WIDTH: " << screen_width << " PAD: " << x_screen_pad << "\n";

		//we look a certain number of frames ahead -- assuming the focus
		//keeps moving at the current velocity, we converge toward the point
		//they will be at in x frames.
		const int PredictiveFramesHorz = 20;
		const int PredictiveFramesVert = 5;

		int displacement_x = 0, displacement_y = 0;
		if(pos.focus_x || pos.focus_y) {
			displacement_x = focus->feet_x() - pos.focus_x;
			displacement_y = focus->feet_y() - pos.focus_y;
		}

		pos.focus_x = focus->feet_x();
		pos.focus_y = focus->feet_y();

		//find the point we want the camera to converge toward. It will be the
		//feet of the player, but inside the boundaries we calculated above.
		int x = focus->feet_x() + displacement_x*PredictiveFramesHorz;

		//calculate the adjustment to the camera's target position based on
		//our vertical look. This is calculated as the square root of the
		//vertical look, to make the movement slowly converge.
		const int vertical_look = focus->vertical_look();

		//find the y point for the camera to converge toward
		int y = focus->feet_y() - (screen_height/(5*lvl.zoom_level())).as_int() + displacement_y*PredictiveFramesVert + vertical_look;

		//std::cerr << "POSITION: " << x << "," << y << " IN " << min_x << "," << min_y << "," << max_x << "," << max_y << "\n";

		if(lvl.focus_override().empty() == false) {
			std::vector<entity_ptr> v = lvl.focus_override();
			int left = 0, right = 0, top = 0, bottom = 0;
			while(v.empty() == false) {
				left = v[0]->feet_x();
				right = v[0]->feet_x();
				top = v[0]->feet_y();
				bottom = v[0]->feet_y();
				foreach(const entity_ptr& e, v) {
					left = std::min<int>(e->feet_x(), left);
					right = std::max<int>(e->feet_x(), right);
					top = std::min<int>(e->feet_y(), top);
					bottom = std::min<int>(e->feet_y(), bottom);
				}

				const int BorderSize = 20;
				if(v.size() == 1 || right - left < screen_width/lvl.zoom_level() - BorderSize && bottom - top < screen_height/lvl.zoom_level() - BorderSize) {
					break;
				}

				break;

				v.pop_back();
			}

			x = (left + right)/2;
			y = ((top + bottom)/2 - screen_height/(5*lvl.zoom_level())).as_int();
		}

		pos.target_xpos = 100*(x - screen_width/2);
		pos.target_ypos = 100*(y - screen_height/2);

		//std::cerr << "POSITION2: " << x << "," << y << " IN " << min_x << "," << min_y << "," << max_x << "," << max_y << "\n";
		if(lvl.lock_screen()) {
			x = lvl.lock_screen()->x;
			y = lvl.lock_screen()->y;
		}
		//std::cerr << "POSITION3: " << x << "," << y << " IN " << min_x << "," << min_y << "," << max_x << "," << max_y << "\n";

		//for small screens the speech dialog arrows cover the entities they are
		//pointing to. adjust to that by looking up a little bit.
		if (lvl.current_speech_dialog() && preferences::virtual_screen_height() < 600)
			y = (y + (600 - screen_height)/(2*lvl.zoom_level())).as_int();

		//find the target x,y position of the camera in centi-pixels. Note that
		//(x,y) represents the position the camera should center on, while
		//now we're calculating the top-left point.
		//
		//the actual camera position will converge toward this point
		const int target_xpos = 100*(x - screen_width/2);
		const int target_ypos = 100*(y - screen_height/2);

		if(pos.init == false) {
			pos.x_pos = target_xpos;
			pos.y_pos = target_ypos;
			pos.init = true;
		} else {

			//Make (pos.x_pos, pos.y_pos) converge toward (target_xpos,target_ypos).
			//We do this by moving asymptotically toward the target, which
			//makes the camera have a nice acceleration/decceleration effect
			//as the target position moves.
			const int horizontal_move_speed = (30/lvl.zoom_level()).as_int();
			const int vertical_move_speed = (10/lvl.zoom_level()).as_int();
			int xdiff = (target_xpos - pos.x_pos)/horizontal_move_speed;
			int ydiff = (target_ypos - pos.y_pos)/vertical_move_speed;

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
		if ((std::abs(pos.shake_x_vel) < 50) && (std::abs(pos.shake_x_offset) < 50)){
			//prematurely end the oscillation if it's in the asymptote
			pos.shake_x_offset = 0;
			pos.shake_x_vel = 0;
		}else{
			//extraneous signs kept for consistency with conventional spring physics, also
			//the value that "offset" is divided by, is (the inverse of) 'k', aka "spring stiffness"
			//the value that "velocity" is divided by, is (the inverse of) 'b', aka "oscillation damping",
			//which causes the spring to come to rest.
			//These values are very sensitive, and tweaking them wrongly will cause the spring to 'explode',
			//and increase its motion out of game-bounds. 
			if(pos.shake_x_offset > 0) {
				pos.shake_x_vel -= (1 * pos.shake_x_offset/3 + pos.shake_x_vel/15);
			} else if(pos.shake_x_offset < 0) {
				pos.shake_x_vel += (-1 * pos.shake_x_offset/3 - pos.shake_x_vel/15);
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

		const int minmax_x_adjust = screen_width*(1.0 - inverse_zoom_level)*0.5;
		const int minmax_y_adjust = screen_height*(1.0 - inverse_zoom_level)*0.5;
	
		int min_x = (lvl.boundaries().x() - minmax_x_adjust)*100;
		int min_y = (lvl.boundaries().y() - minmax_y_adjust)*100;
		int max_x = (lvl.boundaries().x2() - minmax_x_adjust - screen_width*inverse_zoom_level)*100;
		int max_y = (lvl.boundaries().y2() - minmax_y_adjust - screen_height*inverse_zoom_level)*100;

		if(min_x > max_x) {
			min_x = max_x = (min_x + max_x)/2;
		}

		if(min_y > max_y) {
			min_y = max_y = (min_y + max_y)/2;
		}

		pos.x = std::min(std::max(pos.x_pos, min_x), max_x);
		pos.y = std::min(std::max(pos.y_pos, min_y), max_y);
	}


	last_position = pos;

	return draw_level;
}

void render_scene(const level& lvl, const screen_position& pos) {
		formula_profiler::instrument instrumentation("DRAW");
#ifndef NO_EDITOR
	const int sidebar_width = editor::sidebar_width();
#else
	const int sidebar_width = 0;
#endif
	const int screen_width = graphics::screen_width() - (lvl.in_editor() ? sidebar_width : 0);

	get_main_window()->prepare_raster();
	glPushMatrix();

	const int camera_rotation = lvl.camera_rotation();
	if(camera_rotation) {
		GLfloat rotate = GLfloat(camera_rotation)/1000.0;
		glRotatef(rotate, 0.0, 0.0, 1.0);
	}

	if(pos.flip_rotate) {
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		const double angle = sin(0.5*3.141592653589*GLfloat(pos.flip_rotate)/1000.0);
		const int pixels = (preferences::actual_screen_width()/2)*angle;
		
		//then squish all future drawing inwards
		glViewport(pixels, 0, preferences::actual_screen_width() - pixels*2, preferences::actual_screen_height());
	}

	int xscroll = (pos.x/100)&preferences::xypos_draw_mask;
	int yscroll = (pos.y/100)&preferences::xypos_draw_mask;

	int bg_xscroll = xscroll;
	int bg_yscroll = yscroll;

//	if(pos.zoom > 1.0) {
		glScalef(pos.zoom, pos.zoom, 0);
		xscroll += (screen_width/2)*(-1.0/pos.zoom + 1.0);
		yscroll += (graphics::screen_height()/2)*(-1.0/pos.zoom + 1.0);
//	}

	if(pos.zoom < 1.0) {
		bg_xscroll = xscroll;
		bg_yscroll = yscroll;
	}

	glTranslatef(-xscroll, -yscroll, 0);
	lvl.draw_background(bg_xscroll, bg_yscroll, camera_rotation);

	int draw_width = screen_width;
	int draw_height = graphics::screen_height();
	if(pos.zoom < 1.0) {
		draw_width /= pos.zoom;
		draw_height /= pos.zoom;
	}
	lvl.draw(xscroll, yscroll, draw_width, draw_height);

	foreach(const rect& r, current_debug_rects) {
		graphics::draw_rect(r, graphics::color(0, 0, 255, 175));
	}

	if(current_debug_rects_valid_cycle != lvl.cycle()) {
		current_debug_rects.clear();
	}

	current_debug_rects_valid_cycle = lvl.cycle();

	graphics::clear_raster_distortion();

	lvl.draw_later(xscroll, yscroll, draw_width, draw_height);
	glPopMatrix();

	lvl.draw_absolutely_positioned_objects();

	for(std::vector<screen_flash>::iterator i = flashes().begin();
	    i != flashes().end(); ) {
		const graphics::color& tint = i->color.to_color();
		if(tint.a() > 0) {
			graphics::draw_rect(rect(0, 0, graphics::screen_width(), graphics::screen_height()), tint);
		}

		i->color = graphics::color_transform(i->color.r() + i->delta.r(),
		                                     i->color.g() + i->delta.g(),
		                                     i->color.b() + i->delta.b(),
		                                     i->color.a() + i->delta.a());

		if(--i->duration <= 0) {
			i = flashes().erase(i);
		} else {
			++i;
		}
	}
	
	//draw borders around the screen if the screen is bigger than the level.
	if(pos.x_border > 0) {
		graphics::draw_rect(rect(0, 0, pos.x_border, graphics::screen_height()), graphics::color(0,0,0,255));
		graphics::draw_rect(rect(graphics::screen_width() - pos.x_border, 0, pos.x_border, graphics::screen_height()), graphics::color(0,0,0,255));
	}

	if(pos.y_border > 0) {
		graphics::draw_rect(rect(pos.x_border, 0, graphics::screen_width() - pos.x_border*2, pos.y_border), graphics::color(0,0,0,255));
		graphics::draw_rect(rect(pos.x_border, graphics::screen_height() - pos.y_border, graphics::screen_width() - pos.x_border*2, pos.y_border), graphics::color(0,0,0,255));
	}


#ifndef NO_EDITOR
	debug_console::draw();
#endif
	gui::draw_tooltip();

	debug_console::draw_graph();

	if (!pause_stack) lvl.draw_status();
	gles2::active_shader()->prepare_draw();

	if(scene_title_duration_ > 0) {
		--scene_title_duration_;
		const const_graphical_font_ptr f = graphical_font::get("default");
		ASSERT_LOG(f.get() != NULL, "COULD NOT LOAD DEFAULT FONT");
		const rect r = f->dimensions(scene_title());
		const GLfloat alpha = scene_title_duration_ > 10 ? 1.0 : scene_title_duration_/10.0;
		{
			glColor4ub(0, 0, 0, 128*alpha);
			f->draw(graphics::screen_width()/2 - r.w()/2 + 2, graphics::screen_height()/2 - r.h()/2 + 2, scene_title());
			glColor4ub(255, 255, 255, 255*alpha);
		}

		{
			f->draw(graphics::screen_width()/2 - r.w()/2, graphics::screen_height()/2 - r.h()/2, scene_title());
			glColor4ub(255, 255, 255, 255);
		}
	}
	
	if(current_achievement && current_achievement_duration > 0) {
		--current_achievement_duration;

		const_gui_section_ptr left = gui_section::get("achievements_left");
		const_gui_section_ptr right = gui_section::get("achievements_right");
		const_gui_section_ptr main = gui_section::get("achievements_main");

		const const_graphical_font_ptr title_font = graphical_font::get("white_outline");
		const const_graphical_font_ptr main_font = graphical_font::get("door_label");

		const std::string title_text = _("Achievement Unlocked!");
		const std::string name = current_achievement->name();
		const std::string description = "(" + current_achievement->description() + ")";
		const int width = std::max<int>(std::max<int>(
		  title_font->dimensions(title_text).w(),
		  main_font->dimensions(name).w()),
		  main_font->dimensions(description).w()
		  ) + 8;
		
		const int xpos = graphics::screen_width() - 16 - left->width() - right->width() - width;
		const int ypos = 16;

		const GLfloat alpha = current_achievement_duration > 10 ? 1.0 : current_achievement_duration/10.0;

		glColor4f(1.0, 1.0, 1.0, alpha);

		left->blit(xpos, ypos);
		main->blit(xpos + left->width(), ypos, width, main->height());
		right->blit(xpos + left->width() + width, ypos);

		title_font->draw(xpos + left->width(), ypos - 10, title_text);
		main_font->draw(xpos + left->width(), ypos + 24, name);

		glColor4f(0.0, 1.0, 0.0, alpha);
		main_font->draw(xpos + left->width(), ypos + 48, description);
		glColor4f(1.0, 1.0, 1.0, 1.0);
		
	}
	gles2::active_shader()->prepare_draw();
	
	if(pos.flip_rotate) {
		const double angle = sin(0.5*3.141592653589*GLfloat(pos.flip_rotate)/1000.0);
		//const int pixels = (fb->w/2)*angle;
		const int pixels = (preferences::actual_screen_width()/2)*angle;
		
		
		//first draw black over the sections of the screen which aren't to be drawn to
		//GLshort varray1[8] = {0,0,  pixels,0,  pixels,fb->h,   0,fb->h};
		//GLshort varray2[8] = {fb->w - pixels,0,  fb->w,0,   fb->w,fb->h,  fb->w - pixels,fb->h};
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
	}
}

namespace {
boost::intrusive_ptr<performance_data> current_perf_data;
}

variant performance_data::get_value(const std::string& key) const
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

void performance_data::get_inputs(std::vector<game_logic::formula_input>* inputs) const
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

void draw_fps(const level& lvl, const performance_data& data)
{
	if(!preferences::debug()) {
		return;
	}

	const_graphical_font_ptr font(graphical_font::get("door_label"));
	if(!font) {
		return;
	}
	std::ostringstream s;
	s << data.fps << "/" << data.cycles_per_second << "fps; " << (data.draw/10) << "% draw; " << (data.flip/10) << "% flip; " << (data.process/10) << "% process; " << (data.delay/10) << "% idle; " << lvl.num_active_chars() << " objects; " << data.nevents << " events";

	rect area = font->draw(10, 60, s.str());

	if(controls::num_players() > 1) {
		//draw networking stats
		std::ostringstream s;
		s << controls::packets_received() << " packets received; " << controls::num_errors() << " errors; " << controls::cycles_behind() << " behind; " << controls::their_highest_confirmed() << " remote cycles " << controls::last_packet_size() << " packet";

		area = font->draw(10, area.y2() + 5, s.str());
	}

	if(!data.profiling_info.empty()) {
		font->draw(10, area.y2() + 5, data.profiling_info);
	}
}

void add_debug_rect(const rect& r)
{
	current_debug_rects.push_back(r);
}
