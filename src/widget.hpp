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

#pragma once

#include <string>
#include <functional>

#include "SDL.h"

#include "geometry.hpp"
#include "Texture.hpp"
#include "Color.hpp"

#include "asserts.hpp"
#include "formula.hpp"
#include "tooltip.hpp"
#include "framed_gui_element.hpp"
#include "widget_fwd.hpp"
#include "variant_utils.hpp"

namespace game_logic 
{
	class FormulaCallableVisitor;
}

namespace gui 
{
	class Widget : public game_logic::FormulaCallable
	{
	public:
		enum HORIZONTAL_ALIGN {HALIGN_LEFT, HALIGN_CENTER, HALIGN_RIGHT};
		enum VERTICAL_ALIGN   {VALIGN_TOP,  VALIGN_CENTER, VALIGN_BOTTOM};

		bool processEvent(const point& p, const SDL_Event& event, bool claimed);
		void draw(int xt=0, int yt=0, float rotate=0, float scale=1.0f) const;

		virtual void setLoc(int x, int y) { true_x_ = x_ = x; true_y_ = y_ = y; recalcLoc(); }
		virtual void setDim(int w, int h) { w_ = w; h_ = h; recalcLoc(); }

		int x() const;
		int y() const;
		int width() const;
		int height() const;
		const rect* clipArea() const;
		void setClipArea(const rect& area);
		void setClipAreaToDim();
		void clearClipArea();
		void setTooltip(const std::string& str, int fontsize=18, const KRE::Color& color=KRE::Color::colorYellow(), const std::string& font="");
		void setTooltipText(const std::string& str);
		void setTooltipFontSize(int fontsize);
		void setTooltipColor(const KRE::Color& color);
		void setTooltipFont(const std::string& font);
		std::string tooltipText() const { return tooltipText_; }
		int tooltipFontSize() const { return tooltip_font_size_; }
		std::string tooltipFont() const { return tooltip_font_; }
		KRE::Color tooltipColor() const { return tooltip_color_; }
		bool visible() { return visible_; }
		void setVisible(bool visible) { visible_ = visible; }
		void setId(const std::string& new_id) { id_ = new_id; }
		const std::string& id() const { return id_; }
		bool disabled() const { return disabled_; }
		void enable(bool val=true) { disabled_ = val; }
		bool claimMouseEvents() const { return claim_mouse_events_; }
		void setClaimMouseEvents(bool claim=true) { claim_mouse_events_ = claim; }

		int disabledOpacity() const { return disabled_opacity_; }
		void setDisabledOpacity(int n) { disabled_opacity_ = std::min(255, std::max(n, 0)); }

		bool drawWithObjectShader() const { return draw_with_object_shader_; }
		void setDrawWithObjectShader(bool dwos=true) { draw_with_object_shader_ = dwos; }

		unsigned getTooltipDelay() const { return tooltip_display_delay_; }
		void setTooltipDelay(unsigned tooltip_delay) { tooltip_display_delay_ = tooltip_delay; }

		virtual WidgetPtr getWidgetById(const std::string& id);
		virtual ConstWidgetPtr getWidgetById(const std::string& id) const;

		virtual bool hasFocus() const { return has_focus_; }
		virtual void setFocus(bool f=true) { has_focus_ = f; }
		virtual void doExecute() {}

		game_logic::FormulaCallable* getEnvironment() const { return environ_; }

		void setZOrder(int z) { zorder_ = z; }
		int zorder() const { return zorder_; }

		int getFrameResolution() const { return resolution_; }
		void setFrameResolution(int r) { resolution_ = r; }
		void setFrameSet(const std::string& frame);
		std::string frameSetName() const { return frame_set_name_; }

		int getAlpha() const { return display_alpha_; }
		void setAlpha(int a=256) { display_alpha_ = a; }

		int getPadWidth() const { return pad_w_; }
		int getPadHeight() const { return pad_h_; }
		void setPadding(int pw, int ph) { pad_w_ = pw; pad_h_ = ph; }

		HORIZONTAL_ALIGN hAlign() const { return align_h_; }
		VERTICAL_ALIGN vAlign() const { return align_v_; }
		void setHAlign(HORIZONTAL_ALIGN h) { align_h_ = h; recalcLoc(); }
		void setVAlign(VERTICAL_ALIGN v) { align_v_ = v; recalcLoc(); }

		virtual std::vector<WidgetPtr> getChildren() const { return std::vector<WidgetPtr>(); }

		void process();

		void performVisitValues(game_logic::FormulaCallableVisitor& visitor) {
			visitValues(visitor);
		}

		void swallowAllEvents() { swallow_all_events_ = true; }

		DialogPtr getSettingsDialog(int x, int y, int w, int h);
		variant write();

		void setTabStop(int ts) { tab_stop_ = ts; }
		int tabStop() const { return tab_stop_; }

		float getRotation() const { return rotation_; }
		void setRotation(float r);

		float getScale() const { return scale_; }
		void setScale(float s);

		const point& getPos() const { return position_; }

		void setColor(const KRE::Color& color);
		const KRE::Color& getColor() const { return color_; }

		void setDrawColor(const KRE::Color& color) { draw_color_ = color; }
		const KRE::Color& getDrawColor() const { return draw_color_; }
	protected:
		Widget();
		explicit Widget(const variant& v, game_logic::FormulaCallable* e);
		virtual ~Widget();

		void normalizeEvent(SDL_Event* event, bool translate_coords=false);
		virtual bool handleEvent(const SDL_Event& event, bool claimed) { return claimed; }
		void setEnvironment(game_logic::FormulaCallable* e = 0) { environ_ = e; }
		std::function<void()> on_process_;
		virtual void handleProcess();
		virtual void recalcLoc();
		virtual bool inWidget(int xloc, int yloc) const;
		virtual variant handleWrite();
		virtual WidgetSettingsDialog* settingsDialog(int x, int y, int w, int h);

		virtual void handleDraw() const = 0;

		void surrenderReferences(GarbageCollector* collector);

	private:
		DECLARE_CALLABLE(Widget);
		
		virtual void visitValues(game_logic::FormulaCallableVisitor& visitor) {}
		virtual void handleColorChanged() {}

		int x_, y_;
		int w_, h_;
		int true_x_;
		int true_y_;
		std::shared_ptr<gui::TooltipItem> tooltip_;
		bool tooltip_displayed_;
		std::string tooltipText_;
		int tooltip_font_size_;
		KRE::Color tooltip_color_;
		std::string tooltip_font_;
		bool visible_;
		game_logic::FormulaCallable* environ_;
		void processDelegate();
		game_logic::FormulaPtr ffl_on_process_;
		// default zorder_ is 0.  A widget *must* have a good reason for wanting
		// higher priority in the draw order.
		int zorder_;
		std::string id_;
		bool disabled_;
		uint8_t disabled_opacity_;
		int tooltip_display_delay_;
		int tooltip_ticks_;
		int display_alpha_;
		int pad_h_;
		int pad_w_;
		bool claim_mouse_events_;
		bool draw_with_object_shader_;
		HORIZONTAL_ALIGN align_h_;
		VERTICAL_ALIGN   align_v_;
		int tab_stop_;
		bool has_focus_;

		float rotation_;
		float scale_;

		std::string frame_set_name_;
		ConstFramedGuiElementPtr frame_set_;
		int resolution_;

		bool swallow_all_events_;

		std::shared_ptr<rect> clip_area_;
		point position_;

		KRE::Color color_;
		KRE::Color draw_color_;
	};

	// Functor to sort widgets by z-ordering.
	class WidgetSortZOrder
	{
	public:
		bool operator()(const WidgetPtr& lhs, const WidgetPtr& rhs) const;
	};

	typedef std::set<WidgetPtr, WidgetSortZOrder> SortedWidgetList;

	// Functor to sort widget by tab ordering
	class widgetSortTabOrder
	{
	public:
		bool operator()(const int lhs, const int rhs) const;
	};

	typedef std::multimap<int, WidgetPtr, widgetSortTabOrder> TabSortedWidgetList;
}
