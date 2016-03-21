/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <boost/filesystem.hpp>

#include "asserts.hpp"
#include "filesystem.hpp"
#include "url_handler.hpp"

namespace xhtml
{
	namespace
	{
		struct file_handler : public url_handler
		{
			explicit file_handler(const std::string& filename)
			{
				createTask(std::launch::deferred, [filename]() -> std::string {
					try {
						return sys::read_file(filename);
					} catch(boost::filesystem::filesystem_error& e) {
						LOG_ERROR("Unable to read file: " << e.what());
						return "";
					}
				});
			}
		};

		typedef std::map<std::string, url_handler::protocol_creator_fn> protocol_map;
		protocol_map& get_protocol_map()
		{
			static protocol_map res;
			return res;
		}
	}

	url_handler::url_handler()
	{
	}

	url_handler::~url_handler()
	{
	}

	url_handler_ptr url_handler::create(const std::string& uri_str)
	{
		// XXX we may need to cache results.
		auto uniform_resource = uri::uri::parse(uri_str);

		if(uniform_resource.protocol().empty()) {
			return std::make_shared<file_handler>(uri_str);
		}

		// search registered handlers for someone that can
		// handle uniform_resource.protocol()
		auto it = get_protocol_map().find(uniform_resource.protocol());
		if(it == get_protocol_map().end()) {
			LOG_ERROR("No handler found for URI protocol: " << uniform_resource.protocol());
			// default to a file lookup
			return std::make_shared<file_handler>(uri_str);
		}
		return nullptr;
	}

	std::string url_handler::getResource()
	{
		//ASSERT_LOG(future_.valid(), "Tried to access a resource which hasn't been scehduled to be created.");	
		// XXX we should probably wait for a timeout here or such if the result isn't immediately available.
		future_.wait();
		return future_.get();
	}

	void url_handler::createTask(std::launch policy, std::function<std::string()> fn)
	{
		future_ = std::async(policy, fn).share();
	}

	void url_handler::registerHandler(const std::string& protocol, url_handler::protocol_creator_fn creator_fn)
	{
		get_protocol_map()[protocol] = creator_fn;
	}
}
