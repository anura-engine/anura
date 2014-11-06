/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#pragma once
#ifndef TBS_INTERNAL_CLIENT_HPP_INCLUDED
#define TBS_INTERNAL_CLIENT_HPP_INCLUDED

#include <boost/function.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include "formula_callable.hpp"
#include "variant.hpp"

namespace tbs
{
	class internal_client : public game_logic::formula_callable
	{
	public:
		internal_client(int session=-1);
		virtual ~internal_client();
		virtual void send_request(const variant& request, 
			int session_id,
			game_logic::map_formula_callable_ptr callable, 
			boost::function<void(const std::string&)> handler);
		void process();
		int session_id() const { return session_id_; }
	protected:
		variant get_value(const std::string& key) const;
	private:
		int session_id_;

		boost::shared_ptr<boost::function<void(const std::string&)> > handler_;
	};

	typedef boost::intrusive_ptr<internal_client> internal_client_ptr;
	typedef boost::intrusive_ptr<const internal_client> const_internal_client_ptr;
}

#endif
