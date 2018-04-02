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
#include <iostream>

#include "Canvas.hpp"
#include "Font.hpp"
#include "geometry.hpp"
#include "WindowManager.hpp"

#include "loading_screen.hpp"
#include "custom_object_type.hpp"
#include "graphical_font.hpp"
#include "i18n.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "profile_timer.hpp"
#include "variant.hpp"

PREF_STRING(loading_screen_bg_color, "#000000", "Color to use for the background of the loading screen");

LoadingScreen::LoadingScreen(int items) 
	: items_(items), 
	status_(0),
	started_at_(profile::get_tick_time())
{
	try {
		background_ = KRE::Texture::createTexture("backgrounds/loading_screen.png");

		auto wnd = KRE::WindowManager::getMainWindow();
		/*
		if(wnd->height() > 0 && wnd->width()/static_cast<float>(wnd->height()) <= 1.4) {
			splash_ = KRE::Texture::createTexture("splash.jpg");
		} else {
			splash_ = KRE::Texture::createTexture("splash-wide.jpg");
		}
		*/
	} catch(KRE::ImageLoadError&) {
	}
}

void LoadingScreen::load(variant node)
{
	for(variant preload_node : node["preload"].as_list())
	{
		drawAndIncrement(preload_node["message"].as_string());
		if(preload_node["type"].as_string() == "object")
		{
			CustomObjectType::get(preload_node["name"].as_string());
		} else if(preload_node["type"].as_string() == "texture") {
			KRE::Texture::createTexture(preload_node["name"].as_string());
		}
	}
}

void LoadingScreen::draw(const std::string& message)
{
	auto wnd = KRE::WindowManager::getMainWindow();
	wnd->setClearColor(KRE::Color(g_loading_screen_bg_color));
	wnd->clear(KRE::ClearFlags::ALL);

	if(splash_) {
		//draw the splash screen while loading
		KRE::Canvas::getInstance()->blitTexture(splash_, 0, rect(0, 0, wnd->width(), wnd->height())); 
	} else {
		drawInternal(message);
	}
	
	wnd->swap();
}

void LoadingScreen::drawInternal(const std::string& message)
{
	if(!background_) {
		LOG_ERROR("No background drawn");
		return;
	}
	auto wnd = KRE::WindowManager::getMainWindow();
	auto canvas = KRE::Canvas::getInstance();
	const int bar_width = 100;
	const int bar_height = 10;
	const int screen_w = wnd->width();
	const int screen_h = wnd->height();

	int bg_w = background_->width();
	int bg_h = background_->height();
//	canvas->blitTexture(background_, 0, rect(screen_w/2-bg_w, std::max(screen_h/2-bg_h, 0), bg_w*2, bg_h*2));
	
	int bar_origin_x = screen_w/2 - bar_width/2;
	rect bg(screen_w/2 - bar_width/2, screen_h/2 - bar_height/2, bar_width, bar_height);
	canvas->drawSolidRect(bg, KRE::Color(96, 96, 96, 255));

	if(items_ > 0) {
		float amount_done = (float)status_ / (float)items_;
		rect bar(screen_w/2 - bar_width/2, screen_h/2 - bar_height/2, static_cast<int>(bar_width*amount_done), bar_height);
		canvas->drawSolidRect(bar, KRE::Color::colorWhite());
	}
	
	std::string font = module::get_default_font();
	if(font == "bitmap") {
		ConstGraphicalFontPtr font = GraphicalFont::get("door_label");
		// explicitly translate loading messages
		if(font) {
			rect getTextSize = font->dimensions(i18n::tr(message));
			font->draw(screen_w/2 - getTextSize.w()/2, screen_h/2 + bar_height/2 + 5, i18n::tr(message));
		}
	} else {
		// todo: we need to load this information (x,y offsets, colors, sizes from a customisation file)
		const int size = 18;
		auto tex = KRE::Font::getInstance()->renderText(i18n::tr(message), KRE::Color::colorWhite(), size, false, font);
		ASSERT_LOG(tex, "Couldn't render text to texture.");
		canvas->blitTexture(tex, 0, rect(screen_w/2 - tex->width()/2, screen_h/2 - tex->height()/2 + bar_height + 10));
	}
}

void LoadingScreen::incrementStatus()
{
	++status_;
}

void LoadingScreen::setNumberOfItems(int items)
{
	items_ = items;
}

void LoadingScreen::finishLoading()
{
	// display the splash screen for a minimum amount of time, if there is one.
	if(!splash_) {
		return;
	}

	while(started_at_ + 3000 > profile::get_tick_time()) {
		drawAndIncrement("Loading");
		profile::delay(20);
	}
}
