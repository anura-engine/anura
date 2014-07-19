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

#ifndef NO_EDITOR

#include <boost/algorithm/string/replace.hpp>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <climits>

#include <math.h>

#include "kre/Font.hpp"

#include "animation_creator.hpp"
#include "animation_widget.hpp"
#include "asserts.hpp"
#include "button.hpp"
#include "code_editor_widget.hpp"
#include "custom_object_dialog.hpp"
#include "draw_scene.hpp"
#include "dropdown_widget.hpp"
#include "file_chooser_dialog.hpp"
#include "frame.hpp"
#include "grid_widget.hpp"
#include "input.hpp"
#include "json_parser.hpp"
#include "label.hpp"
#include "module.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"
#include "tree_view_widget.hpp"

const std::string template_directory = "data/object_templates/";

namespace gui 
{
	class ItemEditDialog : public Dialog
	{
	public:
		ItemEditDialog(int x, int y, int w, int h, const std::string&name, variant items) 
			: Dialog(x,y,w,h), items_(items), display_name_(name), allow_functions_(false),
			row_count_(0)
		{
			if(items_.is_map() == false) {
				std::map<variant, variant> m;
				items_ = variant(&m);
			}
			init();
		}
		virtual ~ItemEditDialog() {}
		variant getItems() const { return item_grid_->getTree(); }
		void allowFunctions(bool val=true) { allow_functions_ = val; }
	protected:
		virtual bool handleEvent(const SDL_Event& event, bool claimed) override;

		void init();
		void onSave();
		bool hasKeyboardFocus();

		void editorSelect(variant* v, std::function<void(const variant&)> save_fn);
		void stringEntrySave();
		void stringEntryDiscard();
	private:
		std::string display_name_;
		variant items_;
		bool allow_functions_;
		WidgetPtr context_menu_;

		tree_editor_WidgetPtr item_grid_;
		code_editor_WidgetPtr string_entry_;
		ButtonPtr save_text_button_;
		ButtonPtr discard_text_button_;
		grid_ptr text_button_grid;
		std::function<void(const variant&)> save_fn_;
		int row_count_;
		std::string saved_text_;
	};
}

namespace 
{
	int slider_transform(float d)
	{
		// normalize to [-20.0,20.0] range.
		d = (d - 0.5f) * 2.0f * 20.0f;
		float d_abs = abs(d);
		if(d_abs > 10) {
			// Above 10 units we go non-linear.
			return static_cast<int>((d < 0 ? -1.0f : 1.0f) * pow(10.0f, d_abs/10.0f));
		}
		return static_cast<int>(d);
	}

	module::module_file_map& get_template_path()
	{
		static module::module_file_map dialog_file_map;
		return dialog_file_map;
	}

	void load_template_file_paths(const std::string& path)
	{
		if(get_template_path().empty()) {
			module::get_unique_filenames_under_dir(path, &get_template_path());
		}
	}


	void load_default_attributes(std::vector<std::string>& d)
	{
		//d.push_back("id");
		//d.push_back("animation");
		//d.push_back("editor_info");
		d.push_back("prototype");
		d.push_back("hitpoints");
		d.push_back("mass");
		d.push_back("vars");
		d.push_back("friction");
		d.push_back("traction");
		d.push_back("traction_in_air");
	}

	std::vector<std::string>& get_default_attribute_list()
	{
		static std::vector<std::string> defaults;
		if(defaults.empty()) {
			load_default_attributes(defaults);
		}
		return defaults;
	}

	void reset_dialog_paths()
	{
		get_template_path().clear();
	}

	std::string get_dialog_file(const std::string& fname)
	{
		load_template_file_paths(template_directory);
		module::module_file_map::const_iterator it = module::find(get_template_path(), fname);
		ASSERT_LOG(it != get_template_path().end(), "OBJECT TEMPLATE FILE NOT FOUND: " << fname);
		return it->second;
	}


	std::string get_id_from_filemap(std::pair<std::string, std::string> p)
	{
		std::string s = module::get_id(p.first);
		if(s.length() > 4 && s.substr(s.length()-4) == ".cfg") {
			return s.substr(0, s.length()-4);
		}
		return s;
	}
}

namespace editor_dialogs 
{
	using std::placeholders::_1;
	using std::placeholders::_2;

	CustomObjectDialog::CustomObjectDialog(editor& e, int x, int y, int w, int h)
		: gui::Dialog(x,y,w,h), dragging_slider_(false), selected_template_(0)
	{
		load_template_file_paths(template_directory);
		setClearBgAmount(255);
		std::map<variant, variant> m;
		object_template_ = variant(&m);
		current_object_save_path_ = module::get_module_path() + "data/objects/";
		std::transform(prototype_file_paths().begin(), prototype_file_paths().end(), std::back_inserter(prototypes_), get_id_from_filemap);
		std::sort(prototypes_.begin(), prototypes_.end());
		init();
	}


	void CustomObjectDialog::init()
	{
		const int border_offset = 30;
		using namespace gui;
		clear();

		addWidget(WidgetPtr(new Label("Object Properties", KRE::Color::colorWhite(), 20)), border_offset, border_offset);

		grid_ptr container(new grid(1));
		container->set_col_width(0, width() - border_offset);

		// Get choices for dropdown list.
		std::vector<std::string> template_choices;
		std::transform(get_template_path().begin(), get_template_path().end(), std::back_inserter(template_choices),
			std::bind(&module::module_file_map::value_type::first,_1));
		std::sort(template_choices.begin(), template_choices.end());
		template_choices.insert(template_choices.begin(), "Blank");

		dropdown_WidgetPtr template_dropdown(new dropdown_widget(template_choices, 200, 30, dropdown_widget::DROPDOWN_LIST));
		template_dropdown->set_dropdown_height(100);
		template_dropdown->setOnSelectHandler(std::bind(&CustomObjectDialog::changeTemplate, this, _1, _2));
		template_dropdown->setSelection(selected_template_);

		grid_ptr g(new grid(4));
		g->setHpad(20);
		g->setZOrder(1);
		g->add_col(WidgetPtr(new Label("Template  ", KRE::Color::colorWhite(), 14)))
			.add_col(template_dropdown);
		TextEditorWidgetPtr change_entry(new TextEditorWidget(200, 28));
		change_entry->setFontSize(14);
		if(object_template_.has_key("id")) {
			change_entry->setText(object_template_["id"].as_string());
		}
		change_entry->setOnChangeHandler(std::bind(&CustomObjectDialog::changeTextAttribute, this, change_entry, "id"));
		change_entry->setOnEnterHandler(std::bind(&CustomObjectDialog::init, this));
		change_entry->setOnTabHandler(std::bind(&CustomObjectDialog::init, this));
		change_entry->setOnEscHandler(std::bind(&CustomObjectDialog::init, this));
		change_entry->setOnChangeFocusHandler(std::bind(&CustomObjectDialog::idChangeFocus, this, _1));
		g->add_col(WidgetPtr(new Label("id: ", KRE::Color::colorWhite(), 14)))
			.add_col(WidgetPtr(change_entry));
		container->add_col(g);

		g.reset(new grid(4));
		g->add_col(WidgetPtr(new Button(new Label("Animations", KRE::Color::colorWhite(), 20), std::bind(&CustomObjectDialog::onEditAnimations, this))));
		g->add_col(WidgetPtr(new Button(new Label("Variables", KRE::Color::colorWhite(), 20), std::bind(&CustomObjectDialog::onEditItems, this, "Variables Editor", "vars", false))));
		g->add_col(WidgetPtr(new Button(new Label("Properties", KRE::Color::colorWhite(), 20), std::bind(&CustomObjectDialog::onEditItems, this, "Properties Editor", "properties", true))));
		g->add_col(WidgetPtr(new Button(new Label("Editor Info", KRE::Color::colorWhite(), 20), std::bind(&CustomObjectDialog::onEditItems, this, "Editor Info", "editor_info", false))));
		container->add_col(g);

		if(template_file_.first.empty()) {
			for(const std::string& attr : get_default_attribute_list()) {
				std::vector<WidgetPtr> widget_list = getWidgetForAttribute(attr);
				for(const WidgetPtr& w : widget_list) {
					if(w) {
						container->add_col(w);
					}
				}
			}
		} else {
			std::vector<variant> keys = object_template_.getKeys().as_list();
			for(const variant& v : keys) {
				std::vector<WidgetPtr> widget_list = getWidgetForAttribute(v.as_string());
				for(const WidgetPtr& w : widget_list) {
					if(w) {
						container->add_col(w);
					}
				}
			}
		}

		error_text_.clear();
		assert_recover_scope recover_from_assert;
		try {
			object_ = CustomObjectTypePtr(new CustomObjectType(object_template_["id"].as_string(), object_template_, NULL, NULL));

			AnimationWidgetPtr preview(new AnimationWidget(128, 128, object_template_));
			addWidget(preview, width() - border_offset - 128, border_offset + 200);
		} catch(validation_failure_exception& e) {
			error_text_ = e.msg;
			std::cerr << "error parsing formula: " << e.msg << std::endl;
		} catch(type_error& e) {
			error_text_ = e.message;
			std::cerr << "error executing formula: " << e.message << std::endl;
		}

		std::string err_text = error_text_;
		boost::replace_all(err_text, "\n", "\\n");
		int max_chars = (width() - border_offset*2)/KRE::Font::charWidth(14);
		if(err_text.length() > static_cast<unsigned>(max_chars) && max_chars > 3) {
			err_text = err_text.substr(0, max_chars-3) + "...";
		}
		LabelPtr error_text(new Label(err_text, KRE::Color::colorRed(), 14));
		addWidget(error_text, border_offset, height() - g->height() - border_offset - error_text->height() - 5);

		g.reset(new grid(3));
		g->setHpad(20);
		g->add_col(ButtonPtr(new Button(new Label("Create", KRE::Color::colorWhite(), 20), std::bind(&CustomObjectDialog::onCreate, this))));
		g->add_col(ButtonPtr(new Button(new Label("Set Path...", KRE::Color::colorWhite(), 20), std::bind(&CustomObjectDialog::onSetPath, this))));
		std::string path = current_object_save_path_;
		if(object_template_.has_key("id")) {
			path += object_template_["id"].as_string() + ".cfg";
		} else {
			path += "<no id>.cfg";
		}
		g->add_col(LabelPtr(new Label(path, KRE::Color::colorGreen())));
		addWidget(g, border_offset, height() - g->height() - border_offset);

		container->set_max_height(height() - g->height() - border_offset - error_text->height() - 10);
		addWidget(container, border_offset, border_offset*2);
	}

	void CustomObjectDialog::onSetPath()
	{
		gui::FileChooserDialog dir_dlg(
			static_cast<int>(preferences::virtual_screen_width()*0.2), 
			static_cast<int>(preferences::virtual_screen_height()*0.2), 
			static_cast<int>(preferences::virtual_screen_width()*0.6), 
			static_cast<int>(preferences::virtual_screen_height()*0.6),
			gui::filter_list(), 
			true, current_object_save_path_);
		dir_dlg.setBackgroundFrame("empty_window");
		dir_dlg.setDrawBackgroundFn(draw_last_scene);
		dir_dlg.use_relative_paths(true);
		dir_dlg.showModal();

		if(dir_dlg.cancelled() == false) {
			current_object_save_path_ = dir_dlg.getPath() + "/";
		}
		init();
	}

	void CustomObjectDialog::idChangeFocus(bool focus)
	{
		if(focus == false) {
			init();
		}
	}

	std::vector<gui::WidgetPtr> CustomObjectDialog::getWidgetForAttribute(const std::string& attr)
	{
		using namespace gui;
		if(attr == "id") {
			//grid_ptr g(new grid(2));
			//TextEditorWidgetPtr change_entry(new TextEditorWidget(200, 28));
			//change_entry->setFontSize(14);
			//if(object_template_.has_key(attr)) {
			//	change_entry->setText(object_template_[attr].as_string());
			//}
			//change_entry->setOnChangeHandler(std::bind(&custom_object_dialog::change_text_attribute, this, change_entry, attr));
			//change_entry->setOnEnterHandler([](){});
			//g->add_col(WidgetPtr(new label(attr + ": ", KRE::Color::colorWhite(), 14))).add_col(WidgetPtr(change_entry));
			//return g;
		} else if(attr == "hitpoints" || attr == "mass" || attr == "friction" 
			|| attr == "traction" || attr == "traction_in_air") {
			grid_ptr g(new grid(3));
			int value = 0;
			TextEditorWidgetPtr change_entry(new TextEditorWidget(100, 28));
			change_entry->setFontSize(14);
			if(object_template_.has_key(attr)) {
				std::stringstream ss;
				value = object_template_[attr].as_int();
				ss << object_template_[attr].as_int();
				change_entry->setText(ss.str());
			} else {
				change_entry->setText("0");
			}
			slider_offset_[attr] = object_template_.has_key(attr) ? object_template_[attr].as_int() : 0;

			SliderPtr slide(new Slider(200, 
				std::bind((&CustomObjectDialog::changeIntAttributeSlider), this, change_entry, attr, _1), 
				static_cast<float>(value)));
			slide->setPosition(0.5f);
			slide->setDragEnd(std::bind(&CustomObjectDialog::sliderDragEnd, this, change_entry, attr, slide, _1));
			change_entry->setOnChangeHandler(std::bind(&CustomObjectDialog::changeIntAttributeText, this, change_entry, attr, slide));
			change_entry->setOnEnterHandler([](){});
			LabelPtr attr_label(new Label(attr + ": ", KRE::Color::colorWhite(), 14));
			attr_label->setDim(200, attr_label->height());
			change_entry->setDim(100, change_entry->height());
			slide->setDim(200, slide->height());
			g->add_col(attr_label).add_col(WidgetPtr(change_entry)).add_col(slide);

			g->set_col_width(0, 200);
			g->set_col_width(1, 100);
			g->set_col_width(2, 200);

			return std::vector<gui::WidgetPtr>(1, g);
		} else if(attr == "animation") {
			//ButtonPtr bb(new button(new label("Edit Animations", KRE::Color::colorWhite(), 20), std::bind(&custom_object_dialog::on_edit_animations, this)));
			//return bb;
		} else if(attr == "vars") {
			//grid_ptr g(new grid(1));
			//g->add_col(WidgetPtr(new label(attr + ": ", KRE::Color::colorWhite(), 14)));
			//return std::vector<gui::WidgetPtr>(1, g);
		} else if(attr == "editor_info") {
			//grid_ptr g(new grid(1));
			//g->add_col(WidgetPtr(new label(attr + ": ", KRE::Color::colorWhite(), 14)));
			//return std::vector<gui::WidgetPtr>(1, g);
		} else if(attr == "prototype") {
			//int count = 0;
			// To make this nicer. Create the buttons before adding them to the grid.
			// Estimate the maximum number of columns needed (take the minimum size button 
			// divided into screen width being used.  Then we start adding buttons to the 
			// grid, if we are about to add a button that would go over the maximum
			// width then we do a .finish_row() (if needed) and start continue
			// adding the column to the next row (with .add_col()).
			std::vector<ButtonPtr> buttons;
			int min_size_button = std::numeric_limits<int>::max();
			if(object_template_.has_key("prototype")) {
				for(const std::string& s : object_template_["prototype"].as_list_string()) {
					buttons.push_back(new Button(WidgetPtr(new Label(s, KRE::Color::colorWhite())), 
						std::bind(&CustomObjectDialog::removePrototype, this, s)));
					if(min_size_button > buttons.back()->width()) {
						min_size_button = buttons.back()->width();
					}
				}
			}
			std::vector<gui::WidgetPtr> rows;
			// conservative
			int column_estimate = (width() - 100) / min_size_button + 2;
			grid_ptr g(new grid(column_estimate));
			LabelPtr attr_label  = new Label(attr + ": ", KRE::Color::colorWhite(), 14);
			ButtonPtr add_button = new Button(WidgetPtr(new Label("Add...", KRE::Color::colorWhite())), 
				std::bind(&CustomObjectDialog::changePrototype, this));
			g->add_col(attr_label).add_col(add_button);

			int current_row_size = attr_label->width() + add_button->width();
			int buttons_on_current_row = 2;
			for(const ButtonPtr& b : buttons) {
				if(b->width() + current_row_size >= width()-100 ) {
					if(buttons_on_current_row < column_estimate) {
						g->finish_row();
					}
					rows.push_back(g);
					g.reset(new grid(column_estimate));
				
					current_row_size = 0;
					buttons_on_current_row = 0;
				}
				g->add_col(b);
				current_row_size += b->width();
				buttons_on_current_row++;
			}
			if(buttons_on_current_row != 0) {
				if(buttons_on_current_row < column_estimate) {
					g->finish_row();
				}
				rows.push_back(g);
			}
			return rows;
		}
		std::cerr << "Unhandled attribute " << attr << std::endl;
		return std::vector<gui::WidgetPtr>();
	}

	void CustomObjectDialog::sliderDragEnd(const gui::TextEditorWidgetPtr editor, const std::string& s, gui::SliderPtr slide, float d)
	{
		int i = slider_transform(d) + slider_offset_[s];
		slider_offset_[s] = i;
		slide->setPosition(0.5f);
		dragging_slider_ = false;
	}

	void CustomObjectDialog::changeIntAttributeSlider(const gui::TextEditorWidgetPtr editor, const std::string& s, float d)
	{	
		dragging_slider_ = true;
		std::ostringstream ss;
		int i = slider_transform(d) + slider_offset_[s];
		ss << i;
		editor->setText(ss.str(), false);
		object_template_.add_attr(variant(s), variant(i));
	}

	void CustomObjectDialog::changeTextAttribute(const gui::TextEditorWidgetPtr editor, const std::string& s)
	{
		object_template_.add_attr(variant(s), variant(editor->text()));
	}

	void CustomObjectDialog::changeIntAttributeText(const gui::TextEditorWidgetPtr editor, const std::string& s, gui::SliderPtr slide)
	{
		if(!dragging_slider_) {
			int i;
			std::istringstream(editor->text()) >> i;
			slider_offset_[s] = i;
			slide->setPosition(0.5);
			object_template_.add_attr(variant(s), variant(i));
		}
	}

	void CustomObjectDialog::changeTemplate(int selection, const std::string& s)
	{
		selected_template_ = selection;
		if(selection == 0) {
			template_file_ = std::pair<std::string, std::string>();
		} else {
			template_file_.first = get_id_from_filemap(std::pair<std::string, std::string>(s,""));
			template_file_.second = get_dialog_file(s);
		}
		if(template_file_.first.empty() == false) {
			object_template_ = json::parse_from_file(template_file_.second);
			ASSERT_LOG(object_template_.is_map(), 
				"OBJECT TEMPLATE READ FROM FILE IS NOT MAP: " << template_file_.second)
			// ignorning these exceptions till we're finished
			//assert_recover_scope recover_from_assert;
			//try {
			//	object_ = CustomObjectTypePtr(new CustomObjectType(object_template_, NULL, NULL));
			//} catch(validation_failure_exception& e) {
			//	std::cerr << "error parsing formula: " << e.msg << std::endl;
			//} catch(type_error& e) {
			//	std::cerr << "error executing formula: " << e.message << std::endl;
			//}
		} else {
			std::map<variant, variant> m;
			object_template_ = variant(&m);
		}
		init();
	}

	void CustomObjectDialog::changePrototype()
	{
		using namespace gui;
		std::vector<std::string> choices;
		if(object_template_.has_key("prototype")) {
			std::vector<std::string> v = object_template_["prototype"].as_list_string();
			std::sort(v.begin(), v.end());
			std::set_difference(prototypes_.begin(), prototypes_.end(), v.begin(), v.end(), 
				std::inserter(choices, choices.end()));
		} else {
			choices = prototypes_;
		}

		int mousex, mousey, my;
		input::sdl_get_mouse_state(&mousex, &my);
		mousex -= this->x();
		mousey = my - this->y();

		gui::grid* grid = new gui::grid(1);
		grid->set_max_height(height() - my);
		grid->setHpad(10);
		grid->setShowBackground(true);
		grid->allowSelection();
		grid->swallowClicks();
		for(const std::string& s : choices) {
			grid->add_col(WidgetPtr(new Label(s, KRE::Color::colorWhite())));
		}
		grid->registerSelectionCallback(std::bind(&CustomObjectDialog::executeChangePrototype, this, choices, _1));

		removeWidget(context_menu_);
		context_menu_.reset(grid);
		addWidget(context_menu_, mousex, mousey);
	}

	void CustomObjectDialog::removePrototype(const std::string& s)
	{
		if(object_template_.has_key("prototype")) {
			std::vector<variant> v = object_template_["prototype"].as_list();
			v.erase(std::remove(v.begin(), v.end(), variant(s)), v.end());
			object_template_.add_attr(variant("prototype"), variant(&v));
		}
		init();
	}

	void CustomObjectDialog::executeChangePrototype(const std::vector<std::string>& choices, size_t index)
	{
		if(context_menu_) {
			removeWidget(context_menu_);
			context_menu_.reset();
		}
		if(index >= choices.size()) {
			return;
		}

		std::vector<variant> v;
		if(object_template_.has_key("prototype")) {
			v = object_template_["prototype"].as_list();
		}
		v.push_back(variant(choices[index]));
		object_template_.add_attr(variant("prototype"), variant(&v));

		init();
	}


	void CustomObjectDialog::onCreate()
	{
		// write out the file
		sys::write_file(current_object_save_path_ + "/" + object_template_[variant("id")].as_string() + ".cfg", object_template_.write_json());
		close();
	}

	void CustomObjectDialog::onEditAnimations()
	{
		gui::AnimationCreatorDialog d(0, 0, preferences::virtual_screen_width(), 
			preferences::virtual_screen_height(),
			object_template_.has_key("animation") ? object_template_["animation"] : variant());
		d.setBackgroundFrame("empty_window");
		d.setDrawBackgroundFn(draw_last_scene);

		d.showModal();
		if(d.cancelled() == false) {
			object_template_.add_attr(variant("animation"), d.getAnimations());
		}
	}

	void CustomObjectDialog::onEditItems(const std::string& name, const std::string& attr, bool allow_functions)
	{
		gui::ItemEditDialog d(0, 0, preferences::virtual_screen_width(), 
			preferences::virtual_screen_height(),
			name,
			object_template_.has_key(attr) ? object_template_[attr] : variant());
		d.setBackgroundFrame("empty_window");
		d.setDrawBackgroundFn(draw_last_scene);
		d.allowFunctions(allow_functions);

		d.showModal();
		if(d.cancelled() == false) {
			object_template_.add_attr(variant(attr), d.getItems());
		}
	}

	void CustomObjectDialog::showModal()
	{
		gui::filter_list f;
		f.push_back(gui::filter_pair("Image Files", ".*?\\.(png|jpg|gif|bmp|tif|tiff|tga|webp|xpm|xv|pcx)"));
		f.push_back(gui::filter_pair("All Files", ".*"));
		gui::file_chooser_dialog open_dlg(
			int(preferences::virtual_screen_width()*0.1), 
			int(preferences::virtual_screen_height()*0.1), 
			int(preferences::virtual_screen_width()*0.8), 
			int(preferences::virtual_screen_height()*0.8),
			f, false, module::map_file("images/"));
		open_dlg.setBackgroundFrame("empty_window");
		open_dlg.setDrawBackgroundFn(draw_last_scene);
		open_dlg.showModal();

		if(open_dlg.cancelled() == false) {
			image_file_ = open_dlg.get_file_name();
			int offs = image_file_.rfind("/");
			image_file_name_ = image_file_.substr(offs+1);

			Dialog::showModal();
		} else {
			cancel();
		}
	}
}

namespace gui 
{
	using std::placeholders::_1;
	using std::placeholders::_2;

	void ItemEditDialog::init()
	{
		clear();

		const int border_offset = 35;
		const int hpad = 20;
		int current_height = border_offset;
		LabelPtr title(new Label(display_name_.empty() ? "Edit" : display_name_, KRE::Color::colorWhite(), 20));
		addWidget(title, border_offset, current_height);
		current_height += title->height() + hpad;

		grid_ptr g(new grid(2));
		g->setHpad(100);
		ButtonPtr mod_button(new Button(new Label("Save&Close", KRE::Color::colorWhite(), 16), std::bind(&ItemEditDialog::onSave, this)));
		ButtonPtr del_button(new Button(new Label("Cancel", KRE::Color::colorWhite(), 16), std::bind(&ItemEditDialog::cancel, this)));
		g->add_col(mod_button).add_col(del_button);
		addWidget(g, (width() - g->width())/2, current_height);
		current_height += g->height() + hpad;


		text_button_grid.reset(new grid(2));
		text_button_grid->setHpad(30);
		save_text_button_.reset(new Button(new Label("Save Text", KRE::Color::colorWhite(), 14), std::bind(&ItemEditDialog::stringEntrySave, this)));
		discard_text_button_.reset(new Button(new Label("Discard Text", KRE::Color::colorWhite(), 14), std::bind(&ItemEditDialog::stringEntryDiscard, this)));
		text_button_grid->add_col(save_text_button_).add_col(discard_text_button_);
		text_button_grid->setVisible(false);

		const int string_entry_height = height() - current_height - border_offset - text_button_grid->height() - 5;
		const int string_entry_width = 2*width()/3 - 2*border_offset;

		addWidget(text_button_grid, width()/3 + border_offset + (string_entry_width - text_button_grid->width())/2, string_entry_height + current_height + 5);

		string_entry_.reset(new code_editor_widget(string_entry_width, string_entry_height));
		string_entry_->setFontSize(12);
		string_entry_->setOnEscHandler(std::bind(&ItemEditDialog::stringEntryDiscard, this));
		string_entry_->setLoc(width()/3 + border_offset, current_height);
		if(allow_functions_) {
			string_entry_->set_formula();
		}

		item_grid_.reset(new TreeEditorWidget(width()/3 - border_offset, height() - current_height - border_offset, items_));
		item_grid_->allowSelection();
		item_grid_->allowPersistentHighlight();
		item_grid_->setEditorHandler(variant::VARIANT_TYPE_STRING, string_entry_, std::bind(&ItemEditDialog::editorSelect, this, _1, _2));
		addWidget(item_grid_, border_offset, current_height);

		current_height += item_grid_->height() + hpad;
	}

	void ItemEditDialog::editorSelect(variant* v, std::function<void(const variant&)> save_fn)
	{
		text_button_grid->setVisible(true);
		saved_text_ = v->as_string();
		string_entry_->setText(saved_text_);
		string_entry_->setFocus(true);
		save_fn_ = save_fn;
	}

	void ItemEditDialog::onSave()
	{
		close();
	}

	bool ItemEditDialog::hasKeyboardFocus()
	{
		return string_entry_->hasFocus();
	}

	bool ItemEditDialog::handleEvent(const SDL_Event& event, bool claimed)
	{
		if(Dialog::handleEvent(event, claimed)) {
			return true;
		}

		if(hasKeyboardFocus()) {
			if(event.type == SDL_KEYDOWN) {
				if(event.key.keysym.sym == SDLK_s && (event.key.keysym.mod&KMOD_CTRL)) {
					stringEntrySave();
					return true;
				}
			}
		}
		return claimed;
	}

	void ItemEditDialog::stringEntrySave()
	{
		if(save_fn_) {
			text_button_grid->setVisible(false);
			save_fn_(variant(string_entry_->text()));
		}
	}

	void ItemEditDialog::stringEntryDiscard()
	{
		if(save_fn_) {
			text_button_grid->setVisible(false);
			save_fn_(variant(saved_text_));
		}
	}
}

#endif // !NO_EDITOR
