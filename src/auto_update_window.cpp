#include <assert.h>
#include <sstream>

#include <boost/filesystem/operations.hpp>

#include "auto_update_window.hpp"
#include "filesystem.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "profile_timer.hpp"
#include "unit_test.hpp"
#include "variant_utils.hpp"

#include "CameraObject.hpp"
#include "Canvas.hpp"
#include "Font.hpp"
#include "Texture.hpp"

#ifdef _MSC_VER
#include <direct.h>
#define chdir _chdir
#define execv _execv
#else
#include <unistd.h>
#endif

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

auto_update_window::auto_update_window() : window_(), nframes_(0), start_time_(SDL_GetTicks()), percent_(0.0), is_new_install_(false)
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

void auto_update_window::set_error_message(const std::string& str)
{
	error_message_ = str;
}

void auto_update_window::process()
{
	++nframes_;
	if(window_ == nullptr && (SDL_GetTicks() - start_time_ > 2000 || is_new_install_)) {
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
		canvas->blitTexture(percent_surf_white, 0, 
			(window_->width() - percent_surf_white->width()) / 2, 
			(window_->height() - percent_surf_white->height()) / 2);
	}

	if(percent_surf_black != nullptr) {
		rect dest((window_->width() - percent_surf_black->width()) / 2, 
			(window_->height() - percent_surf_black->height()) / 2,
			percent_surf_black->width(),
			percent_surf_black->height());
		if(bar_point > dest.x()) {
			if(bar_point < dest.x2()) {
				dest.set_w(bar_point - dest.x());
			}
			canvas->blitTexture(percent_surf_black, 0, dest);
		}
	}

	KRE::TexturePtr message_surf(render_updater_text(message_, KRE::Color(255, 255, 255)));
	if(message_surf != nullptr) {
		canvas->blitTexture(message_surf, 0, window_->width()/2 - message_surf->width()/2, 40 + window_->height()/2 - message_surf->height()/2);
	}

	if(error_message_ != "") {
		KRE::TexturePtr message_surf(render_updater_text(error_message_, KRE::Color(255, 64, 64)));
		if(message_surf != nullptr) {
			canvas->blitTexture(message_surf, 0, window_->width()/2 - message_surf->width()/2, 80 + window_->height()/2 - message_surf->height()/2);
		}
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

namespace {
bool do_auto_update(std::deque<std::string> argv, auto_update_window& update_window, std::string& error_msg)
{
#ifdef _MSC_VER
	std::string anura_exe = "anura.exe";
#else
	std::string anura_exe = "./anura";
#endif

	std::string subdir;
	std::string real_anura;
	bool update_anura = true;
	bool update_module = true;
	bool force = false;

	int timeout_ms = 10000;

	while(!argv.empty()) {
		std::string arg = argv.front();
		argv.pop_front();

		std::string arg_name = arg;
		std::string arg_value;
		auto equal_itor = std::find(arg_name.begin(), arg_name.end(), '=');
		if(equal_itor != arg_name.end()) {
			arg_value = std::string(equal_itor+1, arg_name.end());
			arg_name = std::string(arg_name.begin(), equal_itor);
		}

		if(arg_name == "--timeout") {
			timeout_ms = atoi(arg_value.c_str());
		} else if(arg_name == "--args") {
			ASSERT_LOG(arg_value.empty(), "Unrecognized argument: " << arg);
			break;
		} else if(arg_name == "--update_module" || arg_name == "--update-module") {
			if(arg_value == "true" || arg_value == "yes") {
				update_module = true;
			} else if(arg_value == "false" || arg_value == "no") {
				update_module = false;
			} else {
				ASSERT_LOG(false, "Unrecognized argument: " << arg);
			}
		} else if(arg_name == "--update_anura" || arg_name == "--update-anura") {
			if(arg_value == "true" || arg_value == "yes") {
				update_anura = true;
			} else if(arg_value == "false" || arg_value == "no") {
				update_anura = false;
			} else {
				ASSERT_LOG(false, "Unrecognized argument: " << arg);
			}
		} else if(arg_name == "--anura") {
			ASSERT_LOG(arg_value.empty() == false, "--anura requires a value giving the name of the anura module to use");
			real_anura = arg_value;
		} else if(arg_name == "--anura-exe" || arg_name == "--anura_exe") {
			ASSERT_LOG(arg_value.empty() == false, "--anura-exe requires a value giving the name of the anura executable to use");
			anura_exe = arg_value;
		} else if(arg_name == "--subdir") {
			subdir = arg_value;
		} else if(arg_name == "--force") {
			force = true;
		} else {
			ASSERT_LOG(false, "Unrecognized argument: " << arg);
		}
	}

	ASSERT_LOG(real_anura != "", "Must provide a --anura argument with the name of the anura module to use");

	variant_builder update_info;


	if(update_anura || update_module) {
		boost::intrusive_ptr<module::client> cl, anura_cl;

		bool is_new_install = false;

		bool has_error = false;

#define HANDLE_ERROR(msg) \
			LOG_ERROR(msg); \
		if(is_new_install || (cl && cl->out_of_date()) || (anura_cl && anura_cl->out_of_date())) { \
			std::ostringstream s; \
			s << msg; \
			error_msg = s.str(); \
			bool newer = strstr(error_msg.c_str(), "newer") != nullptr; \
			if(!newer) { has_error = true; } \
			if(!newer && (!cl || !anura_cl)) { \
				return false; \
			} \
		}
			

		if(update_module) {
			cl.reset(new module::client);
			const bool res = cl->install_module(module::get_module_name(), force);
			if(!res) {
				cl.reset();
			} else {
				update_info.add("attempt_module", true);
				if(cl->is_new_install()) {
					is_new_install = true;
				}
			}
		}

		if(update_anura) {
			anura_cl.reset(new module::client);
			//anura_cl->set_install_image(true);
			const bool res = anura_cl->install_module(real_anura, force);
			if(!res) {
				anura_cl.reset();
			} else {
				update_info.add("attempt_anura", true);
				if(anura_cl->is_new_install()) {
					is_new_install = true;
				}
			}
		}

		fprintf(stderr, "is_new_install = %d\n", (int)is_new_install);

		if(is_new_install) {
			timeout_ms *= 10;
		}

		int nbytes_transferred = 0, nbytes_anura_transferred = 0;
		int start_time = profile::get_tick_time();
		int original_start_time = profile::get_tick_time();
		bool timeout = false;
		LOG_INFO("Requesting update to module from server...");
		int nupdate_cycle = 0;

		if(cl || anura_cl) {
		update_window.set_error_message(error_msg);
		if(is_new_install) {
			update_window.set_is_new_install();
		}
		while(cl || anura_cl) {
			update_window.process();

			int nbytes_obtained = 0;
			int nbytes_needed = 0;

			++nupdate_cycle;

			if(cl) {
				const int transferred = cl->nbytes_transferred();
				nbytes_obtained += transferred;
				nbytes_needed += cl->nbytes_total();
				if(transferred != nbytes_transferred) {
					if(nupdate_cycle%10 == 0) {
						LOG_INFO("Transferred " << (transferred/1024) << "/" << (cl->nbytes_total()/1024) << "KB");
					}
					start_time = profile::get_tick_time();
					nbytes_transferred = transferred;
				}
			}

			if(anura_cl) {
				const int transferred = anura_cl->nbytes_transferred();
				nbytes_obtained += transferred;
				nbytes_needed += anura_cl->nbytes_total();
				if(transferred != nbytes_anura_transferred) {
					if(nupdate_cycle%10 == 0) {
						LOG_INFO("Transferred " << (transferred/1024) << "/" << (anura_cl->nbytes_total()/1024) << "KB");
					}
					start_time = profile::get_tick_time();
					nbytes_anura_transferred = transferred;
				}
			}

			const int time_taken = profile::get_tick_time() - start_time;
			if(time_taken > timeout_ms) {
				HANDLE_ERROR("Timed out updating module. Canceling. " << time_taken << "ms vs " << timeout_ms << "ms");
				if(is_new_install) {
					return false;
				}
				break;
			}

			char msg[1024];
			if(nbytes_needed == 0) {
				sprintf(msg, "Updating Game. Contacting server...");
			} else {
				sprintf(msg, "Updating Game. Transferred %.02f/%.02fMB", float(nbytes_obtained/(1024.0*1024.0)), float(nbytes_needed/(1024.0*1024.0)));
			}

			update_window.set_message(msg);

			const float ratio = nbytes_needed <= 0 ? 0 : static_cast<float>(nbytes_obtained)/static_cast<float>(nbytes_needed);
			update_window.set_progress(ratio);
			update_window.draw();

			SDL_Event event;
			while(SDL_PollEvent(&event)) {
				if(event.type == SDL_QUIT) {
					if(is_new_install) {
						return true;
					}

					cl.reset();
					anura_cl.reset();
					break;
				}
			}

			const int target_end = profile::get_tick_time() + 50;
			while(static_cast<int>(profile::get_tick_time()) < target_end && (cl || anura_cl)) {
				if(cl && !cl->process()) {
					if(cl->error().empty() == false) {
						HANDLE_ERROR("Error while updating module: " << cl->error().c_str());
						update_info.add("module_error", variant(cl->error()));
					} else {
						update_info.add("complete_module", true);
					}
					cl.reset();
				}

				if(anura_cl && !anura_cl->process()) {
					if(anura_cl->error().empty() == false) {
						HANDLE_ERROR("Error while updating anura: " << anura_cl->error().c_str());
						update_info.add("anura_error", variant(anura_cl->error()));
					} else {
						update_info.add("complete_anura", true);
					}
					anura_cl.reset();
				}
			}
		}

		if(has_error && is_new_install) {
			return false;
		}

		}
	}

	const std::string working_dir = preferences::dlc_path() + "/" + real_anura + subdir;
	LOG_INFO("CHANGE DIRECTORY: " << working_dir);
	const int res = chdir(working_dir.c_str());
	ASSERT_LOG(res == 0, "Could not change directory to game working directory: " << working_dir);

	//write the file in the directory we are executing in to tell anura
	//what the auto-update status is.
	sys::write_file("./auto-update-status.json", update_info.build().write_json(false, variant::JSON_COMPLIANT));

	std::vector<char*> anura_args;
	anura_args.push_back(const_cast<char*>(anura_exe.c_str()));

	for(const std::string& a : argv) {
		anura_args.push_back(const_cast<char*>(a.c_str()));
	}

	std::string command_line;
	for(const char* c : anura_args) {
		command_line += '"' + std::string(c) + "\" ";
	}

	LOG_INFO("EXECUTING: " << command_line);

	anura_args.push_back(nullptr);
	
	execv(anura_args[0], &anura_args[0]);

	const bool has_file = sys::file_exists(anura_exe);

#ifndef _MSC_VER
	if(has_file && !sys::is_file_executable(anura_exe)) {
		sys::set_file_executable(anura_exe);

		execv(anura_args[0], &anura_args[0]);

		if(!sys::is_file_executable(anura_exe)) {
			ASSERT_LOG(false, "Could not execute " << anura_exe << " from " << working_dir << " file does not appear to be executable");
		}
	}
#endif

	ASSERT_LOG(has_file, "Could not execute " << anura_exe << " from " << working_dir << ". The file does not exist. Try re-running the update process.");
	ASSERT_LOG(false, "Could not execute " << anura_exe << " from " << working_dir << ". The file exists and appears to be executable.");

	return false;
}
}

COMMAND_LINE_UTILITY(update_launcher)
{
	auto_update_window update_window;
	std::string error_msg;
	std::deque<std::string> argv(args.begin(), args.end());
	try {
		while(!do_auto_update(argv, update_window, error_msg)) {
			SDL_Delay(2000);
		}
	} catch(boost::filesystem::filesystem_error& e) {
		ASSERT_LOG(false, "File Error: " << e.what());
	}
}
