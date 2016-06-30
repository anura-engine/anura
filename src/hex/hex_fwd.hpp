/*
	Copyright (C) 2014-2015 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <memory>
#include <vector>

#include "geometry.hpp"
#include "hex_logical_fwd.hpp"

namespace hex 
{
	class HexMap;
	class MaskNode;
	class HexObject;
	class TileSheet;
	class TileType;
	class Overlay;

	typedef boost::intrusive_ptr<HexMap> HexMapPtr;
	typedef boost::intrusive_ptr<MaskNode> MaskNodePtr;
	typedef std::shared_ptr<HexObject> HexObjectPtr;
	typedef std::shared_ptr<const TileSheet> TileSheetPtr;
	typedef std::shared_ptr<TileType> TileTypePtr;
	typedef boost::intrusive_ptr<Overlay> OverlayPtr;
}
