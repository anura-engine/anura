/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "scrollable_widget.hpp"
#include "widget.hpp"
#include "variant.hpp"

namespace gui 
{
	class TreeViewWidget : public ScrollableWidget
	{
	public:
		explicit TreeViewWidget(int w, int h, const variant& tree);
		explicit TreeViewWidget(const variant& v, game_logic::FormulaCallable* e);
		virtual ~TreeViewWidget()
		{}
		void allowSelection(bool val=true) { allow_selection_ = val; }
		void mustSelect(bool val=true, int nrow=0) { must_select_ = val; selected_row_ = nrow; }
		bool hasMustSelect() const { return must_select_; }
		void swallowClicks(bool val=true) { swallow_clicks_ = val; }
		int selection() const { return selected_row_; }
		int getNRows() const { return nrows_; }
		void setMinColSize(int minc) { min_col_size_ = minc; }
		void setMaxColSize(int maxc) { max_col_size_ = maxc; }
		void registerSelectionCallback(std::function<void(const variant&, const variant&)> select_fn) 
		{
			on_select_ = select_fn;
		}
		void allowPersistentHighlight(bool val=true, const KRE::Color& col=KRE::Color::colorBlue())
		{
			persistent_highlight_ = val;
			highlight_color_ = col;
		}
		variant getTree() const { return tree_; }
		virtual WidgetPtr getWidgetById(const std::string& id) override;
		virtual ConstWidgetPtr getWidgetById(const std::string& id) const override;
		virtual WidgetPtr clone() const override;
	protected:
		void onSetYscroll(int old_value, int value) override;
		virtual void init();
		virtual void onSelect(Uint8 button, int selection);

		virtual void onTraverseElement(const variant& key, variant* parent, variant* value, int row);

		int row_height_;
		variant get_selection_key(int selection) const;
		variant tree_;
	private:
		DECLARE_CALLABLE(TreeViewWidget)

		virtual void handleDraw() const override;
		virtual bool handleEvent(const SDL_Event& event, bool claimed) override;

		virtual int traverse(int depth, int x, int y, variant* parent, const variant& key, variant* value);
		void genTraverse(int depth, std::function<void(int,const variant&,variant*)> fn, const variant& key, variant* value);
		int getRowAt(int xpos, int ypos) const;
		void recalculateDimensions();
		void calcColumnWidths(int depth, const variant& key, variant* value);

		int hpad_;			// Amount int pixels to horizonatally pad
		int col_size_;		// A virtual column
		int font_size_;
		int char_height_;
		int char_width_;
		int min_col_size_;
		int max_col_size_;

		bool allow_selection_;
		bool swallow_clicks_;
		bool must_select_;
		int selected_row_;
		int nrows_;
		int max_height_;

		bool persistent_highlight_;
		KRE::Color highlight_color_;
		int highlighted_row_;

		std::function<void(const variant&, const variant&)> on_select_;
		std::vector<WidgetPtr> widgets_;
		std::map<int, int> last_coords_;
		std::vector<int> col_widths_;
		std::map<int, variant_pair> selection_map_;
	};

	typedef ffl::IntrusivePtr<TreeViewWidget> TreeViewWidgetPtr;
	typedef ffl::IntrusivePtr<const TreeViewWidget> ConstTreeViewWidgetPtr;

	class TreeEditorWidget : public TreeViewWidget
	{
	public:
		explicit TreeEditorWidget(int w, int h, const variant& tree);
		explicit TreeEditorWidget(const variant& v, game_logic::FormulaCallable* e);
		virtual ~TreeEditorWidget()
		{}
		void setEditorHandler(variant::TYPE vt, WidgetPtr editor, std::function<void(variant*,std::function<void(const variant&)>)> editor_select) { 
			ex_editor_map_[vt] = editor; 
			on_editor_select_ = editor_select;
		}
		void externalEditorSave(variant* v, const variant &new_value);
		WidgetPtr clone() const override;
	protected:
		virtual void init() override;
		virtual void onSelect(Uint8 button, int selection) override;
		virtual void onTraverseElement(const variant& key, variant* parent, variant* value, int row) override;
	private:
		DECLARE_CALLABLE(TreeEditorWidget)

		virtual void handleDraw() const override;
		virtual bool handleEvent(const SDL_Event& event, bool claimed) override;

		void contextMenuHandler(int tree_selection, const std::vector<std::string>& choices, int menu_selection);
		WidgetPtr context_menu_;
		WidgetPtr edit_menu_;

		std::function<void(variant*,std::function<void(const variant&)>)> on_editor_select_;

		void editField(int row, variant* v);
		void executeEditEnter(const TextEditorWidgetPtr editor, variant* value);
		void executeEditSelect(int selection);
		void onBoolChange(variant* v, int selection, const std::string& s);
		void executeKeyEditEnter(const TextEditorWidgetPtr editor, variant* parent, const variant& key, variant* value);
		void executeKeyEditSelect(int selection);

		std::map<int, std::pair<variant*, variant*> > row_map_;
		std::map<variant::TYPE, WidgetPtr> ex_editor_map_;
	};

	typedef ffl::IntrusivePtr<TreeEditorWidget> TreeEditorWidgetPtr;
	typedef ffl::IntrusivePtr<const TreeEditorWidget> ConstTreeEditorWidgetPtr;
}
