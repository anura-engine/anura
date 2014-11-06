/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#ifndef OPTIONS_DIALOG_HPP_INCLUDED
#define OPTIONS_DIALOG_HPP_INCLUDED

#include <string>
#include <vector>

#include "texture.hpp"
#include "geometry.hpp"
#include "dialog.hpp"


class options_dialog : public gui::dialog
{
public:	
	void draw() const;
	options_dialog(int x, int y, int w, int h);
protected:
	virtual void handle_draw() const;
};

typedef boost::intrusive_ptr<options_dialog> options_dialog_ptr;


#endif
