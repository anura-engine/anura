#ifndef DB_CLIENT_HPP_INCLUDED
#define DB_CLIENT_HPP_INCLUDED

#include <string>
#include <functional>

#include <boost/intrusive_ptr.hpp>

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "variant.hpp"

class db_client;
typedef boost::intrusive_ptr<db_client> db_client_ptr;

// Class representing a client to the Anura backend database. Designed to be
// used by server processes. Use USE_DB_CLIENT to compile this functionality in.
class db_client : public game_logic::formula_callable
{
public:
	struct error {
		explicit error(const std::string& msg);
		std::string msg;
	};

	// Create an instance to connect to the database.
	static db_client_ptr create();

	virtual ~db_client();

	// Call this function to process all remaining outstanding operations.
	// If timeout_us is non-zero, it will cause a timeout if completion isn't
	// achieved by this time. You must call process() to make progress on
	// ongoing operations.
	//
	// Returns true iff there are still outstanding operations.
	virtual bool process(int timeout_us=0) = 0;

	enum PUT_OPERATION { PUT_SET, PUT_ADD, PUT_REPLACE };

	// Function to put the given document into the database with the
	// associated key. Will call on_done when it completes or on_error if it
	// encountered an error along the way.
	virtual void put(const std::string& key, variant doc, std::function<void()> on_done, std::function<void()> on_error, PUT_OPERATION op=PUT_SET) = 0;

	// Function to get the given document from the database. Will call on_done
	// with the document on completion (null if no document is found).
	virtual void get(const std::string& key, std::function<void(variant)> on_done, int lock_seconds=0) = 0;

	virtual void remove(const std::string& key) = 0;

private:
	DECLARE_CALLABLE(db_client);
};

#endif
