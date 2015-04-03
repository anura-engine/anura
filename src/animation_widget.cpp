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

#include "ModelMatrixScope.hpp"

#include "animation_widget.hpp"
#include "custom_object_type.hpp"
#include "formatter.hpp"

namespace gui
{
	AnimationWidget::AnimationWidget(int w, int h, const variant& node)
		: anim_name_("default"),
		  type_(), 
		  cycle_(0), 
  		  play_sequence_count_(0), 
		  max_sequence_plays_(20)
	{
		setDim(w,h);
		if(node.is_map() && node.has_key("animation")) {
			nodes_ = node["animation"].as_list();
		} else if(node.is_list()) {
			nodes_ = node.as_list();
		} else {
			ASSERT_LOG(false, "AnimationWidget: passed in node must be either a list of animations or a map containing an \"animation\" list.");
		}
		init();
	}

	AnimationWidget::AnimationWidget(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v,e), 
		  anim_name_("default"),
		  type_(),
  		  cycle_(0), 
		  play_sequence_count_(0)
	{
		if(v.has_key("animation")) {
			nodes_ = v["animation"].as_list();
		}
		if(v.has_key("object") && v["object"].is_string()) {
			type_ = v["object"].as_string();	
		}
		if(v.has_key("name") && v["name"].is_string()) {
			anim_name_ = v["name"].as_string();
		}
		max_sequence_plays_ = v["max_sequence_plays"].as_int(20);
		// Range of other options to add display label true/false
		// Auto-repeat single frame (id) -- more useful from setValue()
		// Arbitrary label (as string or map)
		init();
	}

	void AnimationWidget::init()
	{
		play_sequence_count_ = 0;

		if(!nodes_.empty()) {
			current_anim_ = nodes_.begin();
			frame_.reset(new Frame(*current_anim_));
			label_.reset(new Label(frame_->id(), KRE::Color::colorYellow(), 16));
			label_->setLoc((width() - label_->width())/2, height()-label_->height());
		} else {
			auto obj_type = CustomObjectType::getOrDie(type_);
			frame_.reset(new Frame(obj_type->getFrame(anim_name_)));

			//label_.reset(new Label(formatter() << "Loc: " << x() << "," << y(), KRE::Color::colorYellow(), 16));
			//label_->setLoc((width() - label_->width())/2, height()-label_->height());
		}
	}

	void AnimationWidget::handleDraw() const
	{
		rect preview_area(x(), y(), width(), height() - (label_ ? label_->height() : 0));
		const float scale = 1.0f;
		//const float scale = static_cast<float>(std::min(preview_area.w()/frame_->width(), preview_area.h()/frame_->height()));
		const int framex = preview_area.x() + (preview_area.w() - int(frame_->width()*scale))/2;
		const int framey = preview_area.y() + (preview_area.h() - int(frame_->height()*scale))/2;
		frame_->draw(nullptr, framex, framey, true, false, cycle_, 0, 1.0f);
		if(++cycle_ >= frame_->duration()) {
			cycle_ = 0;
			if(++play_sequence_count_ > max_sequence_plays_) {
				play_sequence_count_ = 0;
				if(!nodes_.empty()) {
					if(++current_anim_ == nodes_.end()) {
						current_anim_ = nodes_.begin();
					}
					frame_.reset(new Frame(*current_anim_));
					label_.reset(new Label(frame_->id(), KRE::Color::colorYellow(), 16));
					label_->setLoc((width() - label_->width())/2, height()-label_->height());
				}
			}
		}
		if(label_) {
			label_->draw(x(), y(), getRotation(), getScale());
		}
	}

	BEGIN_DEFINE_CALLABLE(AnimationWidget, Widget)
		DEFINE_FIELD(cycle, "int")
			return variant(obj.cycle_);
		DEFINE_SET_FIELD
			obj.cycle_ = value.as_int();
			if(++obj.cycle_ >= obj.frame_->duration()) {
				obj.cycle_ = 0;
			}
	END_DEFINE_CALLABLE(AnimationWidget)
}
