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

#ifndef NO_EDITOR

#include <sstream>
#include <fstream>
#include <math.h>

#include "WindowManager.hpp"

#include "animation_creator.hpp"
#include "draw_scene.hpp"
#include "dropdown_widget.hpp"
#include "file_chooser_dialog.hpp"
#include "frame.hpp"
#include "message_dialog.hpp"
#include "module.hpp"
#include "slider.hpp"
 
namespace gui 
{
	using std::placeholders::_1;
	using std::placeholders::_2;

	namespace 
	{
		int slider_transform(double d)
		{
			// normalize to [-20.0,20.0] range.
			d = (d - 0.5) * 2.0 * 20;
			double d_abs = std::abs(d);
			if(d_abs > 10) {
				// Above 10 units we go non-linear.
				return int((d < 0 ? -1.0 : 1.0) * pow(10, d_abs/10));
			}
			return int(d);
		}

		// Given two variants, which are maps merge the properties from v2 into v1 that
		// don't already exist in v1.
		void variant_map_merge(variant& v1, const variant& v2)
		{
			std::map<variant, variant>::const_iterator v2it = v2.as_map().begin();
			std::map<variant, variant>::const_iterator v2end = v2.as_map().end();
			while(v2it != v2end) {
				std::map<variant, variant>::const_iterator v1it = v1.as_map().find(v2it->first);
				if(v1it == v1.as_map().end()) {
					v1.add_attr(v2it->first, v2it->second);
				}
				v2it++;
			}
		}

		void load_default_properties(std::map<variant, variant>* def_properties)
		{
			// integer properties
			(*def_properties)[variant("frames")] = variant(1);
			(*def_properties)[variant("frames_per_row")] = variant(-1);
			(*def_properties)[variant("duration")] = variant(-1);
			(*def_properties)[variant("pad")] = variant(0);
			(*def_properties)[variant("rotate")] = variant(0);
			(*def_properties)[variant("blur")] = variant(0);
			(*def_properties)[variant("damage")] = variant(0);
			(*def_properties)[variant("feet_x")] = variant(0);
			(*def_properties)[variant("feet_y")] = variant(0);
			(*def_properties)[variant("velocity_x")] = variant(std::numeric_limits<int>::min());
			(*def_properties)[variant("velocity_y")] = variant(std::numeric_limits<int>::min());
			(*def_properties)[variant("accel_x")] = variant(std::numeric_limits<int>::min());
			(*def_properties)[variant("accel_y")] = variant(std::numeric_limits<int>::min());
			(*def_properties)[variant("scale")] = variant(2);
		
			(*def_properties)[variant("id")] = variant("id");
			//(*def_properties)[variant("image")] = variant("");

			std::vector<variant> v;
			//rects
			(*def_properties)[variant("rect")] = variant(&v);
			(*def_properties)[variant("collide")] = variant(&v);
			(*def_properties)[variant("hit")] = variant(&v);
			(*def_properties)[variant("platform")] = variant(&v);

			// some bools
			(*def_properties)[variant("reverse")] = variant::from_bool(false);
			(*def_properties)[variant("play_backwards")] = variant::from_bool(false);
			(*def_properties)[variant("rotate_on_slope")] = variant::from_bool(false);
		}

		std::map<variant, variant>& get_default_properties()
		{
			static std::map<variant, variant> defs;
			if(defs.empty()) {
				load_default_properties(&defs);
			}
			return defs;
		}
	}


	AnimationCreatorDialog::AnimationCreatorDialog(int x, int y, int w, int h, const variant& anims)
		: Dialog(x,y,w,h), 
		selected_frame_(-1), 
		dragging_slider_(false), 
		changed_(false), 
		simple_options_(true)
	{
		setClearBgAmount(255);
		resetCurrentObject();
		if(anims.is_list()) {
			anims_ = anims.as_list();
			if(anims_.size() > 0) {
				selected_frame_ = 0;
				current_ = anims_[selected_frame_];
			}
		} else if(anims.is_map() && anims.has_key("image")){
			anims_.push_back(anims);
		}

		setProcessHook(std::bind(&AnimationCreatorDialog::process, this));
		init();
	}

	std::vector<std::string> AnimationCreatorDialog::commonAnimationList()
	{
		// List of common animations.
		std::vector<std::string> v;
		v.push_back("stand");
		v.push_back("normal");
		v.push_back("hurt");
		v.push_back("turn");
		v.push_back("walk");
		v.push_back("spring");
		v.push_back("fly");
		v.push_back("jump");
		v.push_back("fall");
		v.push_back("open");
		v.push_back("ajar");
		v.push_back("close");
		v.push_back("land");
		v.push_back("thrown");
		v.push_back("lose_wings");
		v.push_back("portrait");
		v.push_back("swim");
		v.push_back("attack");
		v.push_back("cling");
		v.push_back("fire");
		v.push_back("jump_attack");
		v.push_back("run");
		v.push_back("crouch");
		v.push_back("enter_crouch");
		v.push_back("enter_lookup");
		v.push_back("flash");
		v.push_back("leave_crouch");
		v.push_back("lookup");
		v.push_back("pushed");
		v.push_back("roll");
		v.push_back("run_attack");
		v.push_back("shoot");
		return v;
	}

	void AnimationCreatorDialog::init()
	{
		const int border_offset = 35;
		int current_height = 35;
		int hpad = 10;

		clear();

		// Add copy desintation box
		GridPtr g(new Grid(2));
		g->setHpad(20);
		g->addCol(ButtonPtr(new Button(new Label("Set Destination", 14), std::bind(&AnimationCreatorDialog::setDestination, this))))
			.addCol(LabelPtr(new Label(copy_path_, KRE::Color::colorGreen(), 14)));
		g->addCol(WidgetPtr(new Label("", KRE::Color::colorYellow(), 12)))
			.addCol(WidgetPtr(new Label("Images will be copied to the destination directory", KRE::Color::colorYellow(), 12)));
		addWidget(g, border_offset, current_height);
		current_height += g->height() + hpad;

		// Add current list of animations
		g.reset(new Grid(3));
		g->setDim(width()/2, height()/5);
		g->setMaxHeight(height()/5);
		g->setShowBackground(true);
		g->setHpad(10);
		g->setHeaderRow(0);
		g->allowSelection(true);
		g->addCol(LabelPtr(new Label("Identifier", 14)))
			.addCol(LabelPtr(new Label("Image Path", 14)))
			.addCol(LabelPtr(new Label("Area in Image", 14)));
		for(const variant& v : anims_) {
			std::stringstream ss;
			rect r;
			if(v.has_key("rect")) {
				r = rect(v["rect"]);
			} else if(v.has_key("x") && v.has_key("y") && v.has_key("w") && v.has_key("h")) {
				r = rect(v["x"].as_int(),
						v["y"].as_int(),
						v["w"].as_int(),
						v["h"].as_int());
			}
			ss << r;
			g->addCol(LabelPtr(new Label(v.has_key("id") ? v["id"].as_string() : "<missing>", 12)))
				.addCol(LabelPtr(new Label(v.has_key("image") ? v["image"].as_string() : "", 12)))
				.addCol(LabelPtr(new Label(ss.str(), 12)));
		}
		g->registerSelectionCallback(std::bind(&AnimationCreatorDialog::selectAnimation, this, _1));
		addWidget(g, border_offset, current_height);
		current_height += g->height() + hpad;

		g.reset(new Grid(3));
		g->setMaxHeight(int(height()/2 - 50));
		g->setZOrder(1);

		DropdownWidgetPtr id_entry(new DropdownWidget(commonAnimationList(), 150, 28, DropdownType::COMBOBOX));
		id_entry->setFontSize(14);
		id_entry->setText(current_.has_key("id") ? current_["id"].as_string() : "normal");
		id_entry->setDropdownHeight(height() - current_height - border_offset);
		id_entry->setOnChangeHandler(std::bind(&AnimationCreatorDialog::onIdChange, this, id_entry, _1));
		id_entry->setOnSelectHandler(std::bind(&AnimationCreatorDialog::onIdSet, this, id_entry, _1, _2));
		g->addCol(WidgetPtr(new Label("Identifier: ", KRE::Color::colorWhite(), 14)))
			.addCol(WidgetPtr(id_entry))
			.finishRow();

		g->addCol(ButtonPtr(new Button(new Label("Choose Image File", 14), std::bind(&AnimationCreatorDialog::setImageFile, this))))
			.addCol(WidgetPtr(new Label(rel_path_, KRE::Color::colorGreen(), 14)))
			.finishRow();

		for(auto& p : current_.as_map()) {
			if(p.second.is_int() && showAttribute(p.first)) {
				TextEditorWidgetPtr entry(new TextEditorWidget(100, 28));
				std::stringstream ss;
				ss << p.second.as_int();
				entry->setText(ss.str());
				SliderPtr slide(new Slider(200, std::bind((&AnimationCreatorDialog::changeSlide), this, p.first.as_string(), entry, _1), 0.5));
				slide->setDragEnd(std::bind(&AnimationCreatorDialog::endSlide, this, p.first.as_string(), slide, entry, _1));
				entry->setOnChangeHandler(std::bind(&AnimationCreatorDialog::changeText, this, p.first.as_string(), entry, slide));
				entry->setOnEnterHandler(std::bind(&AnimationCreatorDialog::executeChangeText, this, p.first.as_string(), entry, slide));
				entry->setOnTabHandler(std::bind(&AnimationCreatorDialog::executeChangeText, this, p.first.as_string(), entry, slide));
				g->addCol(WidgetPtr(new Label(p.first.as_string(), KRE::Color::colorWhite(), 12)))
					.addCol(entry)
					.addCol(slide);
			}
		}
		addWidget(g, border_offset, current_height);
		current_height += g->height() + hpad;

		// Add/Delete animation buttons
		g.reset(new Grid(4));
		g->setHpad(50);
		g->addCol(ButtonPtr(new Button(new Label("New", 14), std::bind(&AnimationCreatorDialog::animNew, this))))
			.addCol(ButtonPtr(new Button(new Label("Save", 14), std::bind(&AnimationCreatorDialog::animSave, this, static_cast<Dialog*>(nullptr)))))
			.addCol(ButtonPtr(new Button(new Label("Delete", 14), std::bind(&AnimationCreatorDialog::animDel, this))))
			.addCol(ButtonPtr(new Button(new Label("Finish", 14), std::bind(&AnimationCreatorDialog::finish, this))));
		addWidget(g, border_offset, height() - border_offset - g->height());
		current_height = height() - border_offset - g->height();

		CheckboxPtr cb = new Checkbox("Simplified Options", simple_options_, std::bind(&AnimationCreatorDialog::setOption, this), BUTTON_SIZE_DOUBLE_RESOLUTION);
		addWidget(cb, border_offset, current_height - cb->height() - 10);
	}

	void AnimationCreatorDialog::process()
	{
		int border_offset = 35;
		try {
			if(AnimationPreviewWidget::is_animation(current_)) {
				if(!animation_preview_) {
					animation_preview_.reset(new AnimationPreviewWidget(current_));
					animation_preview_->setRectHandler(std::bind(&AnimationCreatorDialog::setAnimationRect, this, _1));
					animation_preview_->setSolidHandler(std::bind(&AnimationCreatorDialog::moveSolidRect, this, _1, _2));
					animation_preview_->setPadHandler(std::bind(&AnimationCreatorDialog::setIntegerAttr, this, "pad", _1));
					animation_preview_->setNumFramesHandler(std::bind(&AnimationCreatorDialog::setIntegerAttr, this, "frames", _1));
					animation_preview_->setFramesPerRowHandler(std::bind(&AnimationCreatorDialog::setIntegerAttr, this, "frames_per_row", _1));
					animation_preview_->setLoc(width() - int(width()*0.42) - border_offset, border_offset);
					animation_preview_->setDim(int(width()*0.42), height()-border_offset*2);
					animation_preview_->init();
				} else {
					animation_preview_->setObject(current_);
				}
			}
		} catch (type_error&) {
			if(animation_preview_) {
				animation_preview_.reset();
			}
		} catch(Frame::Error&) {
			// skip
		} catch(validation_failure_exception&) {
			if(animation_preview_) {
				animation_preview_.reset();
			}
		} catch(KRE::ImageLoadError&) {
			if(animation_preview_) {
				animation_preview_.reset();
			}
		}

		if(animation_preview_) {
			animation_preview_->process();
		}
	}

	void AnimationCreatorDialog::handleDraw() const
	{
		Dialog::handleDraw();

		if(animation_preview_) {
			animation_preview_->draw();
		}

	}

	bool AnimationCreatorDialog::handleEvent(const SDL_Event& event, bool claimed)
	{
		if(animation_preview_) {
			claimed = animation_preview_->processEvent(getPos(), event, claimed) || claimed;
			if(claimed) {
				return claimed;
			}
		}
		return Dialog::handleEvent(event, claimed);
	}

	void AnimationCreatorDialog::setAnimationRect(rect r)
	{
		if(current_.is_null() == false) {
			current_.add_attr(variant("rect"), r.write());
			changed_ = true;
		}
		init();
	}

	void AnimationCreatorDialog::moveSolidRect(int dx, int dy)
	{
		if(current_.is_null() == false) {
			variant solid_area = current_["solid_area"];
			if(!solid_area.is_list() || solid_area.num_elements() != 4) {
				return;
			}

			for(const variant& num : solid_area.as_list()) {
				if(!num.is_int()) {
					return;
				}
			}

			rect area(solid_area);
			area = rect(area.x() + dx, area.y() + dy, area.w(), area.h());
			current_.add_attr(variant("solid_area"), area.write());
			changed_ = true;
		}
	}

	void AnimationCreatorDialog::setIntegerAttr(const char* attr, int value)
	{
		changed_ = true;

		slider_offset_[attr] = value;
		if(current_.is_null() == false) {
			current_.add_attr(variant(attr), variant(value));
		}
		init();
	}

	void AnimationCreatorDialog::onIdChange(DropdownWidgetPtr editor, const std::string& s)
	{
		if(current_.is_null() == false) {
			current_.add_attr(variant("id"), variant(s));
			changed_ = true;
		}
	}

	void AnimationCreatorDialog::onIdSet(DropdownWidgetPtr editor, int selection, const std::string& s)
	{
		if(current_.is_null() == false) {
			current_.add_attr(variant("id"), variant(s));
			changed_ = true;
		}
		init();
	}

	void AnimationCreatorDialog::setImageFile()
	{
		auto wnd = KRE::WindowManager::getMainWindow();
		gui::filter_list f;
		f.push_back(gui::filter_pair("Image Files", ".*?\\.(png|jpg|gif|bmp|tif|tiff|tga|webp|xpm|xv|pcx)"));
		f.push_back(gui::filter_pair("All Files", ".*"));
		gui::FileChooserDialog open_dlg(
			static_cast<int>(wnd->width()*0.1f), 
			static_cast<int>(wnd->height()*0.1f), 
			static_cast<int>(wnd->width()*0.8f), 
			static_cast<int>(wnd->height()*0.8f),
			f);
		open_dlg.setBackgroundFrame("empty_window");
		open_dlg.setDrawBackgroundFn(draw_last_scene);
		open_dlg.showModal();

		if(open_dlg.cancelled() == false) {
			image_file_ = open_dlg.getFileName();
			int offs = image_file_.rfind("/");
			image_file_name_ = image_file_.substr(offs+1);
			if(current_.is_null() == false) {
				current_.add_attr(variant("image"), variant(image_file_));
				changed_ = true;
			}
			rel_path_ = sys::compute_relative_path(module::get_module_path("") + "images", copy_path_ + "/" + image_file_name_);
		}
		init();
	}

	void AnimationCreatorDialog::changeText(const std::string& s, TextEditorWidgetPtr editor, SliderPtr slide)
	{
		if(!dragging_slider_) {
			int i;
			std::istringstream(editor->text()) >> i;
			slide->setPosition(0.5);
			if(current_.is_null() == false) {
				current_.add_attr(variant(s), variant(i));
			}
			changed_ = true;
		}
	}

	void AnimationCreatorDialog::executeChangeText(const std::string& s, TextEditorWidgetPtr editor, SliderPtr Slider)
	{
		if(!dragging_slider_) {
			int i;
			std::istringstream(editor->text()) >> i;
			Slider->setPosition(0.5);
			setIntegerAttr(s.c_str(), i);
		}
	}

	void AnimationCreatorDialog::changeSlide(const std::string& s, TextEditorWidgetPtr editor, double d)
	{
		dragging_slider_ = true;
		std::ostringstream ss;
		int i = slider_transform(d) + slider_offset_[s];
		ss << i;
		editor->setText(ss.str());

		if(current_.is_null() == false) {
			current_.add_attr(variant(s), variant(i));
		}
		changed_ = true;
		//int soffs = slider_offset_[s];
		//setIntegerAttr(s.c_str(), i);
		//slider_offset_[s] = soffs;
	}

	void AnimationCreatorDialog::endSlide(const std::string& s, SliderPtr slide, TextEditorWidgetPtr editor, double d)
	{
		int i = slider_transform(d) + slider_offset_[s];
		setIntegerAttr(s.c_str(), i);
		slide->setPosition(0.5);
		dragging_slider_ = false;
		init();
	}

	void AnimationCreatorDialog::animDel()
	{
		checkAnimChanged();

		if(selected_frame_ >= 0 && size_t(selected_frame_) < anims_.size()) {
			anims_.erase(anims_.begin() + selected_frame_);
			selected_frame_ = -1;
		}
		resetCurrentObject();
		init();
	}

	void AnimationCreatorDialog::animNew()
	{
		checkAnimChanged();
		resetCurrentObject();
		init();
	}

	void AnimationCreatorDialog::resetCurrentObject()
	{
		get_default_properties().clear();
		current_ = variant(&get_default_properties());

		current_.add_attr(variant("image"), variant(image_file_));
		//image_file_name_.clear();
		//image_file_.clear();
		//rel_path_.clear();
		copy_path_ = module::get_module_path("") + "images";

		selected_frame_ = -1;

		// reset the Slider offsets.
		for(std::map<std::string, int>::iterator it = slider_offset_.begin(); it != slider_offset_.end(); it++) {
			it->second = 0;
		}

		if(animation_preview_) {
			animation_preview_.reset();
		}
	}


	void AnimationCreatorDialog::animSave(Dialog* d)
	{
		// Save the current animation parameters, overwriting current animation values.
		if(current_.is_null() == false) {
			if(selected_frame_ == -1) {
				// copy file
				sys::copy_file(image_file_, copy_path_ + "/" + image_file_name_);
				// important to fix up image path
				current_.add_attr(variant("image"), variant(rel_path_));

				// erase any properties still as defaults.
				std::map<variant, variant>::const_iterator vit = current_.as_map().begin();
				std::map<variant, variant>::const_iterator end = current_.as_map().end();
				while(vit != end) {
					std::map<variant, variant>::const_iterator dit = get_default_properties().find(vit->first);
					if(dit != get_default_properties().end()) {
						if(vit->second == dit->second) {
							current_.remove_attr((vit++)->first);
						} else {
							vit++;
						}
					} else {
						vit++;
					}
				}

				// add the animation to list
				anims_.push_back(current_);
			} else {
				anims_[selected_frame_] = current_;
			}
		}
		changed_ = false;
		resetCurrentObject();

		if(d) {
			d->close();
		} else {
			init();
		}
	}

	void AnimationCreatorDialog::checkAnimChanged()
	{
		if(changed_) {
			// Create message box with Save/Cancel options.
			Dialog d((width()-400)/2, (height()-300)/2, 400, 300);
			d.setBackgroundFrame("empty_window");
			d.setPadding(20);
		
			LabelPtr title = new Label("Animation has changed.", KRE::Color::colorWhite(), 24);
			d.addWidget(title, (d.width()-title->width())/2, 50);
			GridPtr g = new Grid(2);
			g->setHeaderRow(40);
			g->addCol(WidgetPtr(new Button("Save", std::bind(&AnimationCreatorDialog::animSave, this, &d))))
				.addCol(WidgetPtr(new Button("Discard", std::bind(&Dialog::cancel, &d))));
			d.addWidget(g, (d.width()-g->width())/2, 30+70+title->height());
			d.showModal();
			changed_ = false;
			init();
		}
	}

	void AnimationCreatorDialog::selectAnimation(int index)
	{
		// index 0 is our label row.
		if(index < 1 || size_t(index) > anims_.size()) {
			return;
		}
		checkAnimChanged();

		selected_frame_ = index - 1;
		current_ = anims_[selected_frame_];
		variant_map_merge(current_, variant(&get_default_properties()));
		init();
	}

	void AnimationCreatorDialog::setDestination()
	{
		auto wnd = KRE::WindowManager::getMainWindow();
		FileChooserDialog dir_dlg(
			static_cast<int>(wnd->width()*0.2f), 
			static_cast<int>(wnd->height()*0.2f), 
			static_cast<int>(wnd->width()*0.6f), 
			static_cast<int>(wnd->height()*0.6f),
			gui::filter_list(), 
			true, module::get_module_path("") + "images");
		dir_dlg.setBackgroundFrame("empty_window");
		dir_dlg.setDrawBackgroundFn(draw_last_scene);
		dir_dlg.useRelativePaths(true);
		dir_dlg.showModal();

		if(dir_dlg.cancelled() == false) {
			copy_path_ = dir_dlg.getPath();
			rel_path_ = sys::compute_relative_path(module::get_module_path("") + "images", copy_path_ + "/" + image_file_name_);
		}
		init();
	}

	void AnimationCreatorDialog::finish()
	{
		checkAnimChanged();
		close();
	}

	void AnimationCreatorDialog::setOption()
	{
		simple_options_ = !simple_options_;
		init();
	}

	bool AnimationCreatorDialog::showAttribute(variant v)
	{
		if(!simple_options_) {
			return true;
		}
		std::string s = v.as_string();
		if(s == "frames" || s == "frames_per_row" || s == "duration" || s == "pad") {
			return true;
		}

		return false;
	}
}

#endif // NO_EDITOR
