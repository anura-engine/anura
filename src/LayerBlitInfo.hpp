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

#include "AttributeSet.hpp"
#include "SceneObject.hpp"
#include "Texture.hpp"

#include "draw_tile.hpp"

class LayerBlitInfo : public KRE::SceneObject
{
public:
	LayerBlitInfo();
	bool isInitialised() const { return initialised_; }
	int xbase() const { return xbase_; }
	int ybase() const { return ybase_; }
	void setXbase(int xb) { xbase_ = xb; }
	void setYbase(int yb) { ybase_ = yb; }
	void setBase(int xb, int yb) { xbase_ = xb; ybase_ = yb; initialised_ = true; }

	void setVertices(std::vector<tile_corner>* op, std::vector<tile_corner>* tr);
private:
	int xbase_;
	int ybase_;
	bool initialised_;

	std::shared_ptr<KRE::Attribute<tile_corner>> opaques_;
	std::shared_ptr<KRE::Attribute<tile_corner>> transparent_;
};