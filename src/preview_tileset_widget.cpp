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

#include <iostream>

#include "Canvas.hpp"

#include "preview_tileset_widget.hpp"
#include "tile_map.hpp"

namespace gui 
{
	PreviewTilesetWidget::PreviewTilesetWidget(const TileMap& tiles)
	  : width_(0), 
	    height_(0)
	{
		setEnvironment();
		tiles.buildTiles(&tiles_);
		init();
	}

	PreviewTilesetWidget::PreviewTilesetWidget(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v,e),
		  width_(0),
		  height_(0)
	{
		build(v["tile_map"]);
	}

	void PreviewTilesetWidget::init()
	{
		for(const LevelTile& t : tiles_) {
			const int w = t.x + t.object->width();
			const int h = t.y + t.object->height();

			width_ = std::max(width_, w);
			height_ = std::max(height_, h);
		}

		setDim(width_, height_);
	}

	void PreviewTilesetWidget::handleDraw() const
	{
		if(width_ == 0 || height_ == 0) {
			return;
		}

		//const float scale = std::min(static_cast<float>(width())/width_, static_cast<float>(height())/height_);
		KRE::ModelManager2D mm(x()+4, y()+4/*, 0, scale*/);
		auto canvas = KRE::Canvas::getInstance();
		for(auto& t : tiles_) {
			rect r(t.x/4, t.y/4, 8, 8);
			LevelObject::queueDraw(canvas, t, &r);
		}
	}

	void PreviewTilesetWidget::build(const variant& value)
	{
		TileMap(value).buildTiles(&tiles_);
		init();
	}

	WidgetPtr PreviewTilesetWidget::clone() const
	{
		return WidgetPtr(new PreviewTilesetWidget(*this));
	}

	BEGIN_DEFINE_CALLABLE(PreviewTilesetWidget, Widget)
		DEFINE_FIELD(tile_map, "null")
			return variant();
		DEFINE_SET_FIELD_TYPE("any")
			obj.build(value);
	END_DEFINE_CALLABLE(PreviewTilesetWidget)
}
