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

#pragma once

#include <string>
#include <functional>

#include "intrusive_ptr.hpp"

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "variant.hpp"

class DbClient;
typedef ffl::IntrusivePtr<DbClient> DbClientPtr;

// Class representing a client to the Anura backend database. Designed to be
// used by server processes. Use USE_DBCLIENT to compile this functionality in.
class DbClient : public game_logic::FormulaCallable
{
public:
	struct error {
		explicit error(const std::string& msg);
		std::string msg;
	};

	// Create an instance to connect to the database.
	static DbClientPtr create(const char* prefix_override=nullptr);

	virtual ~DbClient();

	// Call this function to process all remaining outstanding operations.
	// If timeout_us is non-zero, it will cause a timeout if completion isn't
	// achieved by this time. You must call process() to make progress on
	// ongoing operations.
	//
	// Returns true iff there are still outstanding operations.
	virtual bool process(int timeout_us=0) = 0;

	enum PUT_OPERATION { PUT_SET, PUT_ADD, PUT_REPLACE, PUT_APPEND };
	enum GET_OPERATION { GET_NORMAL, GET_LIST, GET_RAW };

	// Function to put the given document into the database with the
	// associated key. Will call on_done when it completes or on_error if it
	// encountered an error along the way.
	virtual void put(const std::string& key, variant doc, std::function<void()> on_done, std::function<void()> on_error, PUT_OPERATION op=PUT_SET) = 0;

	// Function to get the given document from the database. Will call on_done
	// with the document on completion (null if no document is found).
	virtual void get(const std::string& key, std::function<void(variant)> on_done, int lock_seconds=0, GET_OPERATION op=GET_NORMAL) = 0;

	virtual void remove(const std::string& key) = 0;

	virtual void getKeysWithPrefix(const std::string& key, std::function<void(std::vector<variant>)> on_done) = 0;

private:
	DECLARE_CALLABLE(DbClient);
};
