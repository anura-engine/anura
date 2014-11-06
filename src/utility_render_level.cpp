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
#include <boost/intrusive_ptr.hpp>

#include <string>
#include <vector>

#include "foreach.hpp"
#include "IMG_savepng.h"
#include "level.hpp"
#include "string_utils.hpp"
#include "texture_frame_buffer.hpp"
#include "unit_test.hpp"

UTILITY(render_level)
{
	if(args.size() != 2) {
		std::cerr << "render_level usage: <level> <output_file>\n";
		return;
	}

	std::string file = args[0];
	std::string output = args[1];

	std::vector<std::string> files = util::split(args[0]);
	std::vector<std::string> outputs = util::split(args[1]);

	foreach(const std::string& f, files) {
		std::cerr << "FILENAME (" << f << ")\n";
	}
	
	if(files.size() != outputs.size()) {
		std::cerr << "ERROR: " << files.size() << " FILES " << outputs.size() << "outputs\n";
	}

	std::cout << "[";

	for(int n = 0; n != files.size(); ++n) {
		const std::string file = files[n];
		const std::string output = outputs[n];
		
		boost::intrusive_ptr<level> lvl(new level(file));
		lvl->set_editor();
		lvl->finish_loading();
		lvl->set_as_current_level();

		const int lvl_width = lvl->boundaries().w();
		const int lvl_height = lvl->boundaries().h();

		if(n != 0) {
			std::cout << ",";
		}

		std::cout << "\n  {\n  \"name\": \"" << lvl->id() << "\","
		             << "\n  \"dimensions\": [" << lvl->boundaries().x() << "," << lvl->boundaries().y() << "," << lvl->boundaries().w() << "," << lvl->boundaries().h() << "]\n  }";

		const int seg_width = graphics::screen_width();
		const int seg_height = graphics::screen_height();

		graphics::surface level_surface(SDL_CreateRGBSurface(0, lvl_width, lvl_height, 24, SURFACE_MASK_RGB));

		texture_frame_buffer::init(seg_width, seg_height);

		for(int y = lvl->boundaries().y(); y < lvl->boundaries().y2(); y += seg_height) {
			for(int x = lvl->boundaries().x(); x < lvl->boundaries().x2(); x += seg_width) {
				texture_frame_buffer::render_scope scope;
				get_main_window()->prepare_raster();
				glClearColor(0.0, 0.0, 0.0, 0.0);
				glClear(GL_COLOR_BUFFER_BIT);
				glPushMatrix();
	
				glTranslatef(-x, -y, 0);
				lvl->draw(x, y, seg_width, seg_height);
				glPopMatrix();

				get_main_window()->swap();

				graphics::surface s(SDL_CreateRGBSurface(0, seg_width, seg_height, 24, SURFACE_MASK_RGB));
				glReadPixels(0, 0, seg_width, seg_height, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);

				unsigned char* pixels = (unsigned char*)s->pixels;

				for(int n = 0; n != seg_height/2; ++n) {
					unsigned char* s1 = pixels + n*seg_width*3;
					unsigned char* s2 = pixels + (seg_height-n-1)*seg_width*3;
					for(int m = 0; m != seg_width*3; ++m) {
						std::swap(s1[m], s2[m]);
					}
				}

				SDL_Rect src_rect = {0, 0, seg_width, seg_height};
				SDL_Rect dst_rect = {x - lvl->boundaries().x(), y - lvl->boundaries().y(), 0, 0};
				SDL_SetSurfaceBlendMode(s.get(), SDL_BLENDMODE_NONE);
				SDL_BlitSurface(s.get(), &src_rect, level_surface.get(), &dst_rect);
			}
		}

		IMG_SavePNG(output.c_str(), level_surface.get());
	}

	std::cout << "]";
}
