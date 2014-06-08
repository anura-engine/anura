#include <algorithm>
#include "asserts.hpp"
#include "formula_callable_visitor.hpp"
#include "layout_widget.hpp"
#include "raster.hpp"
#include "widget_factory.hpp"

namespace gui
{
	layout_widget::layout_widget(const variant& v, game_logic::FormulaCallable* e)
		: widget(v,e), fixed_width_(0), fixed_height_(0), layout_type_(ABSOLUTE_LAYOUT)
	{
		if(v.has_key("style")) {
			const std::string style = v["style"].as_string();
			if(style == "absolute") {
				layout_type_ = ABSOLUTE_LAYOUT;
			} else if(style == "relative") {
				layout_type_ = RELATIVE_LAYOUT;
			} else {
				ASSERT_LOG(false, "Unrecognised layout style: " << style);
			}
		}

		ASSERT_LOG(v.has_key("children"), "layout widget must have a 'children' attribute.");
		ASSERT_LOG(v["children"].is_list(), "layout widget must have 'children' attribute that is a list.");
		const variant& children = v["children"];
		for(size_t n = 0; n != children.num_elements(); ++n) {
			children_.insert(widget_factory::create(children[n], e));
		}
		if(width()) {
			fixed_width_ = width();
		}
		if(height()) {
			fixed_height_ = height();
		}

		reflow_children();
	}

	layout_widget::~layout_widget()
	{
	}

	void layout_widget::reflow_children()
	{
		int lx = 0;
		int ly = 0;
		int lw = 0;
		int lh = 0;
		if(layout_type_ == RELATIVE_LAYOUT) {
			for(auto w : children_) {
				ASSERT_LOG(w->width() < fixed_width_, "width of child widget is greater than width of layout widget");
				if(lx + w->width() > fixed_width_) {
					ly += lh;
					lh = 0;
				}
				lh = std::max(lh, w->height());
				w->setLoc(lx, ly);
				lx += w->width();
				lw = std::max(lw, lx);
			}
		} else if(layout_type_ == ABSOLUTE_LAYOUT) {
			// do nothing
			for(auto w : children_) {
				lw = std::max(lw, w->width());
				lh = std::max(lh, w->height());
			}
		} else {
			ASSERT_LOG(false, "Incorrect layout style");
		}
		if(fixed_height_ == 0 && fixed_width_ == 0) {
			setDim(lw, lh);
		}
	}

	void layout_widget::recalcLoc()
	{
		widget::recalcLoc();
		if(width()) {
			fixed_width_ = width();
		}
		if(height()) {
			fixed_height_ = height();
		}
	}

	void layout_widget::handleDraw() const
	{
		glPushMatrix();
		glTranslatef(GLfloat(x() & ~1), GLfloat(y() & ~1), 0.0);
		for(auto w : children_) {
			w->draw();
		}
		glPopMatrix();
	}

	bool layout_widget::handleEvent(const SDL_Event& event, bool claimed)
	{
		for(auto w : children_) {
			claimed = w->processEvent(event, claimed);
			if(claimed) {
				return claimed;
			}
		}
		return claimed;
	}

	std::vector<WidgetPtr> layout_widget::getChildren() const
	{

		std::vector<WidgetPtr> v;
		for(auto w : children_) {
			v.push_back(w);
		}
		return v;
	}

	void layout_widget::visitValues(game_logic::FormulaCallableVisitor& visitor)
	{
		for(auto w : children_) {
			visitor.visit(&w);
		}
	}

	variant layout_widget::handleWrite()
	{
		variant_builder res;
		res.add("type", "layout");
		switch(layout_type_) {
			case ABSOLUTE_LAYOUT: res.add("style", "absolute"); break;
			case RELATIVE_LAYOUT: res.add("style", "relative"); break;
			default:
				ASSERT_LOG(false, "Incorrect layout style");
		}
		for(auto w : children_) {
			res.add("children", w->write());
		}
		return res.build();
	}

	BEGIN_DEFINE_CALLABLE(layout_widget, widget)
		DEFINE_FIELD(dummy, "int")
			return variant();
	END_DEFINE_CALLABLE(layout_widget)
}
