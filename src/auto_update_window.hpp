#pragma once

#include <string>

#include "SDLWrapper.hpp"
#include "Texture.hpp"
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

	bool proceed_or_retry_dialog(const std::string& msg);

	void set_module_path(const std::string& path) { module_path_ = path; }
	void load_background_texture();
	void load_background_texture(const std::string& path);
private:
	void create_window();

	KRE::WindowPtr window_;

	KRE::TexturePtr bg_texture_;
	std::string module_path_;
	SDL::SDL_ptr manager_;
	int nframes_;
	int start_time_;
	std::string message_, error_message_;
	float percent_;
	bool is_new_install_;
};
