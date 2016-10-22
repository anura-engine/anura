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

#include <string>
#include <vector>

#include "DisplayDevice.hpp"
#include "ModelMatrixScope.hpp"
#include "RenderTarget.hpp"
#include "WindowManager.hpp"

#include "level.hpp"
#include "string_utils.hpp"
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

	for(const std::string& f : files) {
		LOG_INFO("FILENAME (" << f << ")");
	}
	
	if(files.size() != outputs.size()) {
		LOG_INFO("ERROR: " << files.size() << " FILES " << outputs.size() << "outputs");
	}

	std::cout << "[";

	for(int n = 0; n != files.size(); ++n) {
		const std::string file = files[n];
		const std::string output = outputs[n];
		
		ffl::IntrusivePtr<Level> lvl(new Level(file));
		lvl->set_editor();
		lvl->finishLoading();
		lvl->setAsCurrentLevel();

		const int lvl_width = lvl->boundaries().w();
		const int lvl_height = lvl->boundaries().h();

		if(n != 0) {
			std::cout << ",";
		}

		std::cout << "\n  {\n  \"name\": \"" << lvl->id() << "\","
		             << "\n  \"dimensions\": [" << lvl->boundaries().x() << "," << lvl->boundaries().y() << "," << lvl->boundaries().w() << "," << lvl->boundaries().h() << "]\n  }";

		auto wnd = KRE::WindowManager::getMainWindow();

		const int seg_width = wnd->width();
		const int seg_height = wnd->height();

		KRE::SurfacePtr level_surface = KRE::Surface::create(lvl_width, lvl_height, KRE::PixelFormat::PF::PIXELFORMAT_RGB24);

		auto fbo = KRE::DisplayDevice::renderTargetInstance(seg_width, seg_height);
		fbo->setClearColor(KRE::Color(0,0,0));

		for(int y = lvl->boundaries().y(); y < lvl->boundaries().y2(); y += seg_height) {
			for(int x = lvl->boundaries().x(); x < lvl->boundaries().x2(); x += seg_width) {
				fbo->apply();
				fbo->clear();
				KRE::ModelManager2D mm(-x, -y);
				lvl->draw(x, y, seg_width, seg_height);
				
				auto s = fbo->readToSurface(nullptr);

				fbo->unapply();

				rect src_rect(0, 0, seg_width, seg_height);
				rect dst_rect(x - lvl->boundaries().x(), y - lvl->boundaries().y(), 0, 0);
				s->setBlendMode(KRE::Surface::BlendMode::BLEND_MODE_NONE);
				level_surface->blitTo(s, src_rect, dst_rect);
			}
		}

		level_surface->savePng(output);
	}

	std::cout << "]";
}
