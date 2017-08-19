/*
	Copyright (C) 2003-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "dialog.hpp"
#include "text_editor_widget.hpp"

namespace gui 
{
	class WidgetSettingsDialog : public Dialog
	{
	public:
		WidgetSettingsDialog(int x, int y, int w, int h, WidgetPtr ptr);
		virtual ~WidgetSettingsDialog();
		WidgetPtr widget() { return widget_; }
		void init();

		void setFont(const std::string& fn);
		std::string font() const { return font_name_; }
		void setTextSize(int ts);
		int getTextSize() const { return text_size_; }
	private:
		WidgetPtr widget_;
		int text_size_;
		std::string font_name_;

		void idChanged(TextEditorWidgetPtr text);

		WidgetSettingsDialog();
		WidgetSettingsDialog(const WidgetSettingsDialog&);
	};

	typedef ffl::IntrusivePtr<WidgetSettingsDialog> WidgetSettingsDialogPtr;
}
