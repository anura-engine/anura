#include <assert.h>
#include <sstream>

#include "auto_update_window.hpp"
#include "filesystem.hpp"
#include "json_parser.hpp"
#include "surface.hpp"

#include <SDL_image.h>
#include <SDL_ttf.h>

namespace {
TTF_Font* get_updater_font() {
	static int res = TTF_Init();
	static TTF_Font* f = TTF_OpenFont("data/fonts/openfonts/FreeMono.ttf", 16);
	assert(f);
	return f;
}

graphics::surface render_updater_text(const std::string& str, const graphics::color& color)
{
	TTF_Font* f = get_updater_font();
	if(f == NULL) {
		return graphics::surface();
	}

	return graphics::surface(TTF_RenderUTF8_Blended(f, str.c_str(), color.as_sdl_color()));
}

class progress_animation
{
public:
	static progress_animation& get() {
		static progress_animation instance;
		return instance;
	}

	progress_animation()
	{
		std::string contents = sys::read_file("update/progress.cfg");
		if(contents == "") {
			SDL_Rect area = {0,0,0,0};
			area_ = area;
			pad_ = rows_ = cols_ = 0;
			return;
		}

		variant cfg = json::parse(contents, json::JSON_NO_PREPROCESSOR);
		SDL_Rect area = { cfg["x"].as_int(), cfg["y"].as_int(), cfg["w"].as_int(), cfg["h"].as_int() };
		area_ = area;
		surf_ = graphics::surface(IMG_Load(cfg["image"].as_string().c_str()));
		pad_ = cfg["pad"].as_int();
		rows_ = cfg["rows"].as_int();
		cols_ = cfg["cols"].as_int();
	}

	graphics::surface surf() const { return surf_; }
	SDL_Rect calculate_rect(int ntime) const {
		if(rows_ * cols_ == 0) {
			return area_;
		}
		ntime = ntime % (rows_*cols_);

		int row = ntime/cols_;
		int col = ntime%cols_;

		SDL_Rect result = area_;
		result.x += (result.w + pad_) * col;
		result.y += (result.h + pad_) * row;
		return result;
	}

private:
	graphics::surface surf_;
	SDL_Rect area_;
	int pad_, rows_, cols_;
};
}

auto_update_window::auto_update_window() : window_(NULL), nframes_(0), start_time_(SDL_GetTicks()), percent_(0.0)
{
}

auto_update_window::~auto_update_window()
{
	if(window_) {
		SDL_DestroyWindow(window_);
	}
}

void auto_update_window::set_progress(float percent)
{
	percent_ = percent;
}

void auto_update_window::set_message(const std::string& str)
{
	message_ = str;
}

void auto_update_window::process()
{
	++nframes_;
	if(window_ == NULL && SDL_GetTicks() - start_time_ > 2000) {
		window_ = SDL_CreateWindow("Updating Anura...", 0, 0, 800, 600, SDL_WINDOW_SHOWN);
	}
}

void auto_update_window::draw() const
{
	if(window_ == NULL) {
		return;
	}

	SDL_Surface* fb = SDL_GetWindowSurface(window_);
	SDL_Rect clear_area = { 0, 0, fb->w, fb->h };
	SDL_FillRect(fb, &clear_area, 0xFF000000);

	SDL_Rect area = {300, 290, 200, 20};
	SDL_FillRect(fb, &area, 0xFFFFFFFF);

	SDL_Rect inner_area = {303, 292, 194, 16};
	SDL_FillRect(fb, &inner_area, 0xFF000000);

	SDL_Rect filled_area = {303, 292, int(194.0*percent_), 16};
	SDL_FillRect(fb, &filled_area, 0xFFFFFFFF);

	const int bar_point = filled_area.x + filled_area.w;

	const int percent = static_cast<int>(percent_*100.0);
	std::ostringstream percent_stream;
	percent_stream << percent << "%";

	graphics::surface percent_surf_white(render_updater_text(percent_stream.str(), graphics::color(255, 255, 255)));
	graphics::surface percent_surf_black(render_updater_text(percent_stream.str(), graphics::color(0, 0, 0)));

	if(percent_surf_white.get() != NULL) {
		SDL_Rect dest = { fb->w/2 - percent_surf_white->w/2, fb->h/2 - percent_surf_white->h/2, percent_surf_white->w, percent_surf_white->h };
		SDL_Rect src = { 0, 0, percent_surf_white->w, percent_surf_white->h };

		SDL_BlitSurface(percent_surf_white.get(), &src, fb, &dest);
	}

	if(percent_surf_black.get() != NULL) {
		SDL_Rect dest = { fb->w/2 - percent_surf_black->w/2, fb->h/2 - percent_surf_black->h/2, percent_surf_black->w, percent_surf_black->h };
		SDL_Rect src = { 0, 0, percent_surf_black->w, percent_surf_black->h };

		if(bar_point > dest.x) {
			if(bar_point < dest.x + dest.w) {
				dest.w = src.w = bar_point - dest.x;
			}
			SDL_BlitSurface(percent_surf_black.get(), &src, fb, &dest);
		}
	}
	
	graphics::surface message_surf(render_updater_text(message_, graphics::color(255, 255, 255)));
	if(message_surf.get() != NULL) {
		SDL_Rect dest = { fb->w/2 - message_surf->w/2, 40 + fb->h/2 - message_surf->h/2, message_surf->w, message_surf->h };
		SDL_BlitSurface(message_surf.get(), NULL, fb, &dest);
	}

	progress_animation& anim = progress_animation::get();
	graphics::surface anim_surf = anim.surf();
	if(anim_surf.get() != NULL) {
		SDL_Rect src = anim.calculate_rect(nframes_);
		SDL_Rect dest = { fb->w/2 - src.w/2, fb->h/2 - src.h*2, src.w, src.h };
		SDL_BlitSurface(anim_surf.get(), &src, fb, &dest);
	}

	SDL_UpdateWindowSurface(window_);
}
