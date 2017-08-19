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

#pragma once

#include <functional>
#include <future>
#include <memory>
#include <string>

#include "uri.hpp"

namespace xhtml
{
	class url_handler;
	typedef std::shared_ptr<url_handler> url_handler_ptr;

	class url_handler
	{
	public:
		url_handler();
		virtual ~url_handler();
		std::string getResource();
		static url_handler_ptr create(const std::string& uri);
		typedef std::function<url_handler_ptr(const uri::uri&)> protocol_creator_fn;
		static void registerHandler(const std::string& protocol, protocol_creator_fn creator_fn);
	protected:
		void createTask(std::launch policy, std::function<std::string()> fn);
	private:
		std::shared_future<std::string> future_;
	};

	struct url_handler_registrar
	{
		explicit url_handler_registrar(const std::string& protocol, url_handler::protocol_creator_fn creator_fn) 
		{
			url_handler::registerHandler(protocol, creator_fn);
		}
	};
}
