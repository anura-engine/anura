#include <assert.h>
#include <sstream>

#include <boost/filesystem/operations.hpp>

#include <GL/glew.h>

#include "auto_update_window.hpp"
#include "base64.hpp"
#include "compress.hpp"
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

extern std::string g_anura_exe_name;
extern std::string g_loading_screen_bg_color;

PREF_STRING(auto_update_dir, "", "Directory in which the auto-updater is");
PREF_STRING(auto_update_exe, "", "Exe of the auto-updater");

PREF_STRING(auto_update_game_name, "Anura", "Title shown on the auto update window");
PREF_STRING(auto_update_title, "Anura auto-update", "Title of the auto-update window");

PREF_STRING(auto_update_install_dir, "", "Directory which to install the game to");

void run_auto_updater()
{
	std::string original_cwd = sys::get_cwd();

	if(!g_auto_update_dir.empty() && !g_auto_update_exe.empty()) {

		int res = chdir(g_auto_update_dir.c_str());
		if(res != 0) {
			LOG_ERROR("Auto-update: Could not chdir " << g_auto_update_dir << ": " << res);
			return;
		}

		std::vector<char*> anura_args;
		anura_args.push_back(const_cast<char*>(g_anura_exe_name.c_str()));
		anura_args.push_back(nullptr);

		LOG_ERROR("Auto-update: switched to " << g_auto_update_dir << " running " << g_auto_update_exe);

		execv(anura_args[0], &anura_args[0]);
		LOG_ERROR("Failed to execute auto updater. Re-running game...");
#if defined(__linux__)
		LOG_ERROR("Errno: " << errno);
#endif

		res = chdir(original_cwd.c_str());
		if(res != 0) {
			LOG_ERROR("Auto-update: Could not chdir " << original_cwd << ": " << res);
			return;
		}
	}
	
	auto v = preferences::argv();
	std::vector<char*> anura_args;
	for(const std::string& s : v) {
		anura_args.push_back(const_cast<char*>(s.c_str()));
	}

	anura_args.push_back(nullptr);

	execv(anura_args[0], &anura_args[0]);
	LOG_ERROR("Failed to restart game\n");
#if defined(__linux__)
	LOG_ERROR("Errno: " << errno);
#endif

	return;
}

namespace 
{
	variant get_update_config(const std::string& name)
	{
		static std::map<std::string,variant> attr;
		static bool init = false;
		if(!init) {
			init = true;
			try {
				if(sys::file_exists("./update/update.cfg")) {
					variant cfg = json::parse(sys::read_file("./update/update.cfg"), json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);

					if(cfg.is_map()) {
						for(auto p : cfg.as_map()) {
							if(p.first.is_string()) {
								attr[p.first.as_string()] = p.second;
							}
						}
					}
				}
			} catch(...) {
			}
		}

		return attr[name];
	}

	KRE::Color get_update_color(const std::string& name, std::string default_val)
	{
		variant v = get_update_config(name);
		if(v.is_null()) {
			return KRE::Color(default_val);
		}

		return KRE::Color(v);
	}

	KRE::TexturePtr render_updater_text(const std::string& str, const KRE::Color& color, int size=-1)
	{
		if(size < 0) {
			size = get_update_config("font_size").as_int(24);
		}

		return KRE::Font::getInstance()->renderText(str, color, size, true, KRE::Font::getDefaultFont());
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

void auto_update_window::load_background_texture()
{
	if(module_path_.empty() == false) {
		if(sys::file_exists(module_path_ + "update-bg.jpg")) {
			sys::copy_file(module_path_ + "update-bg.jpg", "./update/update-bg.jpg");
		}
	}

	load_background_texture("./update/update-bg.jpg");
}

void auto_update_window::load_background_texture(const std::string& path)
{

	try {
		assert_recover_scope guard;
		bg_texture_ = KRE::Texture::createTexture(path);
	} catch(...) {
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

void auto_update_window::set_error_message(const std::string& str)
{
	error_message_ = str;
}

void auto_update_window::process()
{
	++nframes_;
	if(window_ == nullptr && (SDL_GetTicks() - start_time_ > 2000 || is_new_install_)) {
		create_window();
	}
}

void auto_update_window::create_window()
{
	if(window_ != nullptr) {
		return;
	}

	manager_.reset(new SDL::SDL());

	variant_builder hints;
	hints.add("renderer", "opengl");
	hints.add("title", get_update_config("window_title").as_string_default(g_auto_update_title.c_str()));
	hints.add("clear_color", "black");

	KRE::WindowManager wm("SDL");
	window_ = wm.createWindow(get_update_config("window_width").as_int(800), get_update_config("window_height").as_int(600), hints.build());

	window_->setWindowIcon("update/window-icon.png");

	using namespace KRE;

	// Set the default font to use for rendering. This can of course be overridden when rendering the
	// text to a texture.
	Font::setDefaultFont("default");
	std::map<std::string,std::string> font_paths;
	font_paths["default"] = "update/font.otf";
	KRE::Font::setAvailableFonts(font_paths);

	load_background_texture();
}

void auto_update_window::draw() const
{
	if(window_ == nullptr) {
		return;
	}

	auto canvas = KRE::Canvas::getInstance();

	window_->setClearColor(get_update_color("background_color", "black"));
	window_->clear(KRE::ClearFlags::COLOR);

	if(bg_texture_) {
		canvas->blitTexture(bg_texture_, 0, 0, 0);
	}

	const int bar_width = get_update_config("bar_width").as_int(400);

	canvas->drawSolidRect(rect(window_->width()/2 - bar_width/2, get_update_config("bar_ypos").as_int(480), bar_width, get_update_config("bar_height").as_int(10)), get_update_color("bar_empty_color", "white"));
	const rect filled_area(window_->width()/2 - bar_width/2, get_update_config("bar_ypos").as_int(480), int(bar_width*percent_), get_update_config("bar_height").as_int(10));
	canvas->drawSolidRect(filled_area, get_update_color("bar_filled_color", "cyan"));

	KRE::TexturePtr aa_text_surf(render_updater_text(get_update_config("title_text").as_string_default(g_auto_update_game_name.c_str()), get_update_color("title_text_color", "white"), get_update_config("title_font_size").as_int(48)));
	if(aa_text_surf) {
		canvas->blitTexture(aa_text_surf, 0, window_->width()/2 - aa_text_surf->width()/2, get_update_config("title_ypos").as_int(300));
	}

	const int bar_point = filled_area.x2();

	const int percent = static_cast<int>(percent_*100.0);
	std::ostringstream percent_stream;
	percent_stream << percent << "%";

	KRE::TexturePtr percent_surf(render_updater_text(percent_stream.str(), get_update_color("percent_text_color", "white"), get_update_config("percent_font_size").as_int(24)));

	if(percent_surf != nullptr) {
		canvas->blitTexture(percent_surf, 0, 
			(window_->width() - percent_surf->width()) / 2, get_update_config("percent_ypos").as_int(440));
	}

	KRE::TexturePtr message_surf(render_updater_text(message_, KRE::Color(get_update_config("message_text_color").as_string_default("white")), 20));
	if(message_surf != nullptr) {
		canvas->blitTexture(message_surf, 0, window_->width()/2 - message_surf->width()/2, get_update_config("message_ypos").as_int(500));
	}

	if(error_message_ != "") {
		KRE::TexturePtr message_surf(render_updater_text(error_message_, get_update_color("error_text_color", "red")));
		if(message_surf != nullptr) {
			canvas->blitTexture(message_surf, 0, window_->width()/2 - message_surf->width()/2, get_update_config("error_ypos").as_int(540));
		}
	}
	
	progress_animation& anim = progress_animation::get();
	auto anim_tex = anim.tex();
	if(anim_tex != nullptr) {
		rect src = anim.calculate_rect(nframes_);
		rect dest(window_->width()/2 - src.w()/2, window_->height()/2 - src.h(), src.w(), src.h());
		canvas->blitTexture(anim_tex, src, 0, dest);
	}
	window_->swap();
}

bool auto_update_window::proceed_or_retry_dialog(const std::string& msg)
{
	create_window();

	static const KRE::Color normal_button_color(0,140,114,255);
	static const KRE::Color depressed_button_color(168,64,30,255);
	static const KRE::Color text_button_color(158,216,166,255);

	for(;;) {
		auto canvas = KRE::Canvas::getInstance();

		window_->clear(KRE::ClearFlags::COLOR);

		int mx, my;
		SDL_GetMouseState(&mx, &my);

		point mouse_pos(mx, my);

		const rect proceed_button_area(window_->width()/2 - 200, window_->height()/2 + 100, 100, 40);
		const rect retry_button_area(window_->width()/2 + 100, window_->height()/2 + 100, 100, 40);

		const bool mouseover_proceed = pointInRect(mouse_pos, proceed_button_area);
		const bool mouseover_retry = pointInRect(mouse_pos, retry_button_area);

		canvas->drawSolidRect(proceed_button_area, mouseover_proceed ? depressed_button_color : normal_button_color);
		canvas->drawSolidRect(retry_button_area, mouseover_retry ? depressed_button_color : normal_button_color);

		KRE::TexturePtr proceed_text_texture = KRE::Font::getInstance()->renderText("Proceed", KRE::Color(0,0,0,255), 24, true, KRE::Font::getDefaultFont());
		KRE::TexturePtr retry_text_texture = KRE::Font::getInstance()->renderText("Retry", KRE::Color(0,0,0,255), 24, true, KRE::Font::getDefaultFont());
		canvas->blitTexture(proceed_text_texture, 0, (proceed_button_area.x() + proceed_button_area.x2() - proceed_text_texture->width())/2, (proceed_button_area.y() + proceed_button_area.y2() - proceed_text_texture->height())/2);
		canvas->blitTexture(retry_text_texture, 0, (retry_button_area.x() + retry_button_area.x2() - retry_text_texture->width())/2, (retry_button_area.y() + retry_button_area.y2() - retry_text_texture->height())/2);

		KRE::TexturePtr message_texture = KRE::Font::getInstance()->renderText("Failed to update the game. Retry or proceed without updating?", KRE::Color(255,255,255,255), 24, true, KRE::Font::getDefaultFont());
		canvas->blitTexture(message_texture, 0, (window_->width() - message_texture->width())/2, window_->height()/2);

		message_texture = KRE::Font::getInstance()->renderText(msg, KRE::Color(255,0,0,255), 24, true, KRE::Font::getDefaultFont());
		canvas->blitTexture(message_texture, 0, (window_->width() - message_texture->width())/2, window_->height()/2 + 40);

		window_->swap();

		SDL_PumpEvents();
		SDL_Delay(20);

		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
			case SDL_QUIT:
				SDL_Quit();
				exit(0);
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if(mouseover_proceed) {
					return true;
				} else if(mouseover_retry) {
					return false;
				}

				break;
			}
		}
	}

	return true;
}

namespace {

class module_updater_client : public module::client
{
public:
	explicit module_updater_client(auto_update_window& w) : window_(w)
	{}
private:
	auto_update_window& window_;
	std::map<std::string, std::string> update_chunks_;
	virtual bool isHighPriorityChunk(const variant& chunk_id, variant& chunk) override {
		if(!chunk_id.is_string()) {
			return false;
		}

		fprintf(stderr, "CHUNK: %s -> %s\n",chunk_id.as_string().c_str(), chunk["md5"].as_string().c_str());

		std::string id = chunk_id.as_string();
		if(std::equal(id.begin(), id.begin()+7, "update/") == false) {
			return false;
		}

		update_chunks_[chunk["md5"].as_string()] = id;
		return true;

		static const std::string update_bg("update/update-bg.jpg");
		if(id == update_bg) {
			return true;
		}
		return false;
	}

	virtual void onChunkReceived(variant& chunk) override {
		auto itor = update_chunks_.find(chunk["md5"].as_string());
		if(itor != update_chunks_.end()) {
			try {
				std::string data_str;
				if(chunk["data"].is_string()) {
					data_str = chunk["data"].as_string();
				} else {
					data_str = sys::read_file("update-cache/" + chunk["md5"].as_string());
				}

				std::vector<char> data_buf;
				data_buf.insert(data_buf.begin(), data_str.begin(), data_str.end());

				const int data_size = chunk["size"].as_int();

				std::vector<char> data = zip::decompress_known_size(base64::b64decode(data_buf), data_size);
				std::string contents(data.begin(), data.end());

				fprintf(stderr, "WRITE FILE: %s\n", itor->second.c_str());

				sys::write_file(itor->second, contents);

				if(itor->second == "update/update-bg.jpg") {
					window_.set_module_path("");
					window_.load_background_texture("update/update-bg.jpg");
				}
			} catch(...) {
			}
		}
	}
};

bool do_auto_update(std::deque<std::string> argv, auto_update_window& update_window, std::string& error_msg, int timeout_ms)
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

	if(sys::file_exists("./update/overrides.cfg")) {
		try {
			variant overrides = json::parse(sys::read_file("./update/overrides.cfg"), json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
			if(overrides.is_map()) {
				variant one_time_args = overrides["arguments"];
				if(one_time_args.is_list()) {
					for(auto s : one_time_args.as_list_string()) {
						argv.push_front(s);
					}
				}
			}
		} catch(...) {
		}
	}

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
		ffl::IntrusivePtr<module::client> cl, anura_cl;

		bool is_new_install = false;

		bool has_error = false;

#define HANDLE_ERROR(msg) \
		{ \
			LOG_ERROR(msg); \
			std::ostringstream s; \
			s << msg; \
			error_msg = s.str(); \
			bool newer = strstr(error_msg.c_str(), "newer") != nullptr; \
			if(!newer) { has_error = true; } \
			if(!newer && (!cl || !anura_cl)) { \
				if(is_new_install || update_window.proceed_or_retry_dialog(error_msg) == false) { \
					 return false; \
				} \
			} \
		}
			

		if(update_module) {
			cl.reset(new module_updater_client(update_window));
			cl->set_module_description("game");
			if(g_auto_update_install_dir.empty() == false) {
				cl->set_install_path_override(g_auto_update_install_dir + "/modules/" + module::get_module_name());
			}

			cl->set_show_progress_fn([&update_window](const std::string& msg) {
				update_window.set_message(msg);
				update_window.process();
				update_window.draw();
			});
			const bool res = cl->install_module(module::get_module_name(), force);
			if(!res) {
				cl.reset();
			} else {
				update_info.add("attempt_module", true);
				if(cl->is_new_install()) {
					is_new_install = true;
				}

				update_window.set_module_path(cl->module_path());
			}
		}

		if(update_anura) {
			anura_cl.reset(new module::client);
			anura_cl->set_module_description("engine");
			if(g_auto_update_install_dir.empty() == false) {
				anura_cl->set_install_path_override(g_auto_update_install_dir);
			}

			anura_cl->set_show_progress_fn([&update_window](const std::string& msg) {
				update_window.set_message(msg);
				update_window.process();
				update_window.draw();
			});
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

		if(is_new_install) {
			timeout_ms *= 10;
		}

		int start_time = profile::get_tick_time();
		bool timeout = false;
		LOG_INFO("Requesting update to module from server...");
		int nupdate_cycle = 0;

		if(cl || anura_cl) {
		update_window.set_error_message(error_msg);
		if(is_new_install) {
			update_window.set_is_new_install();
		}

		ffl::IntrusivePtr<module::client> cl_install, anura_cl_install;

		int cl_nbytes_total = 0, anura_cl_nbytes_total = 0, cl_nbytes_transferred = 0, anura_cl_nbytes_transferred = 0;

		while(cl || anura_cl) {
			update_window.process();

			++nupdate_cycle;

			if(cl) {
				cl_nbytes_transferred = cl->nbytes_transferred();
				cl_nbytes_total = cl->nbytes_total();
			}

			if(anura_cl) {
				anura_cl_nbytes_transferred = anura_cl->nbytes_transferred();
				anura_cl_nbytes_total = anura_cl->nbytes_total();
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
			if(anura_cl_nbytes_total < 0 || cl_nbytes_total < 0) {
				sprintf(msg, "%s", get_update_config("message_text_contacting").as_string_default("Updating Game. Contacting server...").c_str());
			} else {
				sprintf(msg, "%s%0.2f/%0.2f%s", get_update_config("message_text_prefix").as_string_default("Updating Game. Transferred ").c_str(), float((cl_nbytes_transferred + anura_cl_nbytes_transferred)/(1024.0*1024.0)), float((anura_cl_nbytes_total + cl_nbytes_total)/(1024.0*1024.0)), get_update_config("message_text_postfix").as_string_default("MB").c_str());
			}

			update_window.set_message(msg);

			const float ratio = (anura_cl_nbytes_total < 0 || cl_nbytes_total < 0) ? 0 : static_cast<float>(cl_nbytes_transferred + anura_cl_nbytes_transferred)/static_cast<float>(std::max<int>(1, (anura_cl_nbytes_total + cl_nbytes_total)));
			update_window.set_progress(ratio);
			update_window.draw();

			SDL_Event event;
			while(SDL_PollEvent(&event)) {
				if(event.type == SDL_QUIT) {
					return true;
				}
			}

			const int target_end = profile::get_tick_time() + 50;
			while(static_cast<int>(profile::get_tick_time()) < target_end && (cl || anura_cl)) {
				if(cl && !cl->process()) {
					if(cl->error().empty() == false) {
						HANDLE_ERROR("Error while updating module: " << cl->error().c_str());
						update_info.add("module_error", variant(cl->error()));
					} else {
						cl_install = cl;
					}

					cl.reset();
				}

				if(anura_cl && !anura_cl->process()) {
					if(anura_cl->error().empty() == false) {
						HANDLE_ERROR("Error while updating anura: " << anura_cl->error().c_str());
						update_info.add("anura_error", variant(anura_cl->error()));
					} else {
						update_info.add("complete_anura", true);
						anura_cl_install = anura_cl;
					}

					anura_cl.reset();
				}
			}
		}

		cl = cl_install;
		anura_cl = anura_cl_install;

		if(cl && cl->is_pending_install()) {
			cl->complete_install();
			if(cl->error().empty() == false) {
				HANDLE_ERROR("Error while installing module: " << cl->error().c_str());
				update_info.add("module_error", variant(cl->error()));
			} else {
				update_info.add("complete_module", true);
			}
		}

		if(anura_cl && anura_cl->is_pending_install()) {
			anura_cl->complete_install();
			if(anura_cl->error().empty() == false) {
				HANDLE_ERROR("Error while installing anura: " << anura_cl->error().c_str());
				update_info.add("anura_error", variant(anura_cl->error()));
			} else {
				update_info.add("complete_anura", true);
			}
		}

		if(has_error && (is_new_install || update_window.proceed_or_retry_dialog(error_msg) == false)) {
			return false;
		}

		}
	}

	if(sys::file_exists("./update/overrides.cfg")) {
		try {
			variant overrides = json::parse_from_file("./update/overrides.cfg", json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
			if(overrides.is_map()) {
				overrides.remove_attr_mutation(variant("arguments"));
				sys::write_file("./update/overrides.cfg", overrides.write_json());
			}
		} catch(...) {
		}
	}

	//remove the update cache since we downloaded and installed everything.
	sys::rmdir_recursive("update-cache");

#ifdef _MSC_VER
	const std::string cl_quotes = "\"";
#else
	const std::string cl_quotes = "";
#endif

	std::string cwd_arg = cl_quotes + "--auto-update-dir=" + sys::get_cwd() + cl_quotes;
	std::string exe_arg = cl_quotes + "--auto-update-exe=" + (g_auto_update_exe.empty() == false ? g_auto_update_exe : g_anura_exe_name) + cl_quotes;

	const std::string working_dir = g_auto_update_install_dir.empty() == false ? g_auto_update_install_dir : preferences::dlc_path() + "/" + real_anura + subdir;
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

	anura_args.push_back(const_cast<char*>(cwd_arg.c_str()));
	anura_args.push_back(const_cast<char*>(exe_arg.c_str()));

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
	int timeout_ms = 10000000;

	auto_update_window update_window;
	std::string error_msg;
	std::deque<std::string> argv(args.begin(), args.end());
	try {
		while(!do_auto_update(argv, update_window, error_msg, timeout_ms)) {
			if(timeout_ms < 10000000) {
				timeout_ms = 10000000;
			}
		}
	} catch(boost::filesystem::filesystem_error& e) {
		ASSERT_LOG(false, "File Error: " << e.what());
	}
}

COMMAND_LINE_UTILITY(window_test)
{
	int flags = 0;
	int width = 800, height = 600;

	SDL_Init(SDL_INIT_VIDEO);

	SDL_DisplayMode dm;
	int res = SDL_GetDesktopDisplayMode(0, &dm);
	if(res != 0) {
		fprintf(stderr, "Failed to query desktop display: %s\n", SDL_GetError());
	} else {
		fprintf(stderr, "Desktop display: %dx%d@%dhz format=%d\n", dm.w, dm.h, dm.refresh_rate, (int)dm.format);
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);

	std::deque<std::string> argv(args.begin(), args.end());
	while(argv.empty() == false) {
		std::string a = argv.front();
		argv.pop_front();
		if(a == "--fullscreen-exclusive") {
			flags = flags | SDL_WINDOW_FULLSCREEN;
		} else if(a == "--fullscreen-desktop") {
			flags = flags | SDL_WINDOW_FULLSCREEN_DESKTOP;
		} else if(a == "--opengl") {
			flags = flags | SDL_WINDOW_OPENGL;
		} else if(a == "--borderless") {
			flags = flags | SDL_WINDOW_BORDERLESS;
		} else if(a == "--highdpi") {
			flags = flags | SDL_WINDOW_ALLOW_HIGHDPI;
		} else if(a == "--gl_major") {
			ASSERT_LOG(argv.empty() == false, "No arg specified");
			std::string w = argv.front();
			argv.pop_front();
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, atoi(w.c_str()));
		} else if(a == "--gl_minor") {
			ASSERT_LOG(argv.empty() == false, "No arg specified");
			std::string w = argv.front();
			argv.pop_front();
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, atoi(w.c_str()));
		} else if(a == "--gl_depth") {
			ASSERT_LOG(argv.empty() == false, "No arg specified");
			std::string w = argv.front();
			argv.pop_front();
			SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, atoi(w.c_str()));
		} else if(a == "--gl_stencil") {
			ASSERT_LOG(argv.empty() == false, "No arg specified");
			std::string w = argv.front();
			argv.pop_front();
			SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, atoi(w.c_str()));
		} else if(a == "--gl_bpp") {
			ASSERT_LOG(argv.empty() == false, "No arg specified");
			std::string w = argv.front();
			argv.pop_front();
			SDL_GL_SetAttribute(SDL_GL_RED_SIZE, atoi(w.c_str()));
			SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, atoi(w.c_str()));
			SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, atoi(w.c_str()));
			SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, atoi(w.c_str()));
		} else if(a == "--gl_multisamplebuffers") {
			ASSERT_LOG(argv.empty() == false, "No arg specified");
			std::string w = argv.front();
			argv.pop_front();
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, atoi(w.c_str()));
		} else if(a == "--width") {
			ASSERT_LOG(argv.empty() == false, "No width specified");
			std::string w = argv.front();
			argv.pop_front();
			width = atoi(w.c_str());
		} else if(a == "--height") {
			ASSERT_LOG(argv.empty() == false, "No height specified");
			std::string w = argv.front();
			argv.pop_front();
			height = atoi(w.c_str());
		} else {
			ASSERT_LOG(false, "Unrecognized arg: " << a);
		}
	}

	SDL_Window* win = SDL_CreateWindow("Anura test window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);
	if(win == nullptr) {
		fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
		return;
	}

	auto context = SDL_GL_CreateContext(win);
	assert(context != nullptr);

	SDL_GL_SetSwapInterval(0);

	int prev = 0;
	for(int i = 0; i != 100000; ++i) {
		glClearColor(i%2 ? 0.05 : 0.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		SDL_GL_SwapWindow(win);
		if(i%100 == 0) {
			int t = SDL_GetTicks();
			fprintf(stderr, "%d -> %fms\n", i, float((t - prev)/100.0f));
			prev = t;
		}
	}
	
	SDL_Delay(1000);
	SDL_DestroyWindow(win);
	SDL_Quit();
}
