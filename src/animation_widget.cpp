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
#include "animation_widget.hpp"
#include "color_utils.hpp"
#include "raster.hpp"

namespace gui {

animation_widget::animation_widget(int w, int h, const variant& node)
	: cycle_(0), play_sequence_count_(0), max_sequence_plays_(20)
{
	set_dim(w,h);
	if(node.is_map() && node.has_key("animation")) {
		nodes_ = node["animation"].as_list();
	} else if(node.is_list()) {
		nodes_ = node.as_list();
	} else {
		ASSERT_LOG(false, "animation_widget: passed in node must be either a list of animations or a map containing an \"animation\" list.");
	}
	init();
}

animation_widget::animation_widget(const variant& v, game_logic::formula_callable* e)
	: widget(v,e), cycle_(0), play_sequence_count_(0)
{
	nodes_ = v["animation"].as_list();
	max_sequence_plays_ = v["max_sequence_plays"].as_int(20);
	// Range of other options to add display label true/false
	// Auto-repeat single frame (id) -- more useful from set_value()
	// Arbitrary label (as string or map)
	init();
}

void animation_widget::init()
{
	current_anim_ = nodes_.begin();
	play_sequence_count_ = 0;

	frame_.reset(new frame(*current_anim_));
	label_.reset(new label(frame_->id(), graphics::color_yellow(), 16));
	label_->set_loc((width() - label_->width())/2, height()-label_->height());
}

void animation_widget::handle_draw() const
{
	rect preview_area(x(), y(), width(), height() - (label_ ? label_->height() : 0));
	const GLfloat scale = GLfloat(std::min(preview_area.w()/frame_->width(), preview_area.h()/frame_->height()));
	const int framex = preview_area.x() + (preview_area.w() - int(frame_->width()*scale))/2;
	const int framey = preview_area.y() + (preview_area.h() - int(frame_->height()*scale))/2;
	frame_->draw(framex, framey, true, false, cycle_, 0, scale);
	if(++cycle_ >= frame_->duration()) {
		cycle_ = 0;
		if(++play_sequence_count_ > max_sequence_plays_) {
			play_sequence_count_ = 0;
			if(++current_anim_ == nodes_.end()) {
				current_anim_ = nodes_.begin();
			}
			frame_.reset(new frame(*current_anim_));
			label_.reset(new label(frame_->id(), graphics::color_yellow(), 16));
			label_->set_loc((width() - label_->width())/2, height()-label_->height());
		}
	}
	glPushMatrix();
	glTranslatef(GLfloat(x() & ~1), GLfloat(y() & ~1), 0.0);
	if(label_) {
		label_->draw();
	}
	glPopMatrix();
}

}
