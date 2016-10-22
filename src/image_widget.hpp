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

#include "gui_section.hpp"
#include "image_widget_fwd.hpp"
#include "widget.hpp"

namespace gui 
{
	class ImageWidget : public Widget
	{
	public:
		explicit ImageWidget(const std::string& fname, int w=-1, int h=-1);
		explicit ImageWidget(KRE::TexturePtr tex, int w=-1, int h=-1);
		explicit ImageWidget(const variant& v, game_logic::FormulaCallable* e);

		void init(int w, int h);

		const rect& area() const { return area_; }
		const KRE::TexturePtr& tex() const { return texture_; }

		void setRotation(float rotate) { rotate_ = rotate; }
		void setArea(const rect& area) { area_ = area; }

		WidgetPtr clone() const override;
	private:
		DECLARE_CALLABLE(ImageWidget);

		void handleDraw() const override;

		KRE::TexturePtr texture_;
		float rotate_;
		rect area_;
		std::string image_name_;
	};

	class GuiSectionWidget : public Widget
	{
	public:
		explicit GuiSectionWidget(const std::string& id, int w=-1, int h=-1, int scale=1);
		explicit GuiSectionWidget(const variant& v, game_logic::FormulaCallable* e);

		//sets the GUI section. The dimensions of the widget will not change;
		//you should set a GUI section that is the same size.
		void setGuiSection(const std::string& id);

		WidgetPtr clone() const override;
	private:
		DECLARE_CALLABLE(GuiSectionWidget);
		void handleDraw() const override;
		ConstGuiSectionPtr section_;
		int scale_;
	};

	typedef ffl::IntrusivePtr<GuiSectionWidget> GuiSectionWidgetPtr;
}
