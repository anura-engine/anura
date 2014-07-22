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
#ifndef NO_EDITOR

#include <vector>
#include <map>

#include "animation_preview_widget.hpp"
#include "checkbox.hpp"
#include "button.hpp"
#include "dialog.hpp"
#include "dropdown_widget.hpp"
#include "grid_widget.hpp"
#include "label.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"
#include "variant.hpp"

namespace gui 
{
	class AnimationCreatorDialog : public Dialog
	{
	public:
		AnimationCreatorDialog(int x, int y, int w, int h, const variant& anims=variant());
		virtual ~AnimationCreatorDialog() 
		{}
		variant getAnimations() { return variant(&anims_); }
		void process();
	protected:
		void init();
	
		void setDestination();
		void selectAnimation(int index);
		void setOption();
		void animDel();
		void animNew();
		void animSave(Dialog* d);
		void finish();
		bool showAttribute(variant v);

		void checkAnimChanged();
		void resetCurrentObject();

		virtual void handleDraw() const override;
		virtual bool handleEvent(const SDL_Event& event, bool claimed) override;
	private:
		std::vector<variant> anims_;
		variant current_;				// Holds the currently selected variant.
		int selected_frame_;

		std::string copy_path_;
		std::string image_file_name_;	// file name.
		std::string image_file_;		// full path.
		std::string rel_path_;			// Path relative to modules images path.

		bool changed_;					// current animation modified?
		bool simple_options_;			// simplified list of options.

		std::vector<std::string> commonAnimationList();
		void onIdChange(DropdownWidgetPtr editor, const std::string& s);
		void onIdSet(DropdownWidgetPtr editor, int selection, const std::string& s);
		void setImageFile();
		void changeText(const std::string& s, TextEditorWidgetPtr editor, SliderPtr Slider);
		void executeChangeText(const std::string& s, TextEditorWidgetPtr editor, SliderPtr Slider);
		void changeSlide(const std::string& s, TextEditorWidgetPtr editor, double d);
		void endSlide(const std::string& s, SliderPtr slide, TextEditorWidgetPtr editor, double d);

		void setAnimationRect(rect r);
		void moveSolidRect(int dx, int dy);
		void setIntegerAttr(const char* attr, int value);

		typedef std::pair<std::string, int> slider_offset_pair;
		std::map<std::string, int> slider_offset_;
		bool dragging_slider_;

		AnimationPreviewWidgetPtr animation_preview_;
	};

}

#endif // NO_EDITOR
