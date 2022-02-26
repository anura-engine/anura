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

#include <string>

#include "Texture.hpp"

#include "graphical_font.hpp"
#include "variant.hpp"

class LoadingScreen
{
public:
	LoadingScreen(int items=0);
	void load(variant node); // preload objects defined by preload children of node, blocking, and calling draw automatically
	void draw(const std::string& message);
	void incrementStatus();
	void drawAndIncrement(const std::string& message) {draw(message); incrementStatus();}
	void setNumberOfItems(int items);

	void finishLoading();

private:
	void drawInternal(const std::string& message);
	int items_; // number of items we'll load
	int status_; // how many items we've loaded so far
	KRE::TexturePtr background_;
	KRE::TexturePtr splash_;

	int started_at_;
};
