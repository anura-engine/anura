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

#include "tbs_internal_client.hpp"
#include "tbs_internal_server.hpp"

namespace tbs
{
	internal_client::internal_client(int session)
		: session_id_(session)
	{
	}

	internal_client::~internal_client()
	{
	}

	void internal_client::send_request(const variant& request, 
		int session_id,
		game_logic::MapFormulaCallablePtr callable, 
		std::function<void(const std::string&)> handler)
	{
		internal_server::send_request(request, session_id, callable, handler);
	}

	void internal_client::process()
	{
		// do nothing
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(internal_client)
		DEFINE_FIELD(in_flight, "int")
			return variant(internal_server::requests_in_flight(obj.session_id_));
	END_DEFINE_CALLABLE(internal_client)
}
