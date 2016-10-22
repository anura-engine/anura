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

#include "level_object.hpp"
#include "widget.hpp"

class TileMap;

namespace gui 
{
	class PreviewTilesetWidget : public Widget
	{
	public:
		explicit PreviewTilesetWidget(const TileMap& tiles);
		explicit PreviewTilesetWidget(const variant& v, game_logic::FormulaCallable* e);
		WidgetPtr clone() const override;
	protected:
		void init();
	private:
		DECLARE_CALLABLE(PreviewTilesetWidget)
		void handleDraw() const override;
		void build(const variant& value);
		std::vector<LevelTile> tiles_;
		int width_, height_;
	};

	typedef ffl::IntrusivePtr<PreviewTilesetWidget> PreviewTilesetWidgetPtr;
}
