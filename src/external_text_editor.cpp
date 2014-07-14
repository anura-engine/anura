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

#ifndef NO_EDITOR

#include "custom_object_type.hpp"
#include "external_text_editor.hpp"
#include "formatter.hpp"
#include "json_parser.hpp"
#include "profile_timer.hpp"
#include "thread.hpp"

#include <cstdlib>

#ifdef _WINDOWS
// Windows defines popen with an underscore, for reasons that seem "difficult" to fathom.
#define popen _popen
#endif

namespace 
{
	std::string normalize_fname(std::string fname)
	{
		while(strstr(fname.c_str(), "//")) {
			fname.erase(fname.begin() + (strstr(fname.c_str(), "//") - fname.c_str()));
		}

		return fname;
	}

	class ViEditor : public ExternalTextEditor
	{
		std::string cmd_;
		int counter_;

		std::map<std::string, std::string> files_;
		std::map<std::string, std::string> file_contents_;
		std::string active_file_;

		//vim servers that we've already inspected in the past, and don't
		//need to do so again.
		std::set<std::string> known_servers_;

		threading::mutex mutex_;
		std::unique_ptr<threading::thread> thread_;

		bool shutdown_;

		void refreshEditorList()
		{
			const int begin = profile::get_tick_time();
			const std::string cmd = "gvim --serverlist";
			FILE* p = popen(cmd.c_str(), "r");
			if(p) {
				std::vector<std::string> servers;

				char buf[1024];
				while(fgets(buf, sizeof(buf), p)) {
					std::string s(buf);
					if(s[s.size()-1] == '\n') {
						s.resize(s.size()-1);
					}

					servers.push_back(s);
				}

				fclose(p);

				{
					threading::lock l(mutex_);
					for(std::map<std::string, std::string>::iterator i = files_.begin(); i != files_.end(); ) {
						if(!std::count(servers.begin(), servers.end(), i->second)) {
							files_.erase(i++);
						} else {
							++i;
						}
					}
				}

				for(const std::string& server : servers) {
					{
						threading::lock l(mutex_);
						if(known_servers_.count(server)) {
							continue;
						}
	
						known_servers_.insert(server);
					}

					const std::string cmd = "gvim --servername " + server + " --remote-expr 'simplify(bufname(1))'";
					FILE* p = popen(cmd.c_str(), "r");
					if(p) {
						char buf[1024];
						if(fgets(buf, sizeof(buf), p)) {
							std::string s(buf);
							if(s[s.size()-1] == '\n') {
								s.resize(s.size()-1);
							}

							if(s.size() > 4 && std::string(s.end()-4,s.end()) == ".cfg") {
								threading::lock l(mutex_);
								files_[s] = server;
								std::cerr << "VIM LOADED FILE: " << s << " -> " << server << "\n";
							}
						}

						fclose(p);
					}
				}
			}
		}

		bool getFileContents(const std::string& server, std::string* data)
		{
			const std::string cmd = "gvim --servername " + server + " --remote-expr 'getbufline(1, 1, 1000000)'";

			FILE* p = popen(cmd.c_str(), "r");
			if(p) {
				std::vector<char> buf;
				buf.resize(10000000);
				size_t nbytes = fread(&buf[0], 1, buf.size(), p);
				fclose(p);
				if(nbytes > 0 && nbytes <= buf.size()) {
					buf.resize(nbytes);
					buf.push_back(0);
					*data = buf.empty() ? NULL : &buf.front();
					return true;
				}
			}

			return false;
		}

		void runThread()
		{
			for(int tick = 0; !shutdown_; ++tick) {
				profile::delay(60);

				std::map<std::string, std::string> files;
				{
					threading::lock l(mutex_);
					if(tick%10 == 0) {
						refreshEditorList();
						files = files_;
					} else if(files_.count(active_file_)) {
						files[active_file_] = files_[active_file_];
					}
				}

				std::map<std::string, std::string> results;
				std::set<std::string> remove_files;

				typedef std::pair<std::string,std::string> str_pair;
				for(const str_pair& item : files) {
					std::string contents;
					if(getFileContents(item.second, &contents)) {
						results[item.first] = contents;
					} else {
						remove_files.insert(item.first);
					}
				}

				{
					threading::lock l(mutex_);
					for(const std::string& fname : remove_files) {
						files_.erase(fname);
					}

					for(const str_pair& item : results) {
						if(file_contents_[item.first] != item.second) {
							std::cerr << "CONTENTS OF " << item.first << " UPDATED..\n";
							file_contents_[item.first] = item.second;
							active_file_ = item.first;
						}
					}
				}
			}
		}

	public:
		explicit ViEditor(variant obj) : cmd_(obj["command"].as_string_default("gvim")), counter_(0), shutdown_(false)
		{
			thread_.reset(new threading::thread("vi_editor_thread", std::bind(&ViEditor::runThread, this)));
		}

		void shutdown()
		{
			{
				threading::lock l(mutex_);
				shutdown_ = true;
			}

			thread_.reset();
		}

		void loadFile(const std::string& fname_input)
		{
			const std::string fname = normalize_fname(fname_input);

			std::string existing_instance;
			{
				threading::lock l(mutex_);

				if(files_.count(fname)) {
					existing_instance = files_[fname];
				}
			}

			if(existing_instance.empty() == false) {
				const std::string command = "gvim --servername " + existing_instance + " --remote-expr 'foreground()'";
				const int result = system(command.c_str());
				return;
			}

			std::string server_name = formatter() << "S" << counter_;
			{
				threading::lock l(mutex_);
				while(known_servers_.count(server_name)) {
					++counter_;
					server_name = formatter() << "S" << counter_;
				}
			}

			const std::string command = cmd_ + " --servername " + server_name + " " + fname;
			const int result = system(command.c_str());

			threading::lock l(mutex_);
			files_[fname] = server_name;
			++counter_;
		}

		std::string getFileContents(const std::string& fname_input) override
		{
			const std::string fname = normalize_fname(fname_input);

			threading::lock l(mutex_);
			std::map<std::string, std::string>::const_iterator itor = file_contents_.find(fname);
			if(itor != file_contents_.end()) {
				return itor->second;
			} else {
				return "";
			}
		}

		int getLine(const std::string& fname_input) const override
		{
			const std::string fname = normalize_fname(fname_input);
			return 0;
		}

		std::vector<std::string> getLoadedFiles() const override
		{
			threading::lock l(mutex_);
			std::vector<std::string> result;
			for(std::map<std::string,std::string>::const_iterator i = files_.begin(); i != files_.end(); ++i) {
				result.push_back(i->first);
			}
			return result;
		}
	};

	std::set<ExternalTextEditor*> all_editors;
}

ExternalTextEditor::Manager::Manager()
{
}

ExternalTextEditor::Manager::~Manager()
{
	for(ExternalTextEditor* e : all_editors) {
		e->shutdown();
	}
}


ExternalTextEditorPtr ExternalTextEditor::create(variant key)
{
	const std::string type = key["type"].as_string();
	ExternalTextEditorPtr result;
	if(type == "vi") {
		static ExternalTextEditorPtr ptr(new ViEditor(key));
		result = ptr;
	}

	if(result) {
		result->replace_in_game_editor_ = key["replace_in_game_editor"].as_bool(true);
	}

	return result;
}

ExternalTextEditor::ExternalTextEditor()
  : replace_in_game_editor_(true)
{
	all_editors.insert(this);
}

ExternalTextEditor::~ExternalTextEditor()
{
	all_editors.erase(this);
}

void ExternalTextEditor::process()
{
	std::vector<std::string> files = getLoadedFiles();
	if(files.empty() == false) {
		const int begin = profile::get_tick_time();
		for(const std::string& fname : files) {
			const std::string contents = getFileContents(fname);
			if(contents != json::get_file_contents(fname)) {
				try {
					CustomObjectType::setFileContents(fname, contents);
				} catch(...) {
				}
			}
		}
	}
}

#endif // NO_EDITOR
