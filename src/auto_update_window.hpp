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
	void set_error_message(const std::string& msg);

	void process();
	void draw() const;
	void set_is_new_install() { is_new_install_ = true; }
private:
	KRE::WindowPtr window_;
	SDL::SDL_ptr manager_;
	int nframes_;
	int start_time_;
	std::string message_, error_message_;
	float percent_;
	bool is_new_install_;
};
