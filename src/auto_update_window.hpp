#pragma once

#include <string>

#include "SDLWrapper.hpp"
#include "WindowManager.hpp"

class auto_update_window
{
public:
	auto_update_window();
	~auto_update_window();

	void set_progress(float percent);
	void set_message(const std::string& str);

	void process();
	void draw() const;
private:
	KRE::WindowPtr window_;
	SDL::SDL_ptr manager_;
	int nframes_;
	int start_time_;
	std::string message_;
	float percent_;
};
