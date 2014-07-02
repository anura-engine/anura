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
#include <vector>

#include "kre/Geometry.hpp"
#include "kre/Texture.hpp"

class message_dialog
{
public:
	static void show_modal(const std::string& text, const std::vector<std::string>* options=NULL);
	static void clear_modal();
	static message_dialog* get();
	void draw() const;
	void process();

	int selected_option() const { return selected_option_; }
private:
	message_dialog(const std::string& text, const rect& pos,
	               const std::vector<std::string>* options=NULL);
	std::string text_;
	rect pos_;
	int viewable_lines_;
	int line_height_;

	unsigned cur_row_, cur_char_, cur_wait_;

	std::vector<KRE::TexturePtr> lines_;
	std::vector<KRE::TexturePtr> options_;
	int selected_option_;
};

typedef boost::intrusive_ptr<message_dialog> message_DialogPtr;
