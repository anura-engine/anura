#include <assert.h>
#include <sstream>

#include "auto_update_window.hpp"
#include "filesystem.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "variant_utils.hpp"

#include "CameraObject.hpp"
#include "Canvas.hpp"
#include "Font.hpp"
#include "Texture.hpp"

namespace 
{
	KRE::TexturePtr render_updater_text(const std::string& str, const KRE::Color& color)
	{
		return KRE::Font::getInstance()->renderText(str, color, 16, true, KRE::Font::get_default_monospace_font());
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
				area_ = rect();
				pad_ = rows_ = cols_ = 0;
				return;
			}

			variant cfg = json::parse(contents, json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
			area_ = rect(cfg["x"].as_int(), cfg["y"].as_int(), cfg["w"].as_int(), cfg["h"].as_int());
			tex_ = KRE::Texture::createTexture(cfg["image"].as_string());
			pad_ = cfg["pad"].as_int();
			rows_ = cfg["rows"].as_int();
			cols_ = cfg["cols"].as_int();
		}

		KRE::TexturePtr tex() const { return tex_; }
		rect calculate_rect(int ntime) const {
			if(rows_ * cols_ == 0) {
				return area_;
			}
			ntime = ntime % (rows_*cols_);

			int row = ntime / cols_;
			int col = ntime % cols_;

			rect result(area_.x() + (area_.w() + pad_) * col, area_.y() + (area_.h() + pad_) * row, area_.w(), area_.h());
			return result;
		}

	private:
		KRE::TexturePtr tex_;
		rect area_;
		int pad_, rows_, cols_;
	};
}

auto_update_window::auto_update_window() : window_(), nframes_(0), start_time_(SDL_GetTicks()), percent_(0.0)
{
}

auto_update_window::~auto_update_window()
{
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
	if(window_ == nullptr && SDL_GetTicks() - start_time_ > 2000) {
		manager_.reset(new SDL::SDL());

		variant_builder hints;
		hints.add("renderer", "opengl");
		hints.add("title", "Anura auto-update");
		hints.add("clear_color", "black");

		KRE::WindowManager wm("SDL");
		window_ = wm.createWindow(800, 600, hints.build());

		using namespace KRE;

		// Set the default font to use for rendering. This can of course be overridden when rendering the
		// text to a texture.
		Font::setDefaultFont(module::get_default_font() == "bitmap" 
			? "FreeMono" 
			: module::get_default_font());
		std::map<std::string,std::string> font_paths;
		std::map<std::string,std::string> font_paths2;
		module::get_unique_filenames_under_dir("data/fonts/", &font_paths);
		for(auto& fp : font_paths) {
			font_paths2[module::get_id(fp.first)] = fp.second;
		}
		KRE::Font::setAvailableFonts(font_paths2);
		font_paths.clear();
		font_paths2.clear();
	}
}

void auto_update_window::draw() const
{
	if(window_ == nullptr) {
		return;
	}

	auto canvas = KRE::Canvas::getInstance();

	window_->clear(KRE::ClearFlags::COLOR);

	canvas->drawSolidRect(rect(300, 290, 200, 20), KRE::Color(255, 255, 255, 255));
	canvas->drawSolidRect(rect(303, 292, 194, 16), KRE::Color(0, 0, 0, 255));
	const rect filled_area(303, 292, static_cast<int>(194.0f*percent_), 16);
	canvas->drawSolidRect(filled_area, KRE::Color(255, 255, 255, 255));

	const int bar_point = filled_area.x2();

	const int percent = static_cast<int>(percent_*100.0);
	std::ostringstream percent_stream;
	percent_stream << percent << "%";

	KRE::TexturePtr percent_surf_white(render_updater_text(percent_stream.str(), KRE::Color(255, 255, 255)));
	KRE::TexturePtr percent_surf_black(render_updater_text(percent_stream.str(), KRE::Color(0, 0, 0)));

	if(percent_surf_white != nullptr) {
		rect dest(window_->width()/2 - percent_surf_white->width()/2,
		          window_->height()/2 - percent_surf_white->height()/2,
				  0, 0);
		canvas->blitTexture(percent_surf_white, 0, dest);
	}

	if(percent_surf_black != nullptr) {
		rect dest(window_->width()/2 - percent_surf_black->width()/2,
		          window_->height()/2 - percent_surf_black->height()/2,
				  0, 0);

		if(bar_point > dest.x()) {
			if(bar_point < dest.x() + dest.w()) {
				dest.set_w(bar_point - dest.x());
			}
			canvas->blitTexture(percent_surf_black, 0, dest);
		}
	}

	KRE::TexturePtr message_surf(render_updater_text(message_, KRE::Color(255, 255, 255)));
	if(!message_surf) {
		canvas->blitTexture(message_surf, 0, window_->width()/2 - message_surf->width()/2, 40 + window_->height()/2 - message_surf->height()/2);
	}
	
	progress_animation& anim = progress_animation::get();
	auto anim_tex = anim.tex();
	if(anim_tex != nullptr) {
		rect src = anim.calculate_rect(nframes_);
		rect dest(window_->width()/2 - src.w()/2, window_->height()/2 - src.h()*2, src.w(), src.h());
		canvas->blitTexture(anim_tex, src, 0, dest);
	}

	window_->swap();
}
