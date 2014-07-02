
#include "button.hpp"
#include "language_dialog.hpp"
#include "dialog.hpp"
#include "draw_scene.hpp"
#include "graphical_font_label.hpp"
#include "i18n.hpp"
#include "level.hpp"
#include "preferences.hpp"
#include "json_parser.hpp"
#include "foreach.hpp"

namespace {
void end_dialog(gui::dialog* d)
{

	d->close();
}

void doDraw_scene() {
	draw_scene(level::current(), last_draw_position());
}

void setLocale(const std::string& value) {
	preferences::setLocale(value);
	i18n::init();
	GraphicalFont::initForLocale(i18n::get_locale());
}

class grid {
	gui::dialog& dialog_;
	int cell_width_;
	int cell_height_;
	int h_padding_;
	int v_padding_;
	int start_x_;
	int start_y_;
	int column_count_;
	int widget_count_;

	public:
	grid(gui::dialog& dialog, int cell_width, int cell_height, int h_padding, int v_padding, int start_x, int start_y, int column_count) :
   dialog_(dialog), cell_width_(cell_width), cell_height_(cell_height), h_padding_(h_padding), v_padding_(v_padding), start_x_(start_x), start_y_(start_y), column_count_(column_count), widget_count_(0) {
	}

	void addWidget(gui::WidgetPtr widget) {
		dialog_.addWidget(widget,
			start_x_ + h_padding_ + (widget_count_ % column_count_) * (cell_width_ + h_padding_),
			start_y_ + v_padding_ + (widget_count_ / column_count_) * (cell_height_ + v_padding_));
		widget_count_++;
	}

	int total_width() {
		return start_x_ + column_count_ * (cell_width_ + h_padding_);
	}

	int total_height() {
		return start_y_ + (widget_count_ + column_count_ - 1) / column_count_ * (cell_height_ + v_padding_);
	}
};
}

void show_language_dialog()
{
	using namespace gui;
	dialog d(0, 0, 0, 0);
	d.set_background_frame("empty_window");
	d.set_draw_background_fn(doDraw_scene);

	const int button_width = 300;
	const int button_height = 50;
	const int padding = 20;

	d.addWidget(WidgetPtr(new GraphicalFontLabel(_("Language change will take effect in next level."), "door_label", 2)), padding, padding);

	grid g(d, button_width, button_height, padding, padding, 0, 40, 2);

	typedef std::map<variant, variant> variant_map;
	variant_map languages = json::parse_from_file("data/languages.cfg").as_map();
	int index = 0;
	foreach(variant_map::value_type pair, languages) {
		WidgetPtr b(new button(
			WidgetPtr(new GraphicalFontLabel(pair.second.as_string(), "language_names", 2)),
			std::bind(setLocale, pair.first.as_string()),
			BUTTON_STYLE_NORMAL, BUTTON_SIZE_DOUBLE_RESOLUTION));
		b->setDim(button_width, button_height);
		g.addWidget(b);
	}

	WidgetPtr system_button(new button(
		WidgetPtr(new GraphicalFontLabel(_("Use system language"), "door_label", 2)),
	   	std::bind(setLocale, "system"),
		BUTTON_STYLE_NORMAL, BUTTON_SIZE_DOUBLE_RESOLUTION));
	system_button->setDim(button_width, button_height);
	g.addWidget(system_button);

	WidgetPtr back_button(new button(WidgetPtr(new GraphicalFontLabel(_("Back"), "door_label", 2)), std::bind(end_dialog, &d), BUTTON_STYLE_DEFAULT, BUTTON_SIZE_DOUBLE_RESOLUTION));
	back_button->setDim(button_width, button_height);
	g.addWidget(back_button);

        int dialog_width = g.total_width() + padding;
        int dialog_height = g.total_height() + padding;
        d.setLoc((preferences::virtual_screen_width() - dialog_width) / 2,
		(preferences::virtual_screen_height() - dialog_height) / 2);
	d.setDim(g.total_width() + padding, g.total_height() + padding);

	d.show_modal();
}
