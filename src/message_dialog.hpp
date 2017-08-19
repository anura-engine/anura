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

#include "geometry.hpp"
#include "Texture.hpp"

class MessageDialog
{
public:
	static void showModal(const std::string& text, const std::vector<std::string>* options=nullptr);
	static void clearModal();
	static MessageDialog* get();
	void draw() const;
	void process();

	int selectedOption() const { return selected_option_; }
private:
	MessageDialog(const std::string& text, const rect& pos,
	              const std::vector<std::string>* options=nullptr);
	std::string text_;
	rect pos_;
	int viewable_lines_;
	int line_height_;

	unsigned cur_row_, cur_char_, cur_wait_;

	std::vector<KRE::TexturePtr> lines_;
	std::vector<KRE::TexturePtr> options_;
	int selected_option_;
};

typedef ffl::IntrusivePtr<MessageDialog> MessageDialogPtr;
