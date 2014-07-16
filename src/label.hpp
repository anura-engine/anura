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

	typedef boost::intrusive_ptr<Label> LabelPtr;
	typedef boost::intrusive_ptr<const Label> ConstLabelPptr;
	typedef boost::intrusive_ptr<DialogLabel> DialogLabelPtr;

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

		void setFontSize(int font_size);
		void setFont(const std::string& font);
		void setColor(const KRE::Color& color);
		void setText(const std::string& text);
		void setFixedWidth(bool fixed_width);
		virtual void setDim(int x, int y);
		const KRE::Color& color() const { return *color_; }
		std::string font() const { return font_; }
		int size() { return size_; }
		std::string text() { return text_; }
		void setClickHandler(std::function<void()> click) { on_click_ = click; }
		void setHighlightColor(const KRE::Color &col) {highlight_color_.reset(new KRE::Color(col));}
		void allowHighlightOnMouseover(bool val=true) { highlight_on_mouseover_ = val; }
	protected:
		std::string& currentText();
		const std::string& currentText() const;
		virtual void recalculateTexture();
		void setTexture(KRE::TexturePtr t);

		virtual bool handleEvent(const SDL_Event& event, bool claimed) override;
		virtual variant handleWrite();
		virtual WidgetSettingsDialog* settingsDialog(int x, int y, int w, int h);

	private:
		DISALLOW_COPY_ASSIGN_AND_DEFAULT(Label);
		DECLARE_CALLABLE(Label);

		void handleDraw() const override;
		void innerSetDim(int x, int y);
		void reformatText();

		std::string text_, formatted_;
		KRE::TexturePtr texture_, border_texture_;
		int border_size_;
		KRE::ColorPtr color_;
		KRE::ColorPtr highlight_color_;
		std::unique_ptr<KRE::Color> border_color_;
		int size_;
		std::string font_;
		bool fixed_width_;

		std::function<void()> on_click_;
		game_logic::FormulaPtr ffl_click_handler_;
		void click_delegate();
		bool highlight_on_mouseover_;
		bool draw_highlight_;
		bool down_;

		friend class dropdown_widget;
	};

	class DialogLabel : public Label
	{
	public:
		explicit DialogLabel(const std::string& text, const KRE::Color& color, int size=18);
		explicit DialogLabel(const variant& v, game_logic::FormulaCallable* e);
		void setProgress(int progress);
		int getMaxProgress() { return stages_; }
	protected:
		virtual void recalculateTexture();
	private:
		DISALLOW_COPY_ASSIGN_AND_DEFAULT(DialogLabel);
		DECLARE_CALLABLE(DialogLabel);

		int progress_, stages_;
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
