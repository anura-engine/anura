/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <boost/bind.hpp>

#include "tbs_internal_client.hpp"
#include "tbs_internal_server.hpp"

namespace tbs
{

namespace {
//make request returns go through this function which takes a shared_ptr to the
//actual function we want to call. That way if the internal_client is destroyed
//and we want to cancel getting a reply, we can reset the real function so
//it won't be called.
void handler_proxy(boost::shared_ptr<boost::function<void(const std::string&)> > fn, const std::string& s) {
	(*fn)(s);
}
}

	internal_client::internal_client(int session)
		: session_id_(session)
	{
	}

	internal_client::~internal_client()
	{
		if(handler_) {
			*handler_ = [](const std::string& s){};
		}
	}

	void internal_client::send_request(const variant& request, 
		int session_id,
		game_logic::map_formula_callable_ptr callable, 
		boost::function<void(const std::string&)> handler)
	{
		if(handler_) {
			*handler_ = [](const std::string& s){};
		}

		handler_.reset(new boost::function<void(const std::string&)>(handler));

		boost::function<void(const std::string&)> fn = boost::bind(handler_proxy, handler_, _1);

		internal_server::send_request(request, session_id, callable, fn);
	}

	void internal_client::process()
	{
		// do nothing
	}

	variant internal_client::get_value(const std::string& key) const
	{
		if(key == "in_flight") {
			return variant(internal_server::requests_in_flight(session_id_));
		}

		return variant();
	}

}
