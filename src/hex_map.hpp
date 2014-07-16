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

#include <boost/intrusive_ptr.hpp>
#include <vector>

#include "kre/Geometry.hpp"

#include "hex_object_fwd.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "variant.hpp"

namespace hex 
{
	class HexMap : public game_logic::FormulaCallable
	{
	public:
		HexMap();
		virtual ~HexMap();
		explicit HexMap(variant node);
		int getZorder() const { return zorder_; }
		void setZorder(int zorder) { zorder_ = zorder; }

		int x() const { return x_; }
		int y() const { return y_; }

		size_t width() const { return width_; }
		size_t height() const { return height_; }
		size_t size() const { return width_ * height_; }
		void build();
		virtual void draw() const;
		variant write() const;

		game_logic::FormulaPtr createFormula(const variant& v);
		bool executeCommand(const variant& var);

		bool setTile(int x, int y, const std::string& tile);
		void calculateTileAdjacency();

		HexObjectPtr getHexTile(Direction d, int x, int y) const;
		HexObjectPtr getTileAt(int x, int y) const;
		HexObjectPtr getTileFromPixelPos(int x, int y) const;
		static point getTilePosFromPixelPos(int x, int y);
		static point getPixelPosFromTilePos(int x, int y);

		static point locInDir(int x, int y, Direction d);
		static point locInDir(int x, int y, const std::string& s);
	private:
		DECLARE_CALLABLE(HexMap)

		std::vector<HexObjectPtr> tiles_;
		size_t width_;
		size_t height_;
		int x_;
		int y_;
		int zorder_;
	};

	typedef boost::intrusive_ptr<HexMap> HexMapPtr;
	typedef boost::intrusive_ptr<const HexMap> ConstHexMapPtr;
}
