#pragma once

#include <set>
#include "widget.hpp"

namespace gui 
{
	class layout_widget : public widget
	{
	public:
		enum LayoutType {
			ABSOLUTE_LAYOUT,
			RELATIVE_LAYOUT,
		};

		layout_widget(const variant& v, game_logic::formula_callable* e);
		virtual ~layout_widget();

		std::vector<widget_ptr> get_children() const;

		void reflow_children();
	protected:
		variant handle_write();
		void recalc_loc();
		void handle_draw() const;
		bool handle_event(const SDL_Event& event, bool claimed);
	private:
		DECLARE_CALLABLE(layout_widget);

		void visit_values(game_logic::formula_callable_visitor& visitor);

		LayoutType layout_type_;

		// If width is specified then we keep a track of it here.
		int fixed_width_;
		// If height is specified then we keep a track of it here.
		int fixed_height_;

		typedef std::set<widget_ptr, widget_sort_zorder> widget_list;
		widget_list children_;
	};
	typedef boost::intrusive_ptr<layout_widget> layout_widget_ptr;
}
