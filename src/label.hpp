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

#include "formula_callable_definition.hpp"
#include "widget.hpp"

namespace gui 
{
	class Label;
	class DialogLabel;
	class DropdownWidget;

	typedef ffl::IntrusivePtr<Label> LabelPtr;
	typedef ffl::IntrusivePtr<const Label> ConstLabelPptr;
	typedef ffl::IntrusivePtr<DialogLabel> DialogLabelPtr;

	class Label : public Widget
	{
	public:
		static LabelPtr create(const std::string& text,
								const KRE::Color& color, 
								int size=14, 
								const std::string& font="") 
		{
			return LabelPtr(new Label(text, color, size, font));
		}
		explicit Label(const std::string& text, const KRE::Color& color, int size=14, const std::string& font="");
		explicit Label(const std::string& text, int size=14, const std::string& font="");
		explicit Label(const variant& v, game_logic::FormulaCallable* e);
		Label(const Label& l);

		void setFontSize(int font_size);
		void setFont(const std::string& font);
		void setText(const std::string& text);
		void setFixedWidth(bool fixed_width);
		virtual void setDim(int x, int y) override;
		std::string font() const { return font_; }
		int size() { return size_; }
		std::string text() { return text_; }
		void setClickHandler(std::function<void()> click) { on_click_ = click; }
		void setHighlightColor(const KRE::Color &col) { highlight_color_ = col;}
		void allowHighlightOnMouseover(bool val=true) { highlight_on_mouseover_ = val; }

		virtual WidgetPtr clone() const override;
	protected:
		std::string& currentText();
		const std::string& currentText() const;
		virtual void recalculateTexture();
		void setTexture(KRE::TexturePtr t);

		virtual bool handleEvent(const SDL_Event& event, bool claimed) override;
		virtual variant handleWrite() override;
		virtual WidgetSettingsDialog* settingsDialog(int x, int y, int w, int h) override;

	private:
		DECLARE_CALLABLE(Label);

		void handleDraw() const override;
		void handleColorChanged() override;
		void innerSetDim(int x, int y);
		void reformatText();

		std::string text_, formatted_;
		KRE::TexturePtr texture_, border_texture_;
		int border_size_;
		KRE::Color highlight_color_;
		std::unique_ptr<KRE::Color> border_color_;
		int size_;
		std::string font_;
		bool fixed_width_;

		std::function<void()> on_click_;
		game_logic::FormulaPtr ffl_click_handler_;
		void clickDelegate();
		bool highlight_on_mouseover_;
		bool draw_highlight_;
		bool down_;

		Label() = delete;
	};

	class DialogLabel : public Label
	{
	public:
		explicit DialogLabel(const std::string& text, const KRE::Color& color, int size=18);
		explicit DialogLabel(const variant& v, game_logic::FormulaCallable* e);
		void setProgress(int progress);
		int getMaxProgress() { return stages_; }
		WidgetPtr clone() const override;
	protected:
		virtual void recalculateTexture() override;
	private:
		DECLARE_CALLABLE(DialogLabel);

		int progress_, stages_;

		DialogLabel() = delete;
	};

	class LabelFactory
	{
	public:
		LabelFactory(const KRE::Color& color, int size);
		LabelPtr create(const std::string& text) const;
		LabelPtr create(const std::string& text, const std::string& tip) const;
	private:
		KRE::Color color_;
		int size_;
	};

}
